"""anim_capture_move: capture a COMBAT move (base + variant in ONE editor session) behind a hard pre-capture gate.

The gate costs seconds; an editor capture costs a session. It refuses to capture unless:
  (a) the last run_intent report of the variant is PASS for the same intent hash (follow-through, conservation, gates),
  (b) a dry_run with the same intent hash exists,
  (c) the hitbox sync file (Saved/AnimR2/*hitboxes*.json, {"timeline":..., "clips": {"/Game/..": sha256}}) has the sha256
      of the clip .uasset as it is now (else: rerun the hitbox script),
  (d) the edit is readable: peak change of hand/foot/head position >= min_move_cm OR of a limb direction >= min_rot_deg
      inside the edited window (else FAIL "too small to read"),
  (e) the objective has fewer than 2 capture attempts (the 3rd is refused).
Then it builds PIE_CAPTURE_RUNS (base + variant, per-run cvars), the pie_capture_timed.ps1 command line, reuses cached
captures (base: same clip sha + sequence + cvars + views + build; variant: same intent hash) and writes the review pack
(docs/notes/reviews/<move>/<round>/: mp4 real + slow4x and ONE grid per view, base row over variant row, same frames).
The PIE capture itself runs only through `runner` (pwsh): offline tests inject a fake one.

skill-gaps (2026-11-23, after the dropkick K7 test): poses authored from a CARD (animpipe author/build/check, no anim_run_intent) pass
(a)+(b) through card_report(): a variant run with "card" gets its report from the card's `animpipe check` JSON (verdict PASS, newer
than the card and the build), keyed by the card's content hash. Every run may name its own "kit" (base and variant kits in one
session); the gate report is written to <out_dir>/gate_report.json pass or fail; grids and videos are montage-aligned per run.
Hitboxes are drawn only for "hitbox_view" runs, in separate videos under <out_dir>/hitbox/; the review grids/videos stay clean.

Fast capture (owner 2026-11-24, 1A + 2A):
- fast=True (default) runs the capture through the C++ engine (plugin GSTNCapture, PIE_FASTCAP=1): async readback, only the frames
  around the press reach the disk (fast_window = [frames before, frames after], hitstop frames added on hit runs), simple light on the
  review views, early end of a sequence once its window and asserts are logged;
- the cache key is CONTENT based (C++ source hash, not mtimes: a branch switch no longer invalidates the base) and carries
  CAPTURE_CHAIN: the base (e.g. an approved, locked clip) is captured once per chain and reused every round after;
- hitbox_view runs are captured only when final_round=True (the owner's check, once, on the round that goes to the owner);
- hitstop_pairs=[[whiff_seq, hit_seq], ...] (per move, only where the snap matters): the hit video is extended by the frames the
  attacker spent in hitstop, and <run>_<whiff>_vs_<hit>_{real,slow4x}.mp4 shows both side by side, aligned on the press.
"""
from __future__ import annotations

import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Callable

_HERE = str(Path(__file__).resolve().parent)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import manifest  # noqa: E402
import no_stop_gate  # noqa: E402
import review_pack  # noqa: E402

TIMED_PS1 = "Tools/GASNPPTest/pie_capture_timed.ps1"
GRID_PY = "Tools/GASNPPTest/pie_capture_grid.py"
DEFAULT_VIEWS = "az90,az180,game"
POS_KEYS = ["hand_MainPoint_l", "hand_MainPoint_r", "foot_MainPoint_l", "foot_MainPoint_r", "head"]
# (parent, child) segments whose direction change is the "degrees" part of the readability check
SEGMENTS = [("upperarm_MainPoint_l", "lowerarm_MainPoint_l"), ("lowerarm_MainPoint_l", "hand_MainPoint_l"),
            ("upperarm_MainPoint_r", "lowerarm_MainPoint_r"), ("lowerarm_MainPoint_r", "hand_MainPoint_r"),
            ("thigh_MainPoint_l", "calf_MainPoint_l"), ("calf_MainPoint_l", "foot_MainPoint_l"),
            ("thigh_MainPoint_r", "calf_MainPoint_r"), ("calf_MainPoint_r", "foot_MainPoint_r"),
            ("neck_01", "head")]

Runner = Callable[[list, dict, str], Any]
# Blocks that are never inherited from an approved base (skill audit 2026-11-23): a stop is judged again on every variant, the
# approved version stays locked and the fix goes in a variant (K6 settle, still ~6 frames, was inherited and never re-judged)
NON_INHERITABLE = ("stop_and_go", "hold_reason")
# Version of the capture chain (driver + engine + light). Part of the cache key: a cached capture from another chain is never mixed with a
# new one in the same A/B (rule 10). Bump it when the pictures change for the same clip (light, size, camera framing, engine).
CAPTURE_CHAIN = "fastcap-1"
FAST_WINDOW = (20, 90)


def _repo(root: str | Path | None) -> Path:
    return Path(root).resolve() if root else Path(__file__).resolve().parents[3]


def _name(asset: str) -> str:
    return asset.rsplit("/", 1)[-1]


def _sha(path: Path) -> str:
    import hashlib
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else ""


def clip_uasset(root: Path, clip: str) -> Path | None:
    """/Game/A/B/X -> Content/A/B/X.uasset; falls back to a name search under Content/."""
    if clip.startswith("/Game/"):
        p = root / "Content" / (clip[len("/Game/"):] + ".uasset")
        if p.exists():
            return p
    hits = sorted((root / "Content").rglob(f"{_name(clip)}.uasset")) if (root / "Content").exists() else []
    return hits[0] if hits else None


# --------------------------------------------------------------------------------------------------------------- gate
def _joints(root: Path, asset: str) -> dict[str, list]:
    p = root / "Saved" / "AnimPipeline" / "runstart" / f"joints_{_name(asset)}.json"
    if not p.exists():
        return {}
    return json.loads(p.read_text(encoding="utf-8")).get("joints", {})


def _angle_deg(u: list[float], v: list[float]) -> float:
    nu, nv = math.sqrt(sum(x * x for x in u)), math.sqrt(sum(x * x for x in v))
    if nu < 1e-9 or nv < 1e-9:
        return 0.0
    c = max(-1.0, min(1.0, sum(u[i] * v[i] for i in range(3)) / (nu * nv)))
    return math.degrees(math.acos(c))


def measure_change(var: dict[str, list], base: dict[str, list], window: tuple[int, int] | None = None) -> dict[str, Any]:
    """Peak change vs base inside the window (clip-local frames): hand/foot/head position (cm) and limb direction (deg)."""
    n = min((len(v) for v in list(var.values()) + list(base.values())), default=0)
    if n == 0:
        return {"ok": False, "error": "empty joints"}
    a, b = (0, n - 1) if window is None else (max(0, int(window[0])), min(n - 1, int(window[1])))
    best_cm, best_deg = (0.0, None, None), (0.0, None, None)
    for k in POS_KEYS:
        if k in var and k in base:
            for f in range(a, b + 1):
                d = math.dist(var[k][f], base[k][f])
                if d > best_cm[0]:
                    best_cm = (d, k, f)
    for p, c in SEGMENTS:
        if all(k in var and k in base for k in (p, c)):
            for f in range(a, b + 1):
                uv = [var[c][f][i] - var[p][f][i] for i in range(3)]
                ub = [base[c][f][i] - base[p][f][i] for i in range(3)]
                d = _angle_deg(uv, ub)
                if d > best_deg[0]:
                    best_deg = (d, f"{p}>{c}", f)
    return {"ok": True, "window": [a, b], "peak_cm": round(best_cm[0], 2), "peak_cm_joint": best_cm[1], "peak_cm_frame": best_cm[2],
            "peak_deg": round(best_deg[0], 2), "peak_deg_segment": best_deg[1], "peak_deg_frame": best_deg[2]}


def find_hitbox_files(root: Path) -> list[Path]:
    d = root / "Saved" / "AnimR2"
    return sorted(d.glob("*hitboxes*.json")) if d.exists() else []


def check_hitbox_sync(root: Path, clips: list[str]) -> tuple[bool, str]:
    files = find_hitbox_files(root)
    maps: list[dict[str, str]] = []
    for f in files:
        try:
            maps.append(json.loads(f.read_text(encoding="utf-8")).get("clips", {}))
        except (OSError, ValueError):
            continue
    for clip in clips:
        ua = clip_uasset(root, clip)
        if ua is None:
            return False, f"clip {clip} not found under Content/"
        now = _sha(ua)
        recorded = [m[k] for m in maps for k in m if k == clip or _name(k) == _name(clip)]
        if not recorded:
            return False, f"no Saved/AnimR2/*hitboxes*.json names {_name(clip)}: rerun the hitbox script"
        if now not in recorded:
            return False, f"hitbox sync is stale for {_name(clip)} (clip changed since the hitbox script ran): rerun the hitbox script"
    return True, "ok"


def precapture_gate(root: str | Path | None, asset: str, base_asset: str | None = None, ihash: str | None = None,
                    clips: list[str] | None = None, edit_window: tuple[int, int] | None = None, objective: str = "",
                    hitbox_required: bool = True, min_move_cm: float = 2.0, min_rot_deg: float = 3.0,
                    max_attempts: int = manifest.MAX_ATTEMPTS, no_stop: dict[str, Any] | None = None,
                    base_clips: list[str] | None = None) -> dict[str, Any]:
    """Run every gate and report all of them; ok only if all pass. Pure and offline (reads manifest, joints, uassets).

    no_stop (skill audit 2026-11-23, option A): every clip in `clips` (startup, loop, settle, landing: list them all) passes
    no_stop_gate.check on its joints dump. Options {"stop_cm", "max_ratio", "still_run", "holds": {clip name: "a-b,.."},
    "hold_reasons": {clip name: "why the owner wants it still"} (required for every clip with holds), "startup": {clip name: "a-b"}}. A clip also in `base_clips` is INHERITED: judged again and blocking all the same (the
    approved version stays locked, the fix goes in a variant); the line says "ereditato". Always blocking (owner 2026-11-24: no
    quality cut, a false alarm is fixed in the measure, never silenced): there is no option to turn it off."""
    repo = _repo(root)
    checks: list[dict[str, Any]] = []

    def add(name: str, ok: bool, msg: str, **extra: Any) -> None:
        checks.append({"check": name, "ok": ok, "msg": msg, **extra})

    m = manifest.load(repo, asset)
    ok_a, msg_a = manifest.report_is_pass(m.get("last_report"), ihash)
    add("report_pass", ok_a, msg_a)
    ok_b = bool(ihash) and ihash in m.get("dry_runs", [])
    add("dry_run_exists", ok_b, "ok" if ok_b else f"no dry_run with intent hash {ihash}: run anim_run_intent dry_run=true first")
    if hitbox_required:
        ok_c, msg_c = check_hitbox_sync(repo, list(clips or [asset]))
        add("hitbox_fresh", ok_c, msg_c)
    if base_asset:
        vj, bj = _joints(repo, asset), _joints(repo, base_asset)
        if not vj or not bj:
            add("readable", False, f"joints dump missing for {asset if not vj else base_asset}: cannot measure the change")
        else:
            ch = measure_change(vj, bj, edit_window)
            ok_d = ch["ok"] and (ch["peak_cm"] >= min_move_cm or ch["peak_deg"] >= min_rot_deg)
            add("readable", ok_d, "ok" if ok_d else
                f"too small to read: peak {ch.get('peak_cm')} cm / {ch.get('peak_deg')} deg < {min_move_cm} cm / {min_rot_deg} deg",
                **{k: v for k, v in ch.items() if k != "ok"})
    else:
        add("readable", False, "base_asset missing: cannot measure the change")
    ns = dict(no_stop or {})
    bnames = {_name(b) for b in (base_clips or [])}
    bad, lines = [], []
    for clip in (clips or [asset]):
        nm = _name(clip)
        j = _joints(repo, clip)
        if not j:
            bad.append(nm)
            lines.append(f"{nm}: joints dump missing (clip_joints_dump_ue.py), cannot measure stops")
            continue
        if (ns.get("holds") or {}).get(nm) and not str((ns.get("hold_reasons") or {}).get(nm, "")).strip():
            # owner 2026-11-24: a stop is a defect by default; a hold is accepted only as intended motion, exceptional and
            # declared WITH its reason. A hold without a written reason does not count as declared (and the review still judges
            # every declared hold on the video: INV-12)
            bad.append(nm)
            lines.append(f"{nm}: hold {ns['holds'][nm]} declared without a reason: add no_stop.hold_reasons {{\"{nm}\": \"why\"}}")
            continue
        hs = (ns.get("holds") or {}).get(nm)
        if hs and isinstance(hs, str) and ":" not in hs:   # life.py reads "a-b:why": attach the clip's written reason to each range
            why = str((ns.get("hold_reasons") or {}).get(nm, "")).replace(",", ";").strip()
            hs = ",".join(f"{x.strip()}:{why}" for x in hs.split(",") if x.strip())
        r = no_stop_gate.check(j, hs, (ns.get("startup") or {}).get(nm),
                               float(ns.get("stop_cm", no_stop_gate.DEFAULTS["stop_cm"])),
                               float(ns.get("max_ratio", no_stop_gate.DEFAULTS["max_ratio"])),
                               int(ns.get("still_run", no_stop_gate.DEFAULTS["still_run"])))
        if not r["ok"]:
            bad.append(nm)
            lines.append(f"{nm}{' (ereditato dalla base: rigiudicato, blocca; la versione approvata resta bloccata, correggi in una variante)' if nm in bnames else ''}: "
                         + "; ".join(r["lines"]))
    add("no_stop", not bad, "ok" if not bad else " | ".join(lines), clips_failed=bad)
    n = int(m.get("attempts", {}).get(objective, 0))
    add("attempts", n < max_attempts, f"{n}/{max_attempts} attempts used" if n < max_attempts else
        f"attempt {n + 1} refused: max {max_attempts} per objective {objective!r}; stop and report")
    failed = [c for c in checks if not c["ok"]]
    return {"ok": not failed, "asset": _name(asset), "intent_hash": ihash, "checks": checks,
            "error": "; ".join(c["msg"] for c in failed) if failed else None}


# ------------------------------------------------------------------------------------------------------------ card path
def card_hash(card: Path) -> str:
    return _sha(Path(card))[:16]


def _check_json(repo: Path, card: str | Path, check_json: str | Path | None = None) -> tuple[Path, dict[str, Any]]:
    cp = Path(card) if Path(card).is_absolute() else repo / card
    move = json.loads(cp.read_text(encoding="utf-8")).get("move", cp.stem) if cp.exists() else cp.stem
    chk = Path(check_json) if check_json else repo / "Saved" / "AnimPipeline" / "animpipe" / f"{move}_check.json"
    return chk, (json.loads(chk.read_text(encoding="utf-8")) if chk.exists() else {})


def inherited_blocks(repo: Path, failing: dict[str, str], base_asset: str | None, base_card: str | Path | None,
                     base_check_json: str | Path | None = None) -> tuple[dict[str, str], str]:
    """Which failing blocks also fail, with the SAME line, on the approved base. INFORMATION ONLY: an inherited block still blocks.
    History: skill-k8 (btrk7b, 2026-11-23) made these blocks non-blocking after K8 r1 failed legibility, contact_faf and hitstop_snap
    exactly like the approved K7. Owner rule 2026-11-24 (scope: whole animation skill and review): no compromise on false positives,
    a check that errs is FIXED, never softened. So a block failing on the base too has only two ways out:
      a) the check measures wrong: fix the measure in animpipe/checks.py, recalibrate on the approved version with proof (handover);
      b) the base really has that defect and the owner accepts it: an exemption row in docs/notes/anim-approvate.md written with the
         owner's words, per base, block and exact failing line (owner_exemptions below).
    Without one of the two it blocks. stop_and_go (NON_INHERITABLE) is never even reported as inherited.
    Returns ({block: base line}, reason)."""
    if not failing or not base_asset or not (base_card or base_check_json):
        return {}, "no approved base card to compare with"
    appr = repo / "docs" / "notes" / "anim-approvate.md"
    if not appr.exists() or _name(base_asset) not in appr.read_text(encoding="utf-8"):
        return {}, f"base {_name(base_asset)} is not in docs/notes/anim-approvate.md"
    _, brep = _check_json(repo, base_card or "", base_check_json)
    if not brep:
        return {}, "no animpipe check for the approved base card: run animpipe check on it"
    bblk = brep.get("block") or {}
    inh = {k: bblk[k].get("line", "") for k, line in failing.items()
           if k not in NON_INHERITABLE and k in bblk and not bblk[k].get("ok") and bblk[k].get("line", "") == line}
    return inh, "ok"


EXEMPT_HEADING = "## Esenzioni dei controlli"


def owner_exemptions(repo: Path, base_asset: str | None) -> dict[str, list[str]]:
    """Owner-written exemptions for an approved base, read from docs/notes/anim-approvate.md under EXEMPT_HEADING. Row format:
    | <base asset> | <block> | <exact failing line of animpipe check> | owner <date>: "<owner's words>" |
    A row without the owner's quoted words (a "\"" pair) is ignored: an exemption is the owner's decision, never a thread's.
    The line must match the variant's failing line exactly, so the exemption covers those frames and numbers only: if the variant
    changes them (worse or different), it blocks again. stop_and_go is never exempted here (NON_INHERITABLE)."""
    appr = repo / "docs" / "notes" / "anim-approvate.md"
    if not base_asset or not appr.exists():
        return {}
    txt = appr.read_text(encoding="utf-8")
    if EXEMPT_HEADING not in txt:
        return {}
    out: dict[str, list[str]] = {}
    for row in txt.split(EXEMPT_HEADING, 1)[1].splitlines():
        if row.startswith("## "):
            break
        cells = [c.strip() for c in row.strip().strip("|").split("|")]
        if len(cells) < 4 or cells[0] != _name(base_asset) or cells[1] in NON_INHERITABLE:
            continue
        if "owner" not in cells[3].lower() or cells[3].count('"') < 2:
            continue
        out.setdefault(cells[1], []).append(cells[2].strip("`"))
    return out


def card_report(root: str | Path | None, asset: str, card: str | Path, check_json: str | Path | None = None,
                build_json: str | Path | None = None, base_asset: str | None = None, base_card: str | Path | None = None,
                base_check_json: str | Path | None = None) -> dict[str, Any]:
    """Report for a variant whose poses come from a card (no anim_run_intent): reads `animpipe check <card>`'s JSON and, when it is a
    PASS of THIS card, records it in the manifest as the variant's run report + dry run under the card's content hash, so the
    capture gate checks card poses with the same (a)/(b) rules. Refused (reported, nothing recorded) when the card names another
    clip, the check is missing / FAIL, or the check is older than the card or the last build (stale)."""
    repo = _repo(root)
    cp = Path(card) if Path(card).is_absolute() else repo / card
    checks: list[dict[str, Any]] = []
    if not cp.exists():
        return {"ok": False, "error": f"card not found: {cp}", "checks": []}
    c = json.loads(cp.read_text(encoding="utf-8"))
    move = c.get("move", cp.stem)
    anim = _name(c.get("anim", ""))
    ok_n = anim == _name(asset)
    checks.append({"check": "card_names_asset", "ok": ok_n, "msg": "ok" if ok_n else f"card anim {anim} != asset {_name(asset)}"})
    out = repo / "Saved" / "AnimPipeline" / "animpipe"
    chk = Path(check_json) if check_json else out / f"{move}_check.json"
    bld = Path(build_json) if build_json else out / f"{move}_build.json"
    rep: dict[str, Any] = {}
    if chk.exists():
        rep = json.loads(chk.read_text(encoding="utf-8"))
    blocks = {k: bool(v.get("ok")) for k, v in (rep.get("block") or {}).items()}
    failing = {k: v.get("line", "") for k, v in (rep.get("block") or {}).items() if not v.get("ok")}
    inh, why = inherited_blocks(repo, failing, base_asset, base_card, base_check_json)
    ex = owner_exemptions(repo, base_asset)
    exempted = {k: line for k, line in failing.items() if line and line in ex.get(k, [])}
    still = [k for k in failing if k not in exempted]          # inherited blocks stay here: they block (owner rule 2026-11-24)
    ok_c = rep.get("verdict") == "PASS" or (bool(rep) and not still)
    inh_note = (f"; also failing identically on the approved base (still blocking: fix the check with proof, or the owner writes an "
                f"exemption in anim-approvate.md): {', '.join(k for k in inh if k in still)}" if any(k in still for k in inh)
                else f"; base comparison: {why}" if why != "ok" else "")
    checks.append({"check": "animpipe_check_pass", "ok": ok_c, "msg": "ok" + (f" (owner exemptions: {', '.join(exempted)})"
                   if exempted else "") if ok_c else
                   (f"no animpipe check for {move} ({chk}): run animpipe check" if not rep else
                    f"animpipe check is {rep.get('verdict')} (blocking: {', '.join(still) or '?'}" + inh_note + ")"),
                   "blocks": blocks, "inherited": inh, "exempted": exempted})
    stale = [n for n, f in (("card", cp), ("build", bld)) if chk.exists() and f.exists() and f.stat().st_mtime > chk.stat().st_mtime]
    checks.append({"check": "check_fresh", "ok": not stale and chk.exists(),
                   "msg": "ok" if not stale and chk.exists() else f"check older than the {', '.join(stale) or 'card'}: rerun animpipe build + check"})
    ok = all(x["ok"] for x in checks)
    ihash = card_hash(cp)
    if ok:
        manifest.record_dry_run(repo, asset, ihash)
        manifest.record_report(repo, asset, ihash, {"ok": True, "verdict": "PASS", "gates": {"status": "PASS"}})
        m = manifest.load(repo, asset)
        m["last_report"].update({"source": "card", "card": str(cp.relative_to(repo)) if cp.is_relative_to(repo) else str(cp),
                                 "check_json": str(chk), "blocks": blocks, "inherited": inh,
                                 "exempted": exempted})
        manifest.save(repo, asset, m)
    return {"ok": ok, "source": "card", "asset": _name(asset), "intent_hash": ihash, "checks": checks, "inherited": inh,
            "exempted": exempted, "error": None if ok else "; ".join(x["msg"] for x in checks if not x["ok"])}


# ------------------------------------------------------------------------------------------------------------- capture
HB_CVAR = "gstn.Combat.Debug.Hitboxes"


def hitbox_cvar_errors(runs: list[dict[str, Any]]) -> list[str]:
    """skill-k8 (owner 2026-11-23: hitboxes never in the animation-review videos, only in the separate hitbox video). The capture never
    undoes a cvar and the whiffs run before the hits (pie_capture_move), so a hitbox run can sit right before a review run: EVERY run must
    set the cvar itself, 0 for review runs and 1 for "hitbox_view" runs (K8 r1: the hitbox videos came out with no shapes)."""
    errs = []
    for r in runs:
        vals = [c.split()[-1] for c in str(r.get("cvars", "")).split(";") if c.strip().startswith(HB_CVAR) and len(c.split()) > 1]
        want = "1" if r.get("hitbox_view") else "0"
        if not vals or vals[-1] != want:
            errs.append(f"run {r.get('name')}: cvars must end with '{HB_CVAR} {want}' "
                        f"({'hitbox video' if want == '1' else 'review run: no hitboxes in the review'})")
    return errs


def default_runner(cmd: list, env: dict, cwd: str) -> Any:
    return subprocess.run(cmd, env=env, cwd=cwd, capture_output=True, text=True)


def _rc(res: Any) -> int:
    return int(res.get("returncode", 0)) if isinstance(res, dict) else int(res.returncode)


def _views_list(views: str | list) -> list[str]:
    return [v.strip() for v in (views.split(",") if isinstance(views, str) else views) if v.strip()]


def _run_key(root: Path, run: dict[str, Any], sequences: list[str], views: str, build: str, is_variant: bool,
             chain: str = CAPTURE_CHAIN) -> str:
    shas = [_sha(p) for p in (clip_uasset(root, c) for c in run.get("clips", [])) if p]
    extra = str(run.get("intent_hash") or "") if is_variant else ""
    # the run's kit is part of what is captured (a K7 kit and a K8 kit with the same clip are two captures)
    return manifest.capture_key(shas, sequences, run.get("cvars", ""), views, build,
                                json.dumps([extra, chain, run.get("kit") or ""]))


def _cached_dir(root: Path, run: dict[str, Any], key: str, sequences: list[str]) -> Path | None:
    rec = manifest.load(root, run["asset"]).get("captures", {}).get(key)
    if not rec:
        return None
    d = root / rec["dir"]
    return d if d.is_dir() and all((d / s).is_dir() for s in sequences) else None


def hitstop_frames(run_dir: str | Path, seq: str) -> int:
    """Rendered frames the attacker spent in hitstop in a captured sequence (frames.jsonl: the running timeline's timelinePaused grew)."""
    p = Path(run_dir) / seq / "frames.jsonl"
    if not p.exists():
        return 0
    n, paused, tl0 = 0, None, None
    for line in p.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        c = json.loads(line).get("combat") or {}
        tl = c.get("timeline") if c.get("inTimeline") else None
        now = c.get("timelinePaused", 0)
        if tl is not None and tl == tl0 and paused is not None and now > paused:
            n += 1
        paused, tl0 = (now, tl) if tl is not None else (None, None)
    return n


CAM_JUMP_CM = 6.0


def camera_jumps(run_dir: str | Path, seq: str, limit_cm: float = CAM_JUMP_CM, frames: tuple[int, int] | None = None) -> list[dict[str, Any]]:
    """Camera jumps in a captured sequence (coordinator 2026-11-24, K7/K8 v156-v158): for every view, a frame whose camera step differs
    from the previous step by more than limit_cm (a smooth follow changes its step a little per frame; a jump changes it at once). Reads
    the 'cams' entries of frames.jsonl ({view: [[x, y, z], rot, fov, size]}); only consecutive frames are compared."""
    p = Path(run_dir) / seq / "frames.jsonl"
    if not p.exists():
        return []
    prev: dict[str, tuple[int, list[float], list[float] | None]] = {}
    out = []
    for line in p.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        r = json.loads(line)
        f = int(r.get("f", -1))
        if frames and not frames[0] <= f <= frames[1]:
            continue
        for view, cam in (r.get("cams") or {}).items():
            pos = [float(v) for v in cam[0]]
            last = prev.get(view)
            step = None
            if last and last[0] == f - 1:
                step = [pos[i] - last[1][i] for i in range(3)]
                if last[2] is not None:
                    jump = math.dist(step, last[2])
                    if jump > limit_cm:
                        out.append({"view": view, "frame": f, "cm": round(jump, 1)})
            prev[view] = (f, pos, step)
    return out


def video_commands(repo: Path, runs: list[dict[str, Any]], run_dirs: dict[str, str], sequences: list[str], out_rel: str,
                   cap_win: tuple[int, int], alignment: dict[str, Any], extend: dict[str, dict[str, int]] | None = None) -> list[list[str]]:
    """pie_capture_grid video per run/sequence, real + slow4x; each run's window is shifted by its montage offset (same animation
    frames on the same video frame index as the base). extend[run][seq] adds frames at the end (the hitstop of a hit run)."""
    cmds = []
    for r in runs:
        for s in sequences:
            k = int((alignment.get(s) or {}).get("offsets", {}).get(r["name"], 0))
            x = int(((extend or {}).get(r["name"]) or {}).get(s, 0))
            for tag, fps in (("real", 60), ("slow4x", 15)):
                cmds.append(["python", str(repo / GRID_PY), "video", run_dirs[r["name"]], s,
                             str(repo / out_rel / f"{r['name']}_{s}_{tag}.mp4"), f"--fps={fps}",
                             f"--from={cap_win[0] + k}", f"--to={cap_win[1] + k + x}"])
    return cmds


def hitstop_pair_commands(repo: Path, runs: list[dict[str, Any]], out_rel: str, pairs: list[list[str]]) -> list[list[str]]:
    """ffmpeg hstack of a run's whiff and hit videos (same press frame on the same video index: the hit side freezes in hitstop and
    snaps after it, the whiff side keeps going). The shorter side holds its last picture (tpad)."""
    cmds = []
    for r in runs:
        for whiff, hit in pairs:
            for tag in ("real", "slow4x"):
                a = str(repo / out_rel / f"{r['name']}_{whiff}_{tag}.mp4")
                b = str(repo / out_rel / f"{r['name']}_{hit}_{tag}.mp4")
                cmds.append(["ffmpeg", "-y", "-loglevel", "error", "-i", a, "-i", b, "-filter_complex",
                             "[0:v]tpad=stop_mode=clone:stop_duration=10[a];[1:v]tpad=stop_mode=clone:stop_duration=10[b];"
                             "[a][b]hstack=inputs=2:shortest=0[v]", "-map", "[v]", "-shortest", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                             str(repo / out_rel / f"{r['name']}_{whiff}_vs_{hit}_{tag}.mp4")])
    return cmds


def anim_capture_move(kit: str, sequences: list[str], variant_runs: list[dict[str, Any]], views: str = DEFAULT_VIEWS,
                      out: str = "", assert_spec: dict[str, Any] | None = None, dry_run: bool = False, move: str = "",
                      round: str = "r1", objective: str = "", edit_window: list[int] | None = None,
                      window: list[int] | None = None, anchor: int | None = None, faf: int | None = None,
                      root: str | Path | None = None, runner: Runner | None = None, build: str | None = None,
                      hitbox_required: bool = True, min_move_cm: float = 2.0, min_rot_deg: float = 3.0,
                      raw_dir: str = "", max_images: int = 8, dense_radius: int = 4, fast: bool = True,
                      fast_window: list[int] | None = None, final_round: bool = False,
                      hitstop_pairs: list[list[str]] | None = None,
                      no_stop: dict[str, Any] | None = None, motion: dict[str, Any] | None = None,
                      identity: list[str] | None = None, key_frames: list[int] | None = None) -> dict[str, Any]:
    """variant_runs[0] is the base: {"name","asset","clips":["/Game/.."],"cvars":"a 1;b 2","kit"?}; the others are variants with
    "intent_hash" (hash of anim_run_intent dry run; see manifest.intent_hash) or "card" (card-authored poses, see card_report), and
    an optional "kit" (default: `kit`) and "assert" (default: assert_spec). All runs share ONE Play session. A run with
    "hitbox_view": true (give it cvars gstn.Combat.Debug.Hitboxes 1) is left out of the review grids/videos and gets videos with the
    hitboxes drawn in <out>/hitbox/ (the owner's check, not the animator's)."""
    repo = _repo(root)
    if not sequences:
        return {"ok": False, "error": "sequences is empty"}
    if len(variant_runs) < 2:
        return {"ok": False, "error": "variant_runs needs the base run first and at least one variant"}
    names = [r.get("name") for r in variant_runs]
    if len(set(names)) != len(names) or not all(names) or any(not r.get("asset") for r in variant_runs):
        return {"ok": False, "error": "every run needs a unique name and an asset"}
    move = move or _name(kit)
    out_rel = out or f"docs/notes/reviews/{move}/{round}"
    raw_rel = raw_dir or f"Saved/AnimPipeline/captures/{move}/{round}"
    # hitbox runs are the owner's check: only on the round that goes to the owner (1A, 2026-11-24)
    skipped_hitbox = [r["name"] for r in variant_runs if r.get("hitbox_view") and not final_round]
    variant_runs = [r for r in variant_runs if not (r.get("hitbox_view") and not final_round)]
    pairs = [list(p) for p in (hitstop_pairs or [])]
    if any(len(p) != 2 or p[0] not in sequences or p[1] not in sequences for p in pairs):
        return {"ok": False, "error": "hitstop_pairs: each pair is [whiff_sequence, hit_sequence], both in sequences"}
    base, variants = variant_runs[0], variant_runs[1:]
    hb_errs = hitbox_cvar_errors(variant_runs)
    if hb_errs:
        return {"ok": False, "verdict": "FAIL", "stage": "hitbox_cvars", "error": "; ".join(hb_errs), "captured": False}

    # 1. hard gate for every variant: nothing is captured (or planned for real) if one fails. A variant with "card" and no
    #    intent_hash gets its report from the card's animpipe check first (card_report), then the same gate
    gates, cards = {}, {}
    for v in variants:
        if v.get("card") and not v.get("intent_hash"):
            cards[v["name"]] = card_report(repo, v["asset"], v["card"], v.get("check_json"), v.get("build_json"), base["asset"],
                                           v.get("approved_base_card") or base.get("card"), v.get("approved_base_check_json"))
            v = dict(v, intent_hash=cards[v["name"]]["intent_hash"])
            variant_runs[[r["name"] for r in variant_runs].index(v["name"])] = v
        gates[v["name"]] = precapture_gate(
            repo, v["asset"], base["asset"], v.get("intent_hash"), v.get("clips") or [v["asset"]],
            tuple(edit_window) if edit_window else None, objective or v["name"], hitbox_required, min_move_cm, min_rot_deg,
            no_stop=no_stop, base_clips=base.get("clips") or [base["asset"]])
        if v["name"] in cards:
            gates[v["name"]]["card_report"] = cards[v["name"]]
    variants = variant_runs[1:]
    failed = {k: g for k, g in gates.items() if not g["ok"]}
    gate_file = repo / out_rel / "gate_report.json"
    gate_file.parent.mkdir(parents=True, exist_ok=True)
    gate_file.write_text(json.dumps({"verdict": "FAIL" if failed else "PASS", "gates": gates}, indent=1), encoding="utf-8")
    if failed:
        first = next(iter(failed.values()))
        err = first["error"]
        if first.get("card_report") and not first["card_report"]["ok"]:
            err = first["card_report"]["error"] + "; " + err
        return {"ok": False, "verdict": "FAIL", "stage": "precapture_gate", "error": err, "gates": gates,
                "captured": False, "gate_report": str(gate_file.relative_to(repo))}

    # 2. cache: reuse what is already captured
    build_id = build if build is not None else manifest.cpp_source_hash(repo)[:16]
    vstr = ",".join(_views_list(views))
    # Level 2 is never windowed (INV: "no frame uncovered", rule 10/17): the final round captures the whole move through the
    # full chain; the fast window is for the cycle rounds only (unified skill 2026-11-24)
    if final_round:
        fast = False
    win = list(fast_window or FAST_WINDOW)
    chain = f"{CAPTURE_CHAIN}:{win[0]},{win[1]}" if fast else "png-export"
    keys, reuse = {}, {}
    for i, r in enumerate(variant_runs):
        keys[r["name"]] = _run_key(repo, r, sequences, vstr, build_id, i > 0, chain)
        d = _cached_dir(repo, r, keys[r["name"]], sequences)
        if d is not None:
            reuse[r["name"]] = d
    todo = [r for r in variant_runs if r["name"] not in reuse]

    # 3. plan
    asserts = assert_spec or {}
    # every run carries its kit (the base kit's runs too): base and variant kits of a move share ONE Play session
    runs_json = [{"name": r["name"], "kit": r.get("kit") or kit, "cvars": r.get("cvars", ""), "sequences": list(sequences),
                  "assert": r.get("assert") or asserts} for r in todo]
    cap_win = tuple(window) if window else None
    if cap_win is None:
        ws = [e["window"] for es in asserts.values() for e in es if e.get("window")]
        cap_win = (min(w[0] for w in ws), max(w[1] for w in ws)) if ws else (0, 120)
    runs_file = f"{raw_rel}/runs.json"
    assert_file = f"{raw_rel}/assert_spec.json"
    ps1 = str(repo / TIMED_PS1)
    cmd = ["pwsh", "-NoProfile", "-File", ps1, "-Kit", kit, "-Out", raw_rel, "-Assert", assert_file] if todo else None
    env = {"PIE_CAPTURE_RUNS": runs_file, "PIE_CAPTURE_VIEWS": vstr}
    if fast:
        env.update({"PIE_FASTCAP": "1", "PIE_FASTCAP_WINDOW": f"{win[0]},{win[1]}"})
    run_dirs = {r["name"]: str(reuse[r["name"]]) if r["name"] in reuse else str(repo / raw_rel / r["name"]) for r in variant_runs}
    plan = {"kit": kit, "sequences": sequences, "views": vstr, "out_dir": out_rel, "raw_dir": raw_rel, "build_id": build_id,
            "capture_runs": [r["name"] for r in todo], "reused": {k: str(v) for k, v in reuse.items()},
            "command": cmd, "env": env, "runs_json": runs_json, "pack_window": list(cap_win), "anchor": anchor, "faf": faf,
            "chain": chain, "skipped_hitbox_runs": skipped_hitbox, "hitstop_pairs": pairs,
            "outputs": [f"{out_rel}/grid_<seq>_<view>.png", f"{out_rel}/<run>_<seq>_real.mp4", f"{out_rel}/<run>_<seq>_slow4x.mp4"]}
    if dry_run:
        return {"ok": True, "dry_run": True, "gates": gates, "plan": plan}

    # 4. real run
    t0 = time.time()
    minutes = 0.0
    if todo:
        (repo / raw_rel).mkdir(parents=True, exist_ok=True)
        (repo / runs_file).write_text(json.dumps(runs_json, indent=2), encoding="utf-8")
        (repo / assert_file).write_text(json.dumps(asserts, indent=2), encoding="utf-8")
        for v in variants:
            if v["name"] not in reuse:
                manifest.bump_attempt(repo, v["asset"], objective or v["name"])
        full_env = os.environ.copy()
        full_env.update(dict(env, PIE_CAPTURE_RUNS=str(repo / runs_file)))
        res = (runner or default_runner)(cmd, full_env, str(repo))
        minutes = (time.time() - t0) / 60.0
        if _rc(res) != 0:
            return {"ok": False, "verdict": "FAIL", "stage": "capture", "returncode": _rc(res), "plan": plan,
                    "error": "capture failed or ASSERT not OK (pie_capture_timed exit code != 0)",
                    "stdout": (getattr(res, "stdout", "") if not isinstance(res, dict) else res.get("stdout", ""))[-600:]}
        for r in todo:
            m = manifest.load(repo, r["asset"])
            m["captures"][keys[r["name"]]] = {"dir": f"{raw_rel}/{r['name']}", "run": r["name"], "sequences": sequences,
                                              "views": vstr, "t": time.strftime("%Y-%m-%dT%H:%M:%S")}
            if r.get("intent_hash"):
                m["intents_captured"][r["intent_hash"]] = keys[r["name"]]
            manifest.save(repo, r["asset"], m)
            manifest.record_step(repo, r["asset"], "capture", minutes / max(1, len(todo)))

    # 5. review pack (grids need Pillow; videos need ffmpeg through the runner)
    (repo / out_rel).mkdir(parents=True, exist_ok=True)
    # skill-k8: a whiff with a dummy still near the host (pie_capture_move writes dummy_in_view.txt) is not a clean stage: no review pack
    dirty = sorted(str(p.relative_to(repo)) if p.is_relative_to(repo) else str(p)
                   for r in variant_runs for p in Path(run_dirs[r["name"]]).glob("*/dummy_in_view.txt"))
    if dirty:
        return {"ok": False, "verdict": "FAIL", "stage": "scene", "plan": plan, "captured": bool(todo),
                "error": f"dummy in view during a whiff ({len(dirty)} sequence(s)): {', '.join(dirty[:3])}; recapture"}
    # Hitbox runs ("hitbox_view": true, cvars with gstn.Combat.Debug.Hitboxes 1) stay OUT of the animation review (grids and videos):
    # owner 2026-11-23, the hitboxes are for the owner's check, not for the animator. They get their own videos in <out>/hitbox/
    review = [r for r in variant_runs if not r.get("hitbox_view")]
    hbruns = [r for r in variant_runs if r.get("hitbox_view")]
    pack = review_pack.build_pack({r["name"]: Path(run_dirs[r["name"]]) for r in review}, sequences, _views_list(views),
                                  repo / out_rel, cap_win, anchor, faf, dense_radius, 1, max_images)
    hit_seqs = {p[1] for p in pairs}
    extend = {r["name"]: {s: hitstop_frames(run_dirs[r["name"]], s) for s in hit_seqs} for r in review}
    video_cmds = video_commands(repo, review, run_dirs, sequences, out_rel, cap_win, pack["alignment"], extend)
    video_cmds += hitstop_pair_commands(repo, review, out_rel, pairs)
    if hbruns:
        al = {s: review_pack.run_offsets({r["name"]: Path(run_dirs[r["name"]]) for r in [variant_runs[0]] + hbruns}, s)
              for s in sequences}
        hb_cmds = [c + ["--hitboxes=1"] for c in video_commands(repo, hbruns, run_dirs, sequences, f"{out_rel}/hitbox", cap_win, al)]
    else:
        hb_cmds = []
    if pack.get("grids") and sum(g.get("hitbox_cells", 0) for g in pack["grids"]):   # review grids are drawn without hitboxes
        return {"ok": False, "verdict": "FAIL", "stage": "hitbox_in_review", "plan": plan,
                "error": "hitboxes drawn in the review grids: they belong only in <out>/hitbox/"}
    if any("--hitboxes=1" in c for c in video_cmds):
        return {"ok": False, "verdict": "FAIL", "stage": "hitbox_in_review", "plan": plan, "error": "a review video asks for hitboxes"}
    video_cmds += hb_cmds
    video_rc = [_rc((runner or default_runner)(c, os.environ.copy(), str(repo))) for c in video_cmds]
    # camera jumps read as a snap of the body in slow motion: warn (frames inside the review window only), never hide them
    jumps = {f"{r['name']}/{s}": j for r in variant_runs for s in sequences
             for j in [camera_jumps(run_dirs[r["name"]], s, frames=(cap_win[0] - 5, cap_win[1] + 60))] if j}
    (repo / out_rel / "alignment.json").write_text(json.dumps(pack["alignment"], indent=1), encoding="utf-8")
    life = life_images(repo, review, sequences, _views_list(views), out_rel, cap_win, anchor, pack["alignment"], run_dirs,
                       no_stop, motion)
    order = review_pack.order_pack(life["slowmo"], life["motion"], [g["png"] for g in pack["grids"]], max_images)
    slow_mp4 = [f"{out_rel}/{r['name']}_{s}_slow4x.mp4" for r in review for s in sequences]
    (repo / out_rel / "pack.json").write_text(json.dumps(dict(order, slowmo_mp4=slow_mp4, stops=life["stops"],
                                                                errors=life["errors"]), indent=1), encoding="utf-8")
    # Level 2 (final review) material, never trimmed: whole-move slowmo in 3 views, velocity sheets of every clip, key poses,
    # base comparison, critic brief (contract/invarianti-review.md). complete false = no level-2 review (INV-08: the gates above
    # only discard, they never promote)
    import final_review_pack
    var = review[1] if len(review) > 1 else review[0]
    try:
        fin = final_review_pack.build_final_pack(
            out_rel, {r["name"]: run_dirs[r["name"]] for r in review}, sequences, list(cap_win), list(identity or []),
            key_frames, anchor, faf, _views_list(views), [_name(c) for c in (var.get("clips") or [var["asset"]])],
            [_name(c) for c in (review[0].get("clips") or [review[0]["asset"]])], (no_stop or {}).get("holds"),
            (no_stop or {}).get("hold_reasons"), [v for v in slow_mp4 + [x.replace("_slow4x", "_real") for x in slow_mp4]],
            root=repo)
    except Exception as exc:  # noqa: BLE001 - reported: an incomplete pack is never reviewed
        fin = {"complete": False, "missing": ["build"], "invariants": {}, "errors": [str(exc)]}
    final = {"complete": fin["complete"], "missing": fin["missing"], "json": f"{out_rel}/final/review_pack.json",
             "notes": {k: fin["invariants"][k]["note"] for k in fin["missing"] if k in fin.get("invariants", {})},
             "errors": fin.get("errors", [])}
    return {"ok": True, "verdict": "PASS", "captured": bool(todo), "reused": sorted(reuse), "out_dir": out_rel,
            "final_pack": final, "review_ready": fin["complete"],
            "pack": {"frames": pack["frames"], "grids": [g["png"] for g in pack["grids"]],
                     "missing_frames": sum(len(g["missing"]) for g in pack["grids"]),
                     "offsets": {s: a["offsets"] for s, a in pack["alignment"].items()},
                     "unaligned": {s: a["unknown"] for s, a in pack["alignment"].items() if a["unknown"]},
                     "hitbox_cells": sum(g.get("hitbox_cells", 0) for g in pack["grids"]),
                     "images": order["images"], "over_budget": order["over_budget"], "slowmo_mp4": slow_mp4,
                     "stops": life["stops"], "life_errors": life["errors"]},
            "videos_failed": sum(1 for c in video_rc if c != 0), "minutes": float(f"{minutes:.2f}"), "plan": plan,
            "hitstop_frames": extend, "camera_jumps": jumps,
            "warnings": [f"camera jump {k}: " + ", ".join(f"{x['view']} v{x['frame']} {x['cm']} cm" for x in v[:4]) for k, v in jumps.items()],
            "next": ("level-2 review = final/review_pack.json in its order (videos, whole-move slowmo 3 views, velocity sheets, "
                     "key poses) + pack.json grids; blind critic gets final/critico_brief.md; commit the out_dir"
                     if fin["complete"] else "NO level-2 review: final pack incomplete (" + ", ".join(fin["missing"])
                     + "); fix and rebuild with final_review_pack.py, the gates passing is not a PASS")
                    + ("; PACK OVER 8 IMAGES: nothing was cut, write it in the handover" if order["over_budget"] else "")}


def life_images(repo: Path, review: list[dict[str, Any]], sequences: list[str], views: list[str], out_rel: str,
                cap_win: tuple[int, int], anchor: int | None, alignment: dict[str, Any], run_dirs: dict[str, str],
                no_stop: dict[str, Any] | None = None, motion: dict[str, Any] | None = None) -> dict[str, Any]:
    """The images that measure LIFE, added to the pack for EVERY move (skill audit 2026-11-23, option A; K7 had them by hand only):
      - slowmo_<seq>_<view>.png: the startup frame by frame (1 tick per cell), base band over variant band, first sequence, az90;
      - velocity_<run>_az90.png, velocity_<run>_az180.png: the variant's trails of wrists, ankles, head, pelvis with velocity
        arrows, STOP dots and the speed strip (motion_overlay), base trails in grey. Clip = the variant's first clip (the startup).
    Errors (no Pillow, no joints dump) are reported, never fatal: the grids are still the review."""
    import motion_overlay
    mo = dict(motion or {})
    out = {"slowmo": [], "motion": [], "stops": {}, "errors": []}
    seq = sequences[0]
    view = "az90" if "az90" in views else views[0]
    try:
        rd = {r["name"]: Path(run_dirs[r["name"]]) for r in review}
        tile = review_pack.build_slowmo_tile(rd, seq, view, review_pack.startup_frames(cap_win, anchor, int(mo.get("slowmo_cells", 24))),
                                             repo / out_rel / f"slowmo_{seq}_{view}.png", int(mo.get("slowmo_cols", 8)),
                                             offsets=(alignment.get(seq) or {}).get("offsets"))
        out["slowmo"].append(tile["png"])
    except Exception as exc:  # noqa: BLE001 - reported in pack.json, the grids stay valid
        out["errors"].append(f"slowmo tile: {exc}")
    base = review[0]
    for r in review[1:2]:   # the variant under review (the first one): two velocity sheets, the pack's budget
        clip = (r.get("clips") or [r["asset"]])[0]
        holds = ((no_stop or {}).get("holds") or {}).get(_name(clip))
        res = motion_overlay.anim_motion_overlay(clip, mo.get("frame_range"), mo.get("views", "az90,az180"),
                                                 base_asset=(base.get("clips") or [base["asset"]])[0], root=repo,
                                                 out=out_rel, holds=holds, prefix=f"velocity_{r['name']}",
                                                 stop_cm=float((no_stop or {}).get("stop_cm", no_stop_gate.DEFAULTS["stop_cm"])),
                                                 still_run=int((no_stop or {}).get("still_run", no_stop_gate.DEFAULTS["still_run"])))
        if res.get("ok"):
            out["motion"] += list(res["images"].values())
            out["stops"][r["name"]] = {"stops": res["stops"], "blocking_runs": res["still_runs_blocking"]}
        else:
            out["errors"].append(f"velocity sheets {r['name']}: {res.get('error')}")
    return out

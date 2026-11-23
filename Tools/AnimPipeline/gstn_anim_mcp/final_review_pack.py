"""Level-2 (final) review pack, built deterministically at 0 tokens: everything the final review needs, nothing trimmed.

Why (consult 2026-11-24, owner rule: quality equal to the OG skill, savings only from automatic tools): every "slimming" of the
animation skill cut the final look at the motion (8-image cap, grids only, anchor/FAF window, inherited parts skipped) and K8 r2
passed with a1-a5 frozen. This tool prepares the WHOLE level-2 material so the reviewer spends tokens on judging, not on gathering:
  INV-02 the real + slow4x videos of every run (listed, must exist);
  INV-03 the whole move in slow motion, 1 tick per cell, in az90 + az180 + game, base band over variant band, every frame of the
         window covered (coverage is checked: an uncovered frame = pack incomplete);
  INV-04 the velocity sheets (trails, an arrow per frame, STOP marks, speed strip) for EVERY clip of the variant, base trails in grey;
  INV-05 one large image per key pose and view, base beside variant;
  INV-06 base comparison on the same montage-aligned frames, inherited clips included;
  INV-01 the identity traits of the brief, INV-07 the blind critic's brief, INV-11 the measured stops.
`complete: false` means NO level-2 review: the missing items are listed, the pack is never cut to fit a budget (INV-09).
The invariant ids are the ones of `contract/invarianti-review.md`; tests/test_review_invariants.py ties the two together.

CLI: python final_review_pack.py <spec.json>   (spec = the keyword arguments of build_final_pack, paths relative to the repo)
"""
from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

_HERE = str(Path(__file__).resolve().parent)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import review_pack  # noqa: E402

# Single source of the invariant ids (skill text, contract and this pack must all carry them; see the test)
INVARIANTS = {
    "INV-01": "identity traits first",
    "INV-02": "life before numbers, on the moving video (real + slow4x)",
    "INV-03": "slow motion of the whole move, 1 tick per cell, az90 + az180 + game, no frame uncovered",
    "INV-04": "velocity sheets with stops and trails for every clip",
    "INV-05": "large images of every key pose per view",
    "INV-06": "before/after against the base on the same frames, inherited parts included",
    "INV-07": "blind independent critic",
    "INV-08": "level 1 may discard a round, never promote it",
    "INV-09": "no image cap on level 2: the cap is no frame uncovered",
    "INV-10": "a false alarm is fixed, never softened; inherited blocks block",
    "INV-11": "OG life metrics and pose checks are part of level 2",
    "INV-12": "stops are defects by default; a hold needs a written reason and is still judged on video",
}
PACK_INVARIANTS = ("INV-01", "INV-02", "INV-03", "INV-04", "INV-05", "INV-06", "INV-07", "INV-09", "INV-11", "INV-12")
REQUIRED_VIEWS = ("az90", "az180", "game")
CRITERIA = ("docs/notes/review/critico-estetico.md", "Tools/AnimPipeline/contract/rubric.md",
            "docs/notes/anim-theory/principi-animazione-corporea.md", "Tools/AnimPipeline/contract/invarianti-review.md")


def _rel(repo: Path, p: Path | str) -> str:
    p = Path(p)
    return str(p.relative_to(repo)) if p.is_absolute() and p.is_relative_to(repo) else str(p)


def coverage(runs: dict[str, Path], seq: str, views: list[str], frames: list[int], offsets: dict[str, int]) -> dict[str, list[str]]:
    """{view: ['run fN', ...]} of the window frames with no captured image (offset per run = montage-aligned frame)."""
    out: dict[str, list[str]] = {}
    for v in views:
        miss = [f"{n} f{f + int(offsets.get(n, 0))}" for n, d in runs.items() for f in frames
                if not review_pack.frame_path(d, seq, v, f + int(offsets.get(n, 0))).exists()]
        if miss:
            out[v] = miss
    return out


def build_final_pack(out: str, runs: dict[str, str], sequences: list[str], window: list[int], identity: list[str],
                     key_frames: list[int] | None = None, anchor: int | None = None, faf: int | None = None,
                     views: list[str] | str = REQUIRED_VIEWS, variant_clips: list[str] | None = None,
                     base_clips: list[str] | None = None, holds: dict[str, Any] | None = None,
                     hold_reasons: dict[str, str] | None = None, videos: list[str] | None = None,
                     root: str | Path | None = None, tile_cells: int = 32, tile_cols: int = 8, cell: int = 160,
                     key_cell: int = 512, brief: str = "") -> dict[str, Any]:
    """runs = {name: capture dir} with the BASE FIRST (pie_capture_move layout <dir>/<seq>/<view>/fNNN.png). window = [a, b]
    base capture frames of the whole move (startup to the end of the settle). variant_clips / base_clips = every clip the
    montage plays (startup, loop, settle, landing), paired by index (joints dumps in Saved/AnimPipeline/runstart/). Writes
    <out>/final/ and <out>/final/review_pack.json; returns the same dict."""
    import motion_overlay

    repo = Path(root).resolve() if root else Path(__file__).resolve().parents[3]
    views = [v for v in (views.split(",") if isinstance(views, str) else views) if v]
    outd = repo / out / "final"
    outd.mkdir(parents=True, exist_ok=True)
    rd = {n: (Path(d) if Path(d).is_absolute() else repo / d) for n, d in runs.items()}
    names = list(rd)
    a, b = int(window[0]), int(window[1])
    frames = list(range(a, b + 1))
    inv: dict[str, dict[str, Any]] = {k: {"ok": False, "files": [], "note": ""} for k in PACK_INVARIANTS}
    errors: list[str] = []

    # INV-01 identity (rule 13a: 2-3 traits from the brief, judged first)
    ident = [t for t in (identity or []) if str(t).strip()]
    inv["INV-01"].update(ok=len(ident) >= 2, traits=ident,
                         note="" if len(ident) >= 2 else "the brief must give 2-3 identity traits (rule 13a)")

    # INV-02 videos: real + slow4x of every run and sequence
    vids = list(videos) if videos else [f"{out}/{n}_{s}_{t}.mp4" for n in names for s in sequences for t in ("real", "slow4x")]
    vmiss = [v for v in vids if not (repo / v).exists()]
    inv["INV-02"].update(ok=bool(vids) and not vmiss and any("slow4x" in v for v in vids), files=vids,
                         note=f"missing videos: {', '.join(vmiss[:4])}" if vmiss else "")

    # INV-03 whole-move slowmo per sequence and view, chunked (never trimmed), + coverage; INV-06 base band in every tile
    slow, uncovered, aligned = [], {}, {}
    missing_views = [v for v in REQUIRED_VIEWS if v not in views]
    for s in sequences:
        al = review_pack.run_offsets(rd, s)
        aligned[s] = al
        offs = al["offsets"]
        for v in views:
            for i in range(0, len(frames), tile_cells):
                part = frames[i:i + tile_cells]
                t = review_pack.build_slowmo_tile(rd, s, v, part, outd / f"slowmo_{s}_{v}_p{i // tile_cells + 1:02d}.png",
                                                  tile_cols, cell, offs, label="whole move slowmo")
                slow.append(_rel(repo, t["png"]))
        cov = coverage(rd, s, views, frames, offs)
        if cov:
            uncovered[s] = cov
    inv["INV-03"].update(ok=not missing_views and not uncovered and bool(slow), files=slow, frames=[a, b],
                         note="; ".join(([f"views missing: {', '.join(missing_views)}"] if missing_views else [])
                                        + [f"{s} {v}: {len(m)} frames uncovered" for s, c in uncovered.items() for v, m in c.items()]))

    # INV-04 velocity sheets for every clip of the variant (base trails in grey), INV-11 measured stops
    vel, stops = [], {}
    vclips = list(variant_clips or [])
    for i, clip in enumerate(vclips):
        bclip = (base_clips or [None] * len(vclips))[i] if i < len(base_clips or []) else None
        cname = Path(clip).name
        res = motion_overlay.anim_motion_overlay(cname, None, "az90,az180", base_asset=Path(bclip).name if bclip else None,
                                                 root=repo, out=f"{out}/final", holds=(holds or {}).get(cname),
                                                 prefix=f"velocity_{cname}")
        if res.get("ok"):
            vel += [_rel(repo, p) for p in res["images"].values()]
            stops[cname] = {"stops": res.get("stops"), "blocking_runs": res.get("still_runs_blocking")}
        else:
            errors.append(f"velocity {cname}: {res.get('error')}")
    inv["INV-04"].update(ok=bool(vclips) and len(vel) >= 2 * len(vclips), files=vel,
                         note="" if vclips else "list every clip of the montage in variant_clips (startup, loop, settle, landing)")
    life = life_metrics(repo, [Path(c).name for c in vclips], holds, hold_reasons)
    inv["INV-11"].update(ok=inv["INV-04"]["ok"], stops=stops, life=life,
                         note="stops measured; " + ("OG life metrics from animpipe life attached" if life and all(
                             "error" not in v for v in life.values()) else "OG life metrics (impact spacing, hold/recoil, cascade, "
                             "arc jitter) pending animpipe life: judge them on this pack by eye") + "; pose checks by eye on the key poses")

    # INV-12 holds: a stop is a defect by default (owner 2026-11-24); every declared hold needs its written reason and goes on
    # the reviewer's list to be judged ON VIDEO (the declaration never skips the judgement)
    hr = hold_reasons or {}
    no_reason = [c for c, h in (holds or {}).items() if h and not str(hr.get(c, "")).strip()]
    to_judge = [{"clip": c, "frames": h, "reason": hr.get(c, "")} for c, h in (holds or {}).items() if h]
    inv["INV-12"].update(ok=not no_reason, judge_on_video=to_judge,
                         note=f"holds without a reason: {', '.join(no_reason)}" if no_reason else
                         (f"{len(to_judge)} declared hold(s): judge each on the video" if to_judge else "no declared holds"))

    # INV-05 large key poses, base beside variant (rows = runs, base on top)
    keys = sorted({int(k) for k in (key_frames or []) if a <= int(k) <= b} | {k for k in (anchor, faf) if k is not None and a <= k <= b})
    kp = []
    for s in sequences:
        for v in views:
            for k in keys:
                g = review_pack.build_grid(rd, s, v, [k], outd / f"keypose_{s}_{v}_f{k:03d}.png", key_cell, aligned[s]["offsets"])
                kp.append(_rel(repo, g["png"]))
    inv["INV-05"].update(ok=bool(keys), files=kp, frames=keys, note="" if keys else "no key frames (give key_frames, anchor, faf)")

    # INV-06 base comparison: >= 2 runs, base first, every variant clip paired with a base clip, alignment known
    unal = {s: al["unknown"] for s, al in aligned.items() if al["unknown"]}
    pair_ok = bool(vclips) and len(base_clips or []) >= len(vclips)
    inv["INV-06"].update(ok=len(names) >= 2 and pair_ok and not unal, base=names[0] if names else None,
                         note="; ".join(([] if len(names) >= 2 else ["need base + variant runs"])
                                        + ([] if pair_ok else ["pair every variant clip with its base clip (inherited too)"])
                                        + [f"{s}: montage start unknown for {', '.join(u)}" for s, u in unal.items()]))

    # INV-07 blind critic brief: pack + criteria + identity, never the reviewer's verdict, handovers, code or logs
    order = inv["INV-02"]["files"] + slow + vel + kp
    crit = outd / "critico_brief.md"
    crit.write_text("\n".join(
        ["# Critico cieco (livello 2, INV-07)", "",
         "Subagente indipendente: non ha editato la mossa, non legge il verdetto del revisore, handover, codice o log.",
         f"Brief: {brief or '(path del brief della mossa)'}", "", "## Tratti d'identita' (giudicali per primi)"]
        + [f"- {t}" for t in ident] + ["", "## Criteri"] + [f"- {c}" for c in CRITERIA]
        + ["", "## Tenute dichiarate (giudicale a video: la dichiarazione non salta il giudizio)"]
        + [f"- {t['clip']} {t['frames']}: {t['reason']}" for t in to_judge] + ([] if to_judge else ["- nessuna"])
        + ["", "## Materiale, in quest'ordine (video prima dei numeri)"] + [f"- {p}" for p in order]
        + ["", "Esito: PASS/FAIL, max 3 difetti con frame, vista ed etichetta posa / dati montage / gioco."]), encoding="utf-8")
    inv["INV-07"].update(ok=True, files=[_rel(repo, crit)])

    # INV-09 nothing trimmed: every produced image is listed, no cap
    inv["INV-09"].update(ok=True, note=f"{len(slow) + len(vel) + len(kp)} images, no cap (the cap is: no frame uncovered)")

    missing = [k for k in PACK_INVARIANTS if not inv[k]["ok"]]
    res = {"version": 1, "complete": not missing, "missing": missing, "invariants": inv, "order": order,
           "sequences": sequences, "views": views, "window": [a, b], "runs": names, "uncovered": uncovered,
           "alignment": {s: al["offsets"] for s, al in aligned.items()}, "errors": errors,
           "image_count": len(slow) + len(vel) + len(kp)}
    (outd / "review_pack.json").write_text(json.dumps(res, indent=1), encoding="utf-8")
    return res


def life_metrics(repo: Path, clips: list[str], holds: dict[str, Any] | None = None,
                 hold_reasons: dict[str, str] | None = None) -> dict[str, Any]:
    """OG life metrics per clip from `python -m animpipe life <joints.json> --json` (thread "Restore OG life metrics",
    animpipe/life.py, branch claude/project-thread-1uvcac 1018a4fb). {} while that module does not exist; an error per clip is reported, never hidden."""
    ap = repo / "Tools" / "AnimPipeline"
    if not (ap / "animpipe" / "life.py").exists():
        return {}
    import subprocess
    out: dict[str, Any] = {}
    for c in clips:
        j = repo / "Saved" / "AnimPipeline" / "runstart" / f"joints_{c}.json"
        if not j.exists():
            out[c] = {"error": f"joints dump missing: {_rel(repo, j)}"}
            continue
        cmd = [sys.executable, "-m", "animpipe", "life", str(j), "--json"]
        if (holds or {}).get(c):     # declared holds travel with their reason (INV-12), format a-b:why
            cmd.append(f"--holds={holds[c]}:{(hold_reasons or {}).get(c, '')}")
        r = subprocess.run(cmd, cwd=str(ap), capture_output=True, text=True)
        try:
            txt = r.stdout.strip()
            out[c] = json.loads(txt) if txt.startswith("{") else json.loads(txt.splitlines()[-1])
        except (ValueError, IndexError):
            out[c] = {"error": f"animpipe life rc={r.returncode}: {(r.stderr or r.stdout)[-200:]}"}
    return out


def check_pack(path: str | Path, root: str | Path | None = None) -> list[str]:
    """Invariant ids a level-2 pack does not satisfy (missing section, not ok, or, with root, a listed file gone). [] = review may
    start. The reviewer (and the executor before handing over) runs it; a non-empty list = no level-2 review, no PASS."""
    p = Path(path)
    if not p.exists():
        return list(PACK_INVARIANTS)
    d = json.loads(p.read_text(encoding="utf-8"))
    bad = []
    for k in PACK_INVARIANTS:
        e = (d.get("invariants") or {}).get(k)
        if not e or not e.get("ok") or (root and any(not (Path(root) / f).exists() for f in e.get("files", []))):
            bad.append(k)
    return bad


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: final_review_pack.py <spec.json>")
        return 2
    spec = json.loads(Path(argv[1]).read_text(encoding="utf-8"))
    r = build_final_pack(**spec)
    print(f"FINAL_PACK: {'COMPLETE' if r['complete'] else 'INCOMPLETE'} ({r['image_count']} images)")
    if r["missing"]:
        print("missing: " + "; ".join(f"{k} {r['invariants'][k]['note']}" for k in r["missing"])[:400])
    print(f"out: {spec['out']}/final/review_pack.json")
    return 0 if r["complete"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

"""animpipe check <card>: the three classes of checks (brief §4.3) on the card's built AnimSequence, in one pass, no fixes.

  block    invariants: a FAIL stops the move. bone_lengths, clearance, legibility (the striking part seen from the game
           cameras), contact/FAF (the effector peaks inside the contact window, the clip reaches the FAF), planted feet,
           closed fists, hitstop_snap (default on every move: no bone travels more than HITSTOP_MAX cm from a
           hitbox-active frame to the next one, since a hit freezes the clip on that frame and the next frame would pop).
  measure  reported, never block: motion_curves (+ motion_ref against the card's reference), twist_chain, motion_check
           (weight, overlap, balance, pose rules), spacing of the built curves, distance from the source poses.
  eye      quality the owner judges on the review grid; listed, not computed.

The existing scripts run unchanged as subprocesses, each one's output goes to <OUT>/<move>_check/<check>.txt; this
module reads only their verdict line. Bone data: legibility's cache sheets/raw/<take>_legbones.json (component space,
X left, Y forward, Z up). Full result: <OUT>/<move>_check.json. Prints at most 5 lines."""
import json, os, re, subprocess, sys
import numpy as np

from animpipe.card import load, windows, to_move_sheet
from animpipe.__main__ import OUT, TOOLS

PLANT_TOL = 2.0          # cm a planted foot (ankle and ball) may drift from frame 0
HITSTOP_MAX = 10.0       # cm per anim frame (60 fps) out of a hitbox-active frame; card `hitstop_max_cm` overrides
FIST_MAX = 0.7           # fingertip-to-wrist distance / finger chain length; open hand ~0.95, closed fist ~0.5
EYE = ["arcs and silhouette on the review grid", "weight and snap at 1x and 0.25x", "face/head follow-through"]


def _run(name, args, outdir):
    env = dict(os.environ, MSYS_NO_PATHCONV="1", PYTHONIOENCODING="utf-8")
    p = subprocess.run([sys.executable] + args, cwd=TOOLS, capture_output=True, text=True, env=env, encoding="utf-8",
                       errors="replace")
    txt = (p.stdout or "") + (p.stderr or "")
    open(os.path.join(outdir, f"{name}.txt"), "w", encoding="utf-8").write(txt)
    lines = [l for l in txt.splitlines() if l.strip() and not re.match(r"\s+(frame \d+|\d+ frames) done", l)]
    return p.returncode, lines


def _verdict_line(lines, pat=r"VERDICT|PASS|FAIL|OK|WARN"):
    for l in lines:
        if re.search(pat, l):
            return l.strip()[:160]
    return (lines[-1].strip()[:160] if lines else "no output")


def _bones(take):
    d = json.load(open(os.path.join(TOOLS, "sheets", "raw", f"{take}_legbones.json")))["frames"]
    return [{b: np.array(v[:3]) for b, v in d[str(i)].items()} for i in range(len(d))]


def _contact_faf(F, w, eff):
    """Block: the effector's forward reach (Y relative to the pelvis) peaks inside the contact window; clip >= FAF."""
    reach = [f[eff][1] - f["pelvis"][1] for f in F]
    peak = int(np.argmax(reach))
    c0, c1 = w["active"]
    ok = c0 <= peak <= c1 and len(F) - 1 >= w["faf"]
    return ok, f"reach peak at frame {peak} (window {c0}-{c1}), clip {len(F)} frames, FAF {w['faf']}"


def _planted(F, exempt, sides="lr"):
    """Only the card's planted feet (foot_<s>_ik_ctrl): a kicking or stepping foot is not planted."""
    worst = {}
    for s in sides:
        for b in (f"foot_{s}", f"ball_{s}"):
            d = [float(np.linalg.norm(f[b] - F[0][b])) for i, f in enumerate(F) if i not in exempt]
            worst[b] = round(max(d), 2)
    bad = {b: v for b, v in worst.items() if v > PLANT_TOL}
    return not bad, f"max drift {max(worst.values())} cm (tol {PLANT_TOL})" + (f", over: {bad}" if bad else "")


def _fists(F, hands):
    worst = {}
    for s in hands:
        chain = [f"hand_{s}", f"middle_metacarpal_{s}", f"middle_01_{s}", f"middle_02_{s}", f"middle_03_{s}"]
        r = []
        for f in F:
            ln = sum(np.linalg.norm(f[a] - f[b]) for a, b in zip(chain, chain[1:]))
            r.append(np.linalg.norm(f[chain[-1]] - f[chain[0]]) / ln)
        worst[s] = (round(float(max(r)), 2), int(np.argmax(r)))
    bad = {s: v for s, v in worst.items() if v[0] > FIST_MAX}
    return not bad, "openness (ratio, frame) " + ", ".join(f"{s} {v[0]}@{v[1]}" for s, v in worst.items()) + f" (max {FIST_MAX})"


def _hitstop_snap(F, w, lim, exempt):
    """Block (owner rule, 02/11): the hit can land on any active frame and freezes the clip there for the hitstop, so
    the step from every active frame f to f+1 is what the player sees right after the freeze. The largest per-bone
    displacement (component space, every sampled bone) of each such step must stay under `lim`. A big travel belongs
    before the hitbox opens (into the first active frame) or later in the recovery, never right after an active frame."""
    a0, a1 = w["active"]
    worst = []
    for f in range(a0, min(a1, len(F) - 2) + 1):
        if f in exempt:
            continue
        b, d = max(((b, float(np.linalg.norm(F[f + 1][b] - F[f][b]))) for b in F[f]), key=lambda kv: kv[1])
        worst.append((f, b, round(d, 1)))
    bad = [x for x in worst if x[2] > lim]
    return not bad, "active->next max " + ", ".join(f"{f}->{f + 1} {d} cm ({b})" for f, b, d in worst) + f" (max {lim})"


def _spacing(F, segs, eff):
    """Measure: effector progress at the middle frame of every pose segment longer than 2 frames (shorter ones have no
    frame inside to see the spacing at 60 fps)."""
    out = []
    for a, b, p in segs:
        if b - a < 3:
            continue
        tot = np.linalg.norm(F[b][eff] - F[a][eff])
        if tot < 1.0:
            continue
        m = (a + b) // 2
        out.append(f"{a}-{b} {p[:6]} {np.linalg.norm(F[m][eff] - F[a][eff]) / tot:.2f}")
    return out


def run(path, src=False):
    from animpipe.card import segments
    c = load(path)
    move, anim = c["move"], c["anim"]
    if src:                                   # the same checks on the source clip: the "not worse than the source" baseline
        move, anim = move + "_src", c["source_anim"]
    take = anim.rsplit("/", 1)[1][3:]
    w = windows(c)["anim"]
    outdir = os.path.join(OUT, f"{move}_check"); os.makedirs(outdir, exist_ok=True)
    # the move sheet entry the old scripts read is generated from the card, never written by hand
    ms_path = os.path.join(TOOLS, "move_sheets.json")
    ms = json.load(open(ms_path)); ms[move] = to_move_sheet(c); json.dump(ms, open(ms_path, "w"), indent=1)
    exempt = {e["check"]: set(e["frames"]) for e in c.get("exemptions", [])}
    contact, faf = f"{w['active'][0]}-{w['active'][1]}", str(w["faf"])
    res = {"block": {}, "measure": {}, "eye": EYE}

    rc, ln = _run("legibility", ["legibility.py", "measure", anim, take, contact, faf], outdir)
    if rc and not any("VERDICT" in l for l in ln):   # a source-control dialog after a save can block the bridge: once more
        rc, ln = _run("legibility", ["legibility.py", "measure", anim, take, contact, faf], outdir)
    v = _verdict_line(ln, "VERDICT")
    res["block"]["legibility"] = ("PASS" in v and "FAIL" not in v, v)
    raw = os.path.join("sheets", "raw", f"{take}_legbones.json")
    rc, ln = _run("bone_lengths", ["bone_lengths.py", raw, f"--json={os.path.join(outdir, 'bone_lengths.json')}"], outdir)
    res["block"]["bone_lengths"] = (rc == 0, _verdict_line(ln))
    rc, ln = _run("clearance", ["clearance.py", anim, "2", os.path.join(outdir, "clearance.json")], outdir)
    res["block"]["clearance"] = (rc == 0 and "FAIL" not in _verdict_line(ln), _verdict_line(ln))
    F = _bones(take)
    eff = c.get("effector", "middle_01_l")
    res["block"]["contact_faf"] = _contact_faf(F, w, eff)
    sides = "".join(s for s in "lr" if f"foot_{s}_ik_ctrl" in c.get("planted", ["foot_l_ik_ctrl", "foot_r_ik_ctrl"]))
    res["block"]["planted_feet"] = _planted(F, exempt.get("planted", set()), sides)
    res["block"]["fists"] = _fists(F, c.get("fists", ["l", "r"]))
    res["block"]["hitstop_snap"] = _hitstop_snap(F, w, float(c.get("hitstop_max_cm", HITSTOP_MAX)), exempt.get("hitstop_snap", set()))

    for name, args in (("motion_curves", ["motion_curves.py", take, f"--sheet={move}", "--quiet"]),
                       ("twist_chain", ["twist_chain.py", take, "--quiet"]),
                       ("motion_check", ["motion_check.py", take, contact, faf])):
        if name == "twist_chain" and not os.path.exists(os.path.join(TOOLS, f"{take}_targets_raw.json")):
            res["measure"][name] = (0, "n/a: reads the Cascadeur rig points (<take>_targets_raw.json); no such file for a rig build")
            continue
        rc, ln = _run(name, args, outdir)
        res["measure"][name] = (rc, _verdict_line(ln))
    res["measure"]["spacing"] = (0, "; ".join(_spacing(F, segments(c), eff)))
    b = os.path.join(OUT, f"{move}_build.json")
    if os.path.exists(b):
        bj = json.load(open(b))
        wv = max(bj.get("vs_source_cm", {"-": 0}).items(), key=lambda kv: kv[1])
        res["measure"]["vs_source"] = (0, f"worst {wv[1]} cm ({wv[0]}), at poses max {max(bj.get('at_poses_cm', {'-': 0}).values())} cm")

    fails = [k for k, (ok, _) in res["block"].items() if not ok]
    res["verdict"] = "FAIL" if fails else "PASS"
    jp = os.path.join(OUT, f"{move}_check.json")
    json.dump({"verdict": res["verdict"], "block": {k: {"ok": ok, "line": l} for k, (ok, l) in res["block"].items()},
               "measure": {k: {"rc": rc, "line": l} for k, (rc, l) in res["measure"].items()}, "eye": EYE}, open(jp, "w"), indent=1)
    txt = os.path.join(OUT, f"{move}_check.txt")
    open(txt, "w", encoding="utf-8").write("\n".join([f"{move}: {res['verdict']}"] +
                                                     [f"block {k}: {'PASS' if ok else 'FAIL'} | {l}" for k, (ok, l) in res["block"].items()] +
                                                     [f"measure {k}: {l}" for k, (_, l) in res["measure"].items()] +
                                                     [f"eye: {e}" for e in EYE]) + "\n")
    print(f"check {move}: {res['verdict']}" + (f" (blocking: {', '.join(fails)})" if fails else ""))
    print(f"block {sum(ok for ok, _ in res['block'].values())}/{len(res['block'])}, measures {len(res['measure'])}, eye {len(EYE)}")
    print(txt)

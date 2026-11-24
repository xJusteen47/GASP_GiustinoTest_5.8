"""P5 turnaround pivot foot (runstart-turn-foot, 2026-11-23): who hides the R foot spin of the 180 L turnaround in game?
The UE clip has it (foot-pelvis gap f3-f6 -17/-31/-48/-44 deg, turnarounds-4 handover), the game shows it only from ~v91.
Three suspects, one capture run each (runstart_batch_captures.ps1, same session, see docs/notes/handovers/runstart-turn-foot.md):
  BLEND  the Blend Stack entry: clip weight 0.17-0.86 at v84-v88 (assert_pivot_weights.json), the idle foot dominates
  POP    our pop guard (GSTN_AnimNode_BlendOutFootLock): the still R foot jumps > 8 cm in one frame when the body yaws ~20-40
         deg/frame -> held at its old spot and faded over 0.15 s with (1-t)^2 (about 6 frames visible) = shown from ~v91
  PIN    GASP's CR_Biped_FootPlacement pins the foot while contact_r is up: a pivot on the ball keeps contact_r = 1
Per run and frame: clip weight, R foot yaw minus hips facing (the spin as SHOWN), contact_r, pop guard phase of R (4 = Smoothing).
Usage: python runstart_turn_foot_metrics.py <batch dir> [first last]   (default v80-v100). Writes <batch>/turn_foot.txt, prints
one line per run: frames where R is in pop Smoothing, frames with contact_r > 0.5, and the shown spin at v84-v90."""
import json
import os
import sys

batch = sys.argv[1]
lo, hi = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (80, 100)
CLIP = os.environ.get("RTF_CLIP", "AS_GSTN_RunStart_Generic_w2t180L")


def wrap(a):
    return (a + 180.0) % 360.0 - 180.0


out, summary = [], []
for run in sorted(os.listdir(batch)):
    fj = None
    for root, _dirs, files in os.walk(os.path.join(batch, run)):
        if "frames.jsonl" in files:
            fj = os.path.join(root, "frames.jsonl")
            break
    if not fj:
        continue
    rows = [json.loads(x) for x in open(fj, encoding="utf-8") if x.strip()]
    out.append("== %s (%s)" % (run, fj))
    out.append("f clip_w clip_t spin_r(foot_r yaw - hips) contact_r slide_r fl_R")
    pop, pin, spin = [], [], {}
    for r in rows:
        f = r.get("f")
        if f is None or not lo <= f <= hi:
            continue
        b = r.get("body") or {}
        w, t = 0.0, None
        for a in (r.get("combat") or {}).get("anim") or r.get("anim") or []:
            p = str(a).split()
            if p and p[0].startswith(CLIP) and len(p) >= 3:
                w, t = float(p[1]), float(p[2])
        s = None
        if "foot_r_rot" in b and "face_hips" in b:
            s = round(wrap(b["foot_r_rot"][0] - b["face_hips"]), 1)
            spin[f] = s
        cv = b.get("curves") or {}
        fl = (b.get("fl") or [0])[0] % 10
        if fl == 4:
            pop.append(f)
        if cv.get("contact_r", 0) > 0.5:
            pin.append(f)
        out.append("%d %.2f %s %s %s %s %d" % (f, w, t, s, cv.get("contact_r"), cv.get("GSTN_FootSlide_r"), fl))
    seq = " ".join("%s" % spin.get(f) for f in range(84, 91))
    summary.append("%s: popR %s | contact_r>0.5 %s | spin v84-90 %s" % (run, (pop[0], pop[-1]) if pop else "-",
                                                                        (pin[0], pin[-1]) if pin else "-", seq))
open(os.path.join(batch, "turn_foot.txt"), "w", encoding="utf-8").write("\n".join(summary + [""] + out) + "\n")
print("\n".join(summary[:5]))

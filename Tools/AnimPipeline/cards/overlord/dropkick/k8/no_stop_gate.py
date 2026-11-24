"""Anti-stop gate (dropkick K7/K8 diagnosis, 2026-11-24): a clip is staccato when the WHOLE body stops on a frame inside an action or
when one frame jumps far more than its neighbours. Reads a joints dump (clip_joints_dump_ue.py: Saved/AnimPipeline/runstart/
joints_<asset>.json, component space, cm) and checks, over a frame range:
  stop: no step f->f+1 where every part (hands, forearms, head, feet relative to the pelvis, and the pelvis itself) moves < STOP_CM,
        except the declared holds (--holds=a-b,c-d: steps inside a declared, owner-wanted hold);
  pop:  the largest step of any part <= POP_RATIO x the median of that part's moving steps (> 0.5 cm).
Usage: python no_stop_gate.py <joints.json> [--range=0-15] [--holds=] [--stop=3.5] [--pop=4]
Prints `NO_STOP: OK|FAIL` and at most 4 more lines; exit 0 OK, 1 FAIL. Use it on the startup AND on the settle/loop/land clips."""
import json, sys
import numpy as np

PARTS = ["hand_MainPoint_l", "hand_MainPoint_r", "lowerarm_MainPoint_l", "lowerarm_MainPoint_r", "head", "foot_MainPoint_l",
         "foot_MainPoint_r"]


def main(argv):
    opt = dict(a.split("=", 1) for a in argv[1:] if a.startswith("--"))
    J = json.load(open(argv[0]))["joints"]
    pe = np.array(J["pelvis"], float)
    n = len(pe)
    lo, hi = (int(x) for x in opt.get("--range", f"0-{n - 1}").split("-"))
    holds = set()
    for h in filter(None, opt.get("--holds", "").split(",")):
        a, b = (int(x) for x in h.split("-"))
        holds |= set(range(a, b))
    stop_cm, pop = float(opt.get("--stop", 3.5)), float(opt.get("--pop", 4.0))
    V = {p: np.linalg.norm(np.diff(np.array(J[p], float) - pe, axis=0), axis=1) for p in PARTS if p in J}
    V["pelvis"] = np.linalg.norm(np.diff(pe, axis=0), axis=1)
    steps = range(lo, min(hi, n - 1))
    stops = [f for f in steps if f not in holds and max(v[f] for v in V.values()) < stop_cm]
    pops = []
    for p, v in V.items():
        mv = v[lo:hi][v[lo:hi] > 0.5]
        if len(mv) >= 3:
            f = int(np.argmax(v[lo:hi])) + lo
            r = float(v[f] / np.median(mv))
            if r > pop:
                pops.append(f"{p} f{f}->{f + 1} {v[f]:.0f} cm = {r:.1f}x median")
    ok = not stops and not pops
    print(f"NO_STOP: {'OK' if ok else 'FAIL'} range {lo}-{hi} holds {sorted(holds) or '-'}")
    if stops:
        print(f"whole-body stops (< {stop_cm} cm/frame) at steps: {stops}")
    for line in pops[:3]:
        print("pop " + line)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

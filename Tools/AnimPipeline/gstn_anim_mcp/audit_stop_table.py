"""Audit of the stops on every built clip (2026-11-24): one TSV row per joints dump, measured by animpipe/life.py, the same
metric `animpipe check` and life_calibrate.py use (one definition, no copy). No editor.

Why life.py and not no_stop_gate (first version, branch claude/project-thread-3ufz10): that table ran still_runs with
curves.STOP_CM = 3.5 cm on the WHOLE clip. 3.5 cm is the startup threshold (a dip between moving steps of a strike); on a
whole clip it marks every live settle or held pose as still (ftilt v3.1 FAIL 10-14 while the pelvis travels 1.5 cm and the
head 3.0 cm there, slowmo review 11-24 class D) and every stock GASP landing recovery (tests/test_life.py). The whole clip
is judged at life_dead_cm = 0.4 cm (a step under it on EVERY point is a dead frame), plus:
  holds     the declared holds must pass judge_holds, including the live core (pelvis >= life_hold_core_cm over the hold)
  stop_sync lockstep stops (flat run of minima = one, live declared holds skipped)
No dedupe: these are asset dumps, where identical frames are the freeze to catch (life.load_clip).

Columns: clip, frames, verdict, dead runs > STILL_RUN (frame ranges), every dead step, median and max body step (cm/frame),
max/median and its step, holds used, stop_sync, failed blocks.
Holds: a JSON {clip_name: "a-b:why,c-d:why"} (--holds=file), the declarations of the cards; everything else counts.
  python audit_stop_table.py <dir with joints_*.json> <out.tsv> [--holds=holds.json] [--resolved=Saved/AnimPipeline/audit_stop/resolved.json]
Prints <= 5 lines (count of FAIL / OK and the out path)."""
import glob
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import curves as CV  # noqa: E402
from animpipe import life  # noqa: E402

HEAD = ["clip", "frames", "verdict", "dead_runs", "dead_steps", "median_cm", "max_cm", "max_over_median", "max_step",
        "holds", "stop_sync", "failed"]


def row(path, holds, T=None):
    T = T or life.thresholds()
    name = os.path.basename(path)[len("joints_"):-len(".json")]
    F = life.load_clip(path)
    h = holds.get(name, "")
    r = life.run(F, holds=h, T=T, gate=False)   # an audit measures every metric raw (calibration gate: animpipe check)
    held = set()
    for a, b, _ in life.parse_holds(h):
        held |= set(range(a, b))
    st = [d for d, _ in CV.body_steps(F, life._pts(F))]
    dead = [f for f, d in enumerate(st) if f not in held and d < T["life_dead_cm"]]
    runs = CV.long_stills(F, held, T["life_dead_cm"], int(T["life_still_run"]), points=life._pts(F))
    med = statistics.median(st) if st else 0.0
    mx = max(st) if st else 0.0
    ratio = round(mx / med, 1) if med > 0 else float("inf")
    failed = [k for k, v in r["block"].items() if not v[0]]
    return [name, str(len(F)), "OK" if r["verdict"] == "PASS" else "FAIL",
            ";".join(f"{a}-{b + 1}" for a, b in runs) or "-", ",".join(map(str, dead)) or "-",
            f"{med:.2f}", f"{mx:.2f}", str(ratio), str(st.index(mx) if st else "-"), h or "-",
            r["block"]["stop_sync"][1].split(";")[0], ";".join(f"{k}: {r['block'][k][1][:120]}" for k in failed) or "-"]


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip().splitlines()[-2]); return 2
    opt = dict(a.split("=", 1) for a in argv[2:] if a.startswith("--"))
    holds = json.load(open(opt["--holds"])) if opt.get("--holds") else {}
    files = sorted(glob.glob(os.path.join(argv[0], "joints_*.json")))
    if opt.get("--resolved"):   # only this run's clips: the folder also holds joints dumps of older runs
        want = {p.rsplit("/", 1)[-1] for v in json.load(open(opt["--resolved"])).values() for p in v}
        files = [f for f in files if os.path.basename(f)[len("joints_"):-len(".json")] in want]
    T = life.thresholds()
    rows = []
    for p in files:
        try:
            rows.append(row(p, holds, T))
        except (ValueError, KeyError) as exc:       # a dump without pelvis cannot be judged: it is a FAIL, never skipped
            rows.append([os.path.basename(p), "0", "FAIL"] + ["-"] * 8 + [f"unreadable: {exc}"[:120]])
    with open(argv[1], "w", encoding="utf-8") as f:
        f.write("\t".join(HEAD) + "\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    nf = sum(r[2] == "FAIL" for r in rows)
    print(f"AUDIT_STOP: {len(rows)} clips, {nf} FAIL, {len(rows) - nf} OK (whole clip at {T['life_dead_cm']} cm) -> {argv[1]}")
    return 1 if nf else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

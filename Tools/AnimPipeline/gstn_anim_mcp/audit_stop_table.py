"""Audit stop-and-go 2026-11-24: one TSV row per joints dump, with no_stop_gate.check (one definition of the thresholds:
animpipe/curves.py). No editor. Columns: clip, frames, verdict, still runs > STILL_RUN (frame ranges), every still step,
median and max body step (cm/frame), max/median and its frame, declared holds used.
Holds: a JSON {clip_name: "a-b,c-d"} (--holds=file) for the owner-declared frozen poses; everything else counts.
  python audit_stop_table.py <dir with joints_*.json> <out.tsv> [--holds=holds.json] [--resolved=Saved/AnimPipeline/audit_stop/resolved.json]
Prints <= 5 lines (count of FAIL / OK and the out path)."""
import glob
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import no_stop_gate as G  # noqa: E402


def row(path, holds):
    name = os.path.basename(path)[len("joints_"):-len(".json")]
    J = json.load(open(path, encoding="utf-8"))["joints"]
    h = holds.get(name, "")
    r = G.check(J, h)
    st = r["steps"]
    med = statistics.median(st) if st else 0.0
    mx = max(st) if st else 0.0
    ratio = round(mx / med, 1) if med > 0 else float("inf")
    runs = ";".join(f"{a}-{b + 1}" for a, b in r["still_runs"]) or "-"
    return [name, str(len(st) + 1), "OK" if r["ok"] else "FAIL", runs, ",".join(map(str, r["stills"])) or "-",
            f"{med:.2f}", f"{mx:.2f}", str(ratio), str(st.index(mx) if st else "-"), h or "-"]


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip().splitlines()[-2]); return 2
    opt = dict(a.split("=", 1) for a in argv[2:] if a.startswith("--"))
    holds = json.load(open(opt["--holds"])) if opt.get("--holds") else {}
    files = sorted(glob.glob(os.path.join(argv[0], "joints_*.json")))
    if opt.get("--resolved"):   # only this run's clips: the folder also holds joints dumps of older runs
        want = {p.rsplit("/", 1)[-1] for v in json.load(open(opt["--resolved"])).values() for p in v}
        files = [f for f in files if os.path.basename(f)[len("joints_"):-len(".json")] in want]
    rows = [row(p, holds) for p in files]
    head = ["clip", "frames", "verdict", "still_runs", "still_steps", "median_cm", "max_cm", "max_over_median", "max_step", "holds"]
    with open(argv[1], "w", encoding="utf-8") as f:
        f.write("\t".join(head) + "\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    nf = sum(r[2] == "FAIL" for r in rows)
    print(f"AUDIT_STOP: {len(rows)} clips, {nf} FAIL, {len(rows) - nf} OK -> {argv[1]}")
    return 1 if nf else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

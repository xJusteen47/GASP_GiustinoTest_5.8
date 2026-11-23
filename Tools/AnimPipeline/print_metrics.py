import json, sys
for t in sys.argv[1:]:
    r = json.load(open(f"sheets/{t}_run.json", encoding="utf-8-sig"))
    print(t, "| silhouettes s", r["sec_silhouettes"], "| lit+video s", r["sec_lit_and_video"], "| arm outside body % per azimuth")
    for f, row in r["metrics"].items():
        print(" f" + f.rjust(2), " ".join(f"{az}:{v['arm_out_pct_of_sil']:4.1f}" for az, v in row.items()))

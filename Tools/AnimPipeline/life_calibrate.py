"""Calibration proof of animpipe/life.py (owner 2026-11-24): every clip in life_calibration.json must give its expected
verdict, approved moves PASS with no exemption, known defects FAIL. A mismatch is a calibration error to FIX (threshold or
metric, with this run as proof), never a reason to soften a metric. Prints <= 5 lines; full table in
Saved/AnimPipeline/life_calibration_result.json. Exit 0 only on CALIB: OK (all present, all match).

Owner 2026-11-24 decision 2A: each metric blocks only with its own proof. `--write-gate` (only on a COMPLETE run, no clip
missing or broken) writes life_gate.json: a metric is blocking when every expect=PASS clip passes it AND at least one
expect=FAIL clip fails it; the others stay measures. The verdict above is computed on the RAW metrics (gate off)."""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from animpipe import life

PROJ = os.path.dirname(os.path.dirname(HERE))
DUMPS = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart")


def one(e):
    path = os.path.join(HERE, e["dump"]) if "dump" in e else os.path.join(DUMPS, f"joints_{e.get('asset', '')}.json")
    alt = os.path.join(PROJ, "docs", "notes", "life-calib-run", "dumps", os.path.basename(path))   # dumps committed by 1bc74996
    if not os.path.exists(path) and os.path.exists(alt):
        path = alt
    if not os.path.exists(path):
        return {"name": e["name"], "status": "MISSING", "path": path}
    kw = life.from_card(os.path.join(HERE, e["card"])) if "card" in e else {}
    for k in ("window", "contact", "effector"):
        if k in e:
            kw[k] = e[k]
    if "holds" in e:                           # holds declared by the slowmo review, not yet written in the (locked) card
        kw["holds"] = list(kw.get("holds", [])) + life.parse_holds(e["holds"])
    F = life.load_clip(path)     # dedupe only on .jsonl captures: on an asset dump repeated frames are a freeze
    r = life.run(F, gate=False, **kw)
    return {"name": e["name"], "status": "MATCH" if r["verdict"] == e["expect"] else "MISMATCH", "expect": e["expect"],
            "got": r["verdict"], "failed": {k: v[1] for k, v in r["block"].items() if not v[0]}, "why": e.get("why", ""),
            "metrics": {k: bool(v[0]) for k, v in r["block"].items()}}


def gate_rows(rows, man):
    """-> {metric: proof} for the metrics that pass every expect=PASS clip and fail at least one expect=FAIL clip."""
    exp = {e["name"]: e["expect"] for e in man["clips"]}
    names = sorted({k for r in rows for k in r.get("metrics", {})} - life.DECLARATION)
    out = {}
    for m in names:
        good = [r["name"] for r in rows if exp[r["name"]] == "PASS" and m in r["metrics"]]
        bad_pass = [n for n in good if not next(r for r in rows if r["name"] == n)["metrics"][m]]
        caught = [r["name"] for r in rows if exp[r["name"]] == "FAIL" and m in r["metrics"] and not r["metrics"][m]]
        if good and not bad_pass and caught:
            out[m] = {"passes": good, "fails_caught": caught}
    return out


def main():
    man = json.load(open(os.path.join(HERE, "life_calibration.json")))
    rows = []
    for e in man["clips"]:
        try:
            rows.append(one(e))
        except (ValueError, KeyError) as exc:  # a broken card or dump is reported, never counted as a match
            rows.append({"name": e["name"], "status": "ERROR", "error": str(exc)[:300]})
    out = os.path.join(PROJ, "Saved", "AnimPipeline", "life_calibration_result.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    json.dump(rows, open(out, "w"), indent=1)
    miss = [r["name"] for r in rows if r["status"] == "MISSING"]
    bad = [r["name"] for r in rows if r["status"] in ("MISMATCH", "ERROR")]
    verdict = "FAIL" if bad else ("INCOMPLETE" if miss else "OK")
    print(f"CALIB: {verdict} {sum(r['status'] == 'MATCH' for r in rows)}/{len(rows)} match")
    if bad:
        print("mismatch: " + ", ".join(bad))
    if miss:
        print("missing dumps: " + ", ".join(miss))
    print("table: " + out)
    if "--write-gate" in sys.argv[1:]:
        if miss or any(r["status"] == "ERROR" for r in rows):
            print("GATE: not written (incomplete run: every clip needs its dump)")
        else:
            g = gate_rows(rows, man)
            json.dump({"_doc": json.load(open(life.GATE_FILE)).get("_doc", ""), "calib": verdict,
                       "date": __import__("time").strftime("%Y-%m-%d"), "blocking": g}, open(life.GATE_FILE, "w"), indent=1)
            print(f"GATE: {len(g)} blocking metrics: {', '.join(sorted(g)) or 'none'} (commit life_gate.json)")
    return 0 if verdict == "OK" else 1


if __name__ == "__main__":
    sys.exit(main())

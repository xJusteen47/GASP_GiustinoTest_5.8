"""Writes the cost of a finished thread: one row in costs.csv and the COSTO line to paste at the end of the handover.

  python harness/scripts/python/cost_line.py --thread "Fix run start" --goal runstart --machine cloud --model Opus \
      --usd 4 [--desktop-min 0] [--review-tokens 0] [--outcome PASS]
costs.csv path: paths.costs_csv in kit.config.json (default handoffs/costi.csv). Prints at most 2 lines.
"""
import argparse, csv, datetime, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_config  # noqa: E402

HEADER = ["data", "thread", "obiettivo", "macchina", "modello", "minuti_desktop", "token_review", "usd_stimati", "esito"]

ap = argparse.ArgumentParser()
ap.add_argument("--thread", required=True)
ap.add_argument("--goal", required=True)
ap.add_argument("--machine", required=True, choices=["cloud", "desktop", "laptop", "other"])
ap.add_argument("--model", required=True)
ap.add_argument("--usd", type=float, required=True)
ap.add_argument("--desktop-min", type=float, default=0)
ap.add_argument("--review-tokens", default="0")
ap.add_argument("--outcome", default="PASS")
ap.add_argument("--date", default=datetime.date.today().isoformat())
a = ap.parse_args()

p = kit_config.get("paths.costs_csv", "handoffs/costi.csv")
p = p if os.path.isabs(p) else os.path.join(kit_config.root(), p)
os.makedirs(os.path.dirname(p), exist_ok=True)
new = not os.path.exists(p)
with open(p, "a", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    if new:
        w.writerow(HEADER)
    w.writerow([a.date, a.thread, a.goal, a.machine, a.model, f"{a.desktop_min:g}", a.review_tokens, f"{a.usd:g}", a.outcome])
print(f"COSTO: {a.usd:g} | {a.desktop_min:g} | {a.review_tokens}")
print(f"riga aggiunta a {p}")

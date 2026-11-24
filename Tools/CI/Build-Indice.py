"""Build handoffs/INDICE.md and handoffs/costi.csv from the handovers (owner decision 2A, 2026-11-24).

Nobody edits INDICE.md or costi.csv by hand any more: every branch used to touch both, so every merge made them diverge.
The only input is one handover per thread in docs/notes/handovers/*.md, whose closing lines are:

  ESITO: <PASS|PARZIALE|FAIL> <the goal or result in one sentence>  (OK and TIMEOUT are also accepted)
  SUBAGENTI: <how many, for what work, which model each>
  COSTO: <usd estimated> | <minutes of desktop> | <review tokens>

Optional lines: `BRANCH: <name> [<commit>]` (otherwise left empty), `OBIETTIVO:` (otherwise the ESITO goal, else the title).
Schema check (--check or always printed): a handover dated on or after --since (default 2026-11-25, the first day after the rule was set) must
carry the three lines in that shape; an older one that does not is kept as `legacy` (best-effort numbers, never an error), so the
history is not rewritten. The week total of USD is what the Friday comparison with real usage reads.

  python Tools/CI/Build-Indice.py                 write handoffs/INDICE.md and handoffs/costi.csv
  python Tools/CI/Build-Indice.py --check         only validate (exit 1 on a schema error), print at most 5 lines
  python Tools/CI/Build-Indice.py --handovers DIR [DIR...] --out-dir DIR
"""
import argparse
import csv
import datetime
import glob
import os
import re
import subprocess
import sys

ESITI = ("PASS", "OK", "PARZIALE", "FAIL", "TIMEOUT")
LINE = {k: re.compile(r"^[\s>*_`-]*%s\s*:\s*(.*?)\s*$" % k, re.M) for k in ("ESITO", "SUBAGENTI", "COSTO", "BRANCH", "OBIETTIVO")}
NUM = re.compile(r"\d+(?:[.,]\d+)?")
COLUMNS = ["data", "thread", "branch", "commit", "esito", "obiettivo", "subagenti", "usd", "minuti_desktop", "token_review", "schema"]


def last(rx, text):
    m = rx.findall(text)
    return m[-1].strip(" *`") if m else None


def num(s):
    """First number of a cost field: '~1,6 $ (stima)' -> 1.6; None when there is none."""
    m = NUM.search(s or "")
    return float(m.group(0).replace(",", ".")) if m else None


def parse_costo(s):
    """Strict shape '<usd> | <minutes> | <tokens>' -> (usd, min, tok, ok). Legacy text -> best-effort first number, ok False."""
    if s is None:
        return None, None, None, False
    parts = [p.strip() for p in s.split("|")]
    if len(parts) >= 3 and all(num(p) is not None for p in parts[:3]) and re.match(r"^~?\s*\d", parts[0]):
        return num(parts[0]), num(parts[1]), num(parts[2]), True
    return num(parts[0]) if parts else None, None, None, False


def parse_esito(s):
    """'ESITO: PASS una frase' or 'ESITO: OK | una frase' -> (word, goal, valid)."""
    if s is None:
        return None, None, False
    m = re.match(r"\s*([A-Za-z]+)\s*(?:\||:|-)?\s*(.*)$", s)
    word = m.group(1).upper() if m else ""
    goal = m.group(2).strip() if m else ""
    return (word if word in ESITI else s.strip()), goal, word in ESITI


def first_add_dates(handover_dir):
    """{path: (date, commit)} of the commit that added each handover, in one git pass (empty when git is not available)."""
    out = {}
    try:
        r = subprocess.run(["git", "log", "--diff-filter=A", "--name-only", "--format=@%h|%as", "--", handover_dir],
                           capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return out
    cur = None
    for l in r.stdout.splitlines():
        if l.startswith("@"):
            cur = tuple(l[1:].split("|"))[::-1]  # (date, commit)
        elif l.strip() and cur:
            out[os.path.normpath(l.strip())] = cur  # git lists newest first: the last assignment is the oldest add
    return out


def parse_handover(path, dates, since):
    text = open(path, encoding="utf-8-sig", errors="replace").read()
    slug = os.path.splitext(os.path.basename(path))[0]
    date, commit = dates.get(os.path.normpath(path), (None, ""))
    # The date the handover states about itself wins: many old handovers share the commit of one snapshot (17/11), whose date is
    # the snapshot's, not the thread's. Order: first date in the head of the text, date in the file name, commit date, mtime.
    m = re.search(r"20\d\d-\d\d-\d\d", text[:400]) or re.search(r"20\d\d-\d\d-\d\d", slug)
    if m:
        date = m.group(0)
    elif not date:
        date = datetime.date.fromtimestamp(os.path.getmtime(path)).isoformat()
    esito_raw, sub_raw, costo_raw = (last(LINE[k], text) for k in ("ESITO", "SUBAGENTI", "COSTO"))
    esito, goal, esito_ok = parse_esito(esito_raw)
    usd, minutes, tokens, costo_ok = parse_costo(costo_raw)
    title = re.search(r"^#\s+(.+)$", text, re.M)
    goal = goal or last(LINE["OBIETTIVO"], text) or (title.group(1).strip() if title else slug)
    branch = (last(LINE["BRANCH"], text) or "").split()
    errors = []
    if date >= since:
        if not esito_ok:
            errors.append("ESITO mancante o non nella forma 'ESITO: PASS|PARZIALE|FAIL frase'")
        if not sub_raw:
            errors.append("SUBAGENTI mancante")
        if not costo_ok:
            errors.append("COSTO non nella forma 'COSTO: <usd> | <minuti desktop> | <token review>'")
    schema = "ERRORE" if errors else ("ok" if (esito_ok and sub_raw and costo_ok) else "legacy")
    return {
        "data": date, "thread": slug, "branch": branch[0] if branch else "", "commit": branch[1] if len(branch) > 1 else commit,
        "esito": esito or "?", "obiettivo": goal.replace("\n", " ")[:200], "subagenti": (sub_raw or "").replace("\n", " ")[:160],
        "usd": "" if usd is None else ("%g" % usd), "minuti_desktop": "" if minutes is None else ("%g" % minutes),
        "token_review": "" if tokens is None else ("%g" % tokens), "schema": schema, "_errors": errors,
    }


def week_of(date):
    y, w, _ = datetime.date.fromisoformat(date).isocalendar()
    return "%d-W%02d" % (y, w)


def render_indice(rows):
    out = ["# Indice dei thread (handoffs/INDICE.md)", "",
           "> GENERATO da `Tools/CI/Build-Indice.py` dagli handover (`ESITO:` / `SUBAGENTI:` / `COSTO:`). Non modificare a mano: "
           "la modifica va nell'handover e il file si rigenera.", ""]
    weeks = {}
    for r in rows:
        if r["usd"] != "":
            weeks.setdefault(week_of(r["data"]), [0.0, 0])
            weeks[week_of(r["data"])][0] += float(r["usd"])
            weeks[week_of(r["data"])][1] += 1
    out += ["## Totale settimanale (USD stimati, solo handover con costo)", "", "| Settimana | Thread con costo | USD |", "|---|---|---|"]
    for wk in sorted(weeks, reverse=True):
        out.append("| %s | %d | %.1f |" % (wk, weeks[wk][1], weeks[wk][0]))
    out += ["", "## Thread (dal più recente)", "",
            "| Data | Thread | Branch | Commit | Esito | Obiettivo | USD | Min desktop | Schema |", "|---|---|---|---|---|---|---|---|---|"]
    for r in sorted(rows, key=lambda r: (r["data"], r["thread"]), reverse=True):
        out.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["data"], r["thread"], r["branch"], r["commit"], r["esito"], r["obiettivo"].replace("|", "/"),
            r["usd"], r["minuti_desktop"], r["schema"]))
    return "\n".join(out) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--handovers", nargs="+", default=["docs/notes/handovers"])
    ap.add_argument("--out-dir", default="handoffs")
    ap.add_argument("--since", default="2026-11-25", help="handovers from this date on must follow the schema")
    ap.add_argument("--check", action="store_true", help="validate only, write nothing")
    a = ap.parse_args(argv)
    rows = []
    for d in a.handovers:
        dates = first_add_dates(d)
        for p in sorted(glob.glob(os.path.join(d, "*.md"))):
            rows.append(parse_handover(p, dates, a.since))
    errs = [r for r in rows if r["_errors"]]
    if not a.check:
        os.makedirs(a.out_dir, exist_ok=True)
        with open(os.path.join(a.out_dir, "costi.csv"), "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=COLUMNS, extrasaction="ignore", lineterminator="\n")
            w.writeheader()
            w.writerows(sorted(rows, key=lambda r: (r["data"], r["thread"])))
        with open(os.path.join(a.out_dir, "INDICE.md"), "w", encoding="utf-8", newline="\n") as f:
            f.write(render_indice(rows))
    legacy = sum(1 for r in rows if r["schema"] == "legacy")
    print("INDICE: %d handover, %d ok, %d legacy, %d con errori di schema%s"
          % (len(rows), len(rows) - legacy - len(errs), legacy, len(errs), " (solo controllo)" if a.check else ""))
    for r in errs[:4]:
        print("ERRORE %s: %s" % (r["thread"], "; ".join(r["_errors"])))
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())

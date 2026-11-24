"""Principles the level-2 review must CONSIDER (INV-13): template and completeness check, 0 tokens.

Why (owner 2026-11-24): the animation principles and Sakurai's notes of the OG skill were only "loaded for the section you
cite", so nobody checked them; the owner also said they are principles to consider, not rules to apply like an automaton.
So this tool never judges the move and never compares a number to a threshold. It checks that every principle of
`contract/principi-review.md` was considered for this move with the four steps of principi-animazione-corporea.md §13
(intent, prediction, observed frames + view, deviation and whether it is intended), or closed as not pertinent with a reason.

CLI:  python principi_review.py template <out.md> [move]   writes the file to fill (the level-2 pack does it too)
      python principi_review.py check <file.md>            PRINCIPI: OK | INCOMPLETE (max 5 lines, rule 4)
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

CONTRACT = Path(__file__).resolve().parents[1] / "contract" / "principi-review.md"
FIELDS = ("intento", "previsione", "osservato", "scostamento", "voluto", "giudizio")
_ROW = re.compile(r"^\|\s*(PR-\d{2})\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*$")
_FRAME = re.compile(r"\b[fva]\d+\b")
_VIEW = re.compile(r"\b(az\d+|game|dietro|lato|camera)\b", re.I)
_PLACEHOLDER = re.compile(r"^(<.*>|\.\.\.|…|todo|tbd|da fare|-?)$", re.I)
_JUDGE = re.compile(r"^(coerente\b|difetto\s+(grave|medio|lieve)\s+(posa|dati montage|gioco)\b|non pertinente:\s*\S)", re.I)


def principles(contract: Path = CONTRACT) -> dict[str, dict[str, str]]:
    """{id: {title, questions, refs}} from the table of the contract (single source of the ids)."""
    out = {}
    for line in contract.read_text(encoding="utf-8").splitlines():
        m = _ROW.match(line)
        if m:
            out[m.group(1)] = {"title": m.group(2), "questions": m.group(3), "refs": m.group(4)}
    return out


def template(move: str = "<mossa>", contract: Path = CONTRACT) -> str:
    lines = [f"# Principi considerati: {move} (livello 2, INV-13)", "",
             "Principi, non regole (owner 2026-11-24): per ogni voce scrivi come l'hai considerato per QUESTA mossa.",
             "Metodo: principi-animazione-corporea.md §13. Formato e documenti: Tools/AnimPipeline/contract/principi-review.md.",
             "Guarda il video prima dei numeri (INV-02); i numeri dei controlli sono prova, non verdetto.", ""]
    for k, p in principles(contract).items():
        lines += [f"### {k} {p['title']}", f"<!-- {p['questions']} | {p['refs']} -->"]
        lines += [f"- {f}: <...>" for f in FIELDS] + [""]
    return "\n".join(lines)


def _entries(text: str) -> dict[str, dict[str, str]]:
    out: dict[str, dict[str, str]] = {}
    cur = None
    for line in text.splitlines():
        h = re.match(r"^#{2,4}\s*(PR-\d{2})\b", line)
        if h:
            cur = out.setdefault(h.group(1), {})
            continue
        f = re.match(r"^\s*[-*]\s*(\w+)\s*:\s*(.*)$", line)
        if cur is not None and f and f.group(1).lower() in FIELDS:
            cur[f.group(1).lower()] = f.group(2).strip()
    return out


def check(text: str, contract: Path = CONTRACT) -> list[str]:
    """Problems that mean a principle was not considered or not documented. [] = every principle considered.
    It checks the reasoning is written, never what the move measures."""
    ents = _entries(text)
    bad = []
    for k in principles(contract):
        e = ents.get(k)
        if e is None:
            bad.append(f"{k}: missing")
            continue
        g = e.get("giudizio", "")
        if not g or _PLACEHOLDER.match(g) or not _JUDGE.match(g):
            bad.append(f"{k}: giudizio must be 'coerente', 'difetto <grave|medio|lieve> <posa|dati montage|gioco>' "
                       f"or 'non pertinente: <motivo>'")
            continue
        if g.lower().startswith("non pertinente"):
            continue
        for f in ("intento", "previsione", "osservato", "scostamento"):
            v = e.get(f, "")
            if not v or _PLACEHOLDER.match(v):
                bad.append(f"{k}: {f} empty")
        obs = e.get("osservato", "")
        if obs and not (_FRAME.search(obs) and _VIEW.search(obs)):
            bad.append(f"{k}: osservato must cite a frame (f/v/a<n>) and a view")
        dev = e.get("scostamento", "").lower()
        if dev and not _PLACEHOLDER.match(dev) and not dev.startswith("nessun"):
            vol = e.get("voluto", "").lower()
            if not (vol.startswith("no") or re.match(r"^s[iì]\b.*\w{3,}", vol)):
                bad.append(f"{k}: a deviation needs 'voluto: si, <motivo>' or 'voluto: no'")
            elif vol.startswith("no") and g.lower().startswith("coerente"):
                bad.append(f"{k}: an unintended deviation is a 'difetto', not 'coerente'")
    return bad


def main(argv: list[str]) -> int:
    if len(argv) >= 3 and argv[1] == "template":
        Path(argv[2]).write_text(template(argv[3] if len(argv) > 3 else "<mossa>"), encoding="utf-8")
        print(f"PRINCIPI: template {argv[2]} ({len(principles())} voci)")
        return 0
    if len(argv) == 3 and argv[1] == "check":
        bad = check(Path(argv[2]).read_text(encoding="utf-8"))
        print(f"PRINCIPI: {'OK' if not bad else 'INCOMPLETE'} ({len(principles()) - len({b.split(':')[0] for b in bad})}"
              f"/{len(principles())} considerati)")
        for b in bad[:4]:
            print(b[:160])
        return 0 if not bad else 1
    print("usage: principi_review.py template <out.md> [move] | check <file.md>")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))

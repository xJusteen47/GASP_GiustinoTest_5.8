"""Visual review verdict (plan 03/11, skill .claude/skills/gstn-visual-review): the pipeline side.

Round folder: <Saved/AnimPipeline>/visual/<move>/r<N>/ with grid.png + manifest.json (written by `review --visual`, CP2)
and verdict.json (written by the reviewing Claude, schema .claude/skills/gstn-visual-review/verdict.schema.json):
  {move, round, esito: ok|correggi|chiedi_owner, problemi: [{regola, frame, parti, gravita: blocca|nota, pannello, prova,
   correzione: {frame, parte, direzione, quanto}}], segmenti: [{da, a, stato: ok|manca|brutto}]}

animpipe visual-gate <card> [--round=N]   newest round by default; exit 0 = may approve/deliver, 1 = blocked, 2 = no
verdict. Blocked when esito != ok or any problem is `blocca`. After round MAX_ROUNDS still blocked: stop, hand the owner
the image + verdict (no round 3). `approve` calls blocked() and refuses while the newest verdict blocks.
"""
import glob, json, os, re

from animpipe import card as C

MAX_ROUNDS = 2
ESITI = ("ok", "correggi", "chiedi_owner")
GRAVITA = ("blocca", "nota")
STATI = ("ok", "manca", "brutto")


def root():
    return os.path.join(C.REFS if os.path.isdir(C.REFS) else os.path.join(C.TOOLS, "sheets"), "visual")


def rounds(move):
    out = []
    for d in glob.glob(os.path.join(root(), move, "r*")):
        m = re.fullmatch(r"r(\d+)", os.path.basename(d))
        if m and os.path.exists(os.path.join(d, "verdict.json")):
            out.append(int(m.group(1)))
    return sorted(out)


def validate(v, move=None):
    """[error] for a verdict dict (the schema's required fields and enums; no jsonschema dependency)."""
    e = []
    for k in ("move", "round", "esito", "problemi", "segmenti"):
        if k not in v:
            e.append(f"missing {k}")
    if v.get("esito") not in ESITI:
        e.append(f"esito {v.get('esito')!r} not in {ESITI}")
    if move and v.get("move") != move:
        e.append(f"move {v.get('move')!r} != card {move!r}")
    for i, p in enumerate(v.get("problemi", [])):
        for k in ("regola", "frame", "parti", "gravita", "prova"):
            if k not in p:
                e.append(f"problemi[{i}] missing {k}")
        if p.get("gravita") not in GRAVITA:
            e.append(f"problemi[{i}].gravita {p.get('gravita')!r}")
    for i, s in enumerate(v.get("segmenti", [])):
        if s.get("stato") not in STATI:
            e.append(f"segmenti[{i}].stato {s.get('stato')!r}")
    if v.get("esito") == "ok" and any(p.get("gravita") == "blocca" for p in v.get("problemi", [])):
        e.append("esito ok with a 'blocca' problem")
    return e


def load_verdict(move, rnd=None):
    rs = rounds(move)
    if not rs:
        return None, None
    rnd = int(rnd) if rnd else rs[-1]
    p = os.path.join(root(), move, f"r{rnd}", "verdict.json")
    return (json.load(open(p, encoding="utf-8")) if os.path.exists(p) else None), rnd


def blocked(move, rnd=None):
    """None = no verdict; else (is_blocked, line)."""
    v, rnd = load_verdict(move, rnd)
    if v is None:
        return None
    err = validate(v, move)
    if err:
        return True, f"r{rnd} verdict invalid: {'; '.join(err[:3])}"
    hard = [p for p in v["problemi"] if p["gravita"] == "blocca"]
    bad = [f"{s['da']}-{s['a']} {s['stato']}" for s in v["segmenti"] if s["stato"] != "ok"]
    is_b = v["esito"] != "ok" or bool(hard)
    line = f"r{rnd} esito {v['esito']}, blocca {len(hard)}, note {len(v['problemi']) - len(hard)}" + (f", segmenti {', '.join(bad)}" if bad else "")
    return is_b, line


def gate(path, rnd=None):
    c = C.load(path)
    b = blocked(c["move"], rnd)
    if b is None:
        print(f"visual-gate {c['move']}: no verdict in {os.path.join(root(), c['move'])} (run the review first)")
        return 2
    is_b, line = b
    v, r = load_verdict(c["move"], rnd)
    print(f"visual-gate {c['move']}: {'BLOCKED' if is_b else 'PASS'} | {line}")
    if is_b and v:
        for p in [p for p in v.get("problemi", []) if p.get("gravita") == "blocca"][:2]:
            print(f"  {p['regola']} f{p['frame']} {','.join(p['parti']) if isinstance(p['parti'], list) else p['parti']}: {p['prova'][:90]}")
        if v.get("esito") == "chiedi_owner":
            print("  esito chiedi_owner: an owner key is involved, ask the owner (never edit his keys)")
        elif r >= MAX_ROUNDS:
            print(f"  round {r} of {MAX_ROUNDS}: stop, hand the owner grid.png + verdict.json (no round {r + 1})")
    return 1 if is_b else 0

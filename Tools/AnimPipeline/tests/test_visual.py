"""visual-gate (animpipe/visual.py) on synthetic verdicts in a temp root. Run: python tests/test_visual.py (or pytest)."""
import json, os, shutil, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import visual as V

SCHEMA = os.path.join(os.path.dirname(__file__), "..", "..", "..", ".claude", "skills", "animpipe-move", "verdict.schema.json")


def _with(verdicts):
    d = tempfile.mkdtemp(); V.root = lambda: d
    for r, v in verdicts.items():
        os.makedirs(os.path.join(d, "m", f"r{r}"))
        json.dump(v, open(os.path.join(d, "m", f"r{r}", "verdict.json"), "w"))
    return d


def _v(esito, probs=(), segs=({"da": 0, "a": 5, "stato": "ok"},), r=1):
    return {"move": "m", "round": r, "esito": esito, "problemi": list(probs), "segmenti": list(segs)}


P_BLOCK = {"regola": "sinergia", "frame": [6, 12], "parti": ["arm_l"], "gravita": "blocca", "prova": "lato f6-12: braccia ferme"}


def test_schema_enums_match_module():
    s = json.load(open(SCHEMA))["properties"]
    assert tuple(s["esito"]["enum"]) == V.ESITI
    assert tuple(s["problemi"]["items"]["properties"]["gravita"]["enum"]) == V.GRAVITA
    assert tuple(s["segmenti"]["items"]["properties"]["stato"]["enum"]) == V.STATI


def test_none_ok_blocked_newest():
    d = _with({})
    try:
        assert V.blocked("m") is None
    finally:
        shutil.rmtree(d)
    d = _with({1: _v("correggi", [P_BLOCK]), 2: _v("ok", [dict(P_BLOCK, gravita="nota")], r=2)})
    try:
        assert V.blocked("m")[0] is False                      # newest round wins
        assert V.blocked("m", 1)[0] is True
    finally:
        shutil.rmtree(d)


def test_invalid_and_inconsistent():
    assert V.validate(_v("ok", [P_BLOCK])) == ["esito ok with a 'blocca' problem"]
    assert any("esito" in e for e in V.validate(_v("boh")))
    assert any("stato" in e for e in V.validate(_v("ok", segs=[{"da": 0, "a": 1, "stato": "x"}])))
    assert V.validate(_v("ok"), move="other")


if __name__ == "__main__":
    for n, f in sorted(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)

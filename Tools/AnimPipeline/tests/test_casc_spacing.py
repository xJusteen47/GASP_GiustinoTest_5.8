"""Offline tests of animpipe/spacing.py (casc-spacing), no editor and no Cascadeur.

    python -m pytest Tools/AnimPipeline/tests/test_casc_spacing.py -q   (or run the file)
"""
import os, sys
import numpy as np

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)
from animpipe import spacing as S
from animpipe.cascadeur import RESID_BONES

KEYS = [0, 4, 10]


def _clip(fn):
    return {f: {b: np.array(fn(f, i)) for i, b in enumerate(RESID_BONES)} for f in range(11)}


def test_axes_round_trip():
    p = [1.0, 2.0, 3.0]
    assert np.allclose(S.from_casc(S.to_casc(np.array(p))), p)
    assert S.to_casc(np.array(p)) == [1.0, 3.0, 2.0]          # Cascadeur is Y-up: (x, z, y)


def test_key_gap_removed():
    """A constant key-pose gap between Cascadeur and the build is not a spacing error."""
    build = _clip(lambda f, i: [f * 2.0, i, 0.0])
    casc = _clip(lambda f, i: [f * 2.0 + 3.0, i, 1.0])        # same spacing, every pose 3 cm / 1 cm off
    tgt = S.target(build, {k: casc[k] for k in KEYS}, KEYS)
    sc = S.score(casc, tgt)
    assert set(sc) == {1, 2, 3, 5, 6, 7, 8, 9}               # in-betweens only
    assert max(v[0] for v in sc.values()) < 1e-9


def test_spacing_error_measured():
    """Same keys, different timing: linear in Cascadeur vs an eased build shows up on the in-betweens."""
    ease = lambda t: t * t * (3 - 2 * t)
    build = _clip(lambda f, i: [10.0 * (ease(f / 4) if f <= 4 else 1 + ease((f - 4) / 6)), 0.0, 0.0])
    casc = _clip(lambda f, i: [10.0 * (f / 4 if f <= 4 else 1 + (f - 4) / 6), 0.0, 0.0])
    tgt = S.target(build, {k: casc[k] for k in KEYS}, KEYS)
    rep = S.segment_report(S.score(casc, tgt), KEYS)
    assert set(rep) == {"0-4", "4-10"}
    assert 0.5 < rep["0-4"] < 1.5 and rep["4-10"] > 0.5


def test_candidates_and_layers():
    c = S.candidates()
    assert c[0] == {"interp": "LINEAR"} and len(c) == 1 + 3 * len(S.GRID) ** 2
    assert all(set(x) <= {"interp", "rw", "lw"} for x in c)   # never anything that moves a key
    assert "Fingers_L" in S.SKIP_LAYERS


def test_part_delays_refused(tmp_path):
    import json, shutil
    src = os.path.join(TOOLS, "cards", "overlord", "overlord_dair_v4.json")
    card = json.load(open(src, encoding="utf-8"))
    card["part_delays"] = {"head": 1}
    p = tmp_path / "card.json"
    p.write_text(json.dumps(card), encoding="utf-8")
    try:
        S.plan_cmd(str(p), {"score": True})
    except SystemExit as e:
        assert "part_delays" in str(e)
    else:
        raise AssertionError("a card with part_delays must be refused")


if __name__ == "__main__":
    import pytest
    sys.exit(pytest.main([__file__, "-q"]))

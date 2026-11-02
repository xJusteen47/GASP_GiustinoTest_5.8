"""capture + diff manual line (no editor): a synthetic layer backup with hand_r_fk_ctrl keys at frame 5 is copied into a temp
card folder, the card gets a `manual` block with parts {"arm_r": [5]}, and diff warns when arm_r changed at 5 (or 4/6).
Never touches the real cards. Run: python tests/test_capture.py"""
import contextlib, io, json, os, shutil, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import card as C, locks as L

V4 = os.path.join(C.TOOLS, "cards", "overlord", "overlord_dair_v4.json")


def _setup():
    """Temp kit folder with a v1 and v2 copy of v4 (no approved block), a layer backup; v2 differs from v1 by nothing yet."""
    d = tempfile.mkdtemp(dir=os.path.join(C.TOOLS, "cards", "overlord"))
    c = json.load(open(V4)); c.pop("approved", None); c["move"] = "tmp_cap_v2"
    c["author"]["poses"].setdefault("5", {"arm_r": {"hand": [0, 0, 0], "pole": [0, 0, 0]}})
    c["author"]["poses"]["5"].setdefault("arm_r", {"hand": [0, 0, 0], "pole": [0, 0, 0]})
    old = json.loads(json.dumps(c)); old["move"] = "tmp_cap_v1"
    p1, p2 = os.path.join(d, "tmp_cap_v1.json"), os.path.join(d, "tmp_cap_v2.json")
    json.dump(old, open(p1, "w")); json.dump(c, open(p2, "w"))
    layer = os.path.join(d, "layer.json")
    json.dump({"hand_r_fk_ctrl.Rotation.X": [[5, 12.0, "RCIM_CUBIC", "RCTM_AUTO", 0.0, 0.0]]}, open(layer, "w"))
    return d, p1, p2, layer


def _run(f, *a):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        f(*a)
    return buf.getvalue()


def test_capture_and_diff():
    d, p1, p2, layer = _setup()
    try:
        out = _run(L.capture, p2, layer)
        assert len(out.splitlines()) <= 5 and "arm_r 5" in out, out
        c = json.load(open(p2))
        m = c["manual"]
        assert m["file"] == "manual/tmp_cap_v2.json" and m["parts"] == {"arm_r": [5]} and m["keys"] == 1 and m["channels"] == 1, m
        assert json.load(open(os.path.join(d, m["file"]))) == json.load(open(layer))
        assert L.manual_path(c, p2) == os.path.join(d, "manual", "tmp_cap_v2.json")
        out = _run(L.diff, p2)
        assert "manuale: intatto" in out, out
        c["author"]["poses"]["5"]["arm_r"]["hand"][0] += 5       # arm_r moves at frame 5 vs v1
        json.dump(c, open(p2, "w"))
        out = _run(L.diff, p2)
        assert "manuale sotto una posa cambiata: arm_r 5 (ricontrollare)" in out and len(out.splitlines()) <= 5, out
        assert L.ctrl_part("foot_l_ik_ctrl.Location.Z") == "leg_l" and L.ctrl_part("zzz") is None
    finally:
        shutil.rmtree(d)
        for n in ("tmp_cap_v2_diff.json",):
            q = os.path.join(L.out_dir(), n)
            os.path.exists(q) and os.remove(q)


if __name__ == "__main__":
    for n, f in sorted(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)

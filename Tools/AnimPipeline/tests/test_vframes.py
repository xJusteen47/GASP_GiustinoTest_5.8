"""Video frames, verify and the standalone plumbing, no engine. Run: python tests/test_vframes.py (or pytest)."""
import json, os, sys, tempfile

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS); sys.path.insert(0, os.path.join(TOOLS, "tests"))
from animpipe import vframes as V, verify as VF, standalone as SA


def test_mapping():
    a0, hit = 10, 8
    assert [V.video_to_anim(v, a0, hit) for v in (0, 10, 11, 18, 19, 20)] == [0, 10, 10, 10, 11, 12]
    assert V.anim_to_video(10, a0, hit) == 10 and V.anim_to_video(11, a0, hit) == 19
    for f in range(0, 40):                                  # round trip outside the freeze
        assert V.video_to_anim(V.anim_to_video(f, a0, hit), a0, hit) == f
    try:
        V.to_anim_list([12], a0, hit); assert False
    except ValueError as e:
        assert "freeze" in str(e)
    assert V.parse("v13, 14") == [13, 14]


def test_frame_map_lines():
    c = {"poses": [{"frame": 0}, {"frame": 11}, {"frame": 14}]}
    rows = V.frame_map(c, [11, 13], 10, 8, owner={11})
    assert rows[0]["key"] and rows[0]["owner_key"] and rows[1]["segment"] == [11, 14] and not rows[1]["key"]
    assert "owner KEY 11" in V.line(rows[0]) and "between keys 11 and 14" in V.line(rows[1])
    assert rows[0]["video"] == 19                           # after the freeze: +hit


def _dump():
    F = {}
    for f in range(0, 50):                                  # torso leans 20 deg at f17 only; hand_b peaks 2 frames after hand_a
        lean = 20 if f == 17 else 0
        import math
        F[str(f)] = {"pelvis": [0, 0, 100], "spine_05": [0, 50 * math.sin(math.radians(lean)), 100 + 50 * math.cos(math.radians(lean))],
                     "hand_a": [0, 0, 100 + (10 if f == 20 else 0)], "hand_b": [0, 0, 100 + (10 if f == 22 else 0)]}
    return F


def test_verify():
    spec = {"video": False, "reqs": [
        {"id": "lean", "type": "angle", "from": "pelvis", "to": "spine_05", "frame": 17, "min": 18, "max": 22},
        {"id": "lag", "type": "delay", "a": "hand_a", "b": "hand_b", "window": [17, 40], "min": 1, "max": 3},
        {"id": "amp", "type": "amplitude", "bone": "hand_a", "coord": "z", "window": [17, 40], "min": 20},
        {"id": "gone", "type": "angle", "from": "pelvis", "to": "nope", "frame": 17}]}
    rows = {r["id"]: r for r in VF.check(_dump(), spec)}
    assert rows["lean"]["ok"] and abs(rows["lean"]["value"] - 20) < 0.01
    assert rows["lag"]["ok"] and rows["lag"]["value"] == 2
    assert not rows["amp"]["ok"] and rows["amp"]["value"] == 10
    assert not rows["gone"]["ok"] and rows["gone"]["line"].startswith("FAIL gone")
    spec["video"] = True                                    # video frames: with a0=10, hit=8, v25 = anim 17
    spec["reqs"] = [{"id": "lean", "type": "angle", "from": "pelvis", "to": "spine_05", "frame": 25, "min": 18, "max": 22}]
    assert VF.check(_dump(), spec, a0=10, hit=8)[0]["ok"]


def test_standalone_args():
    a = SA.build_args("UE.exe", "P/x.uproject", "log.txt")
    assert a[1].endswith(".uproject") and a[2] == SA.MAP and "-nullrhi" not in a and "-RenderOffScreen" in a
    assert "ANIMPIPE_BOOT" in a[3] and '"' not in a[3]      # the path travels in the environment, no inner quote


def test_convert():
    try:
        from PIL import Image
    except ImportError:
        return
    d = tempfile.mkdtemp()
    os.makedirs(os.path.join(d, "90"))
    json.dump({"n": 1}, open(os.path.join(d, "manifest.json"), "w"))
    for f in (0, 1):
        open(os.path.join(d, "90", "%03d.rgb" % f), "wb").write(bytes([f * 100, 5, 6]) * (SA.SIZE * SA.SIZE))
    dest = os.path.join(d, "png")
    assert SA.convert(d, [90], lambda a: dest) == 2
    assert Image.open(os.path.join(dest, "001.png")).getpixel((3, 3)) == (100, 5, 6)


def test_verbs_frame_map():
    import test_verbs as T
    root, data, cards = T._setup()
    env = T._env(data, cards)
    o = T._ap(env, "approve", "m", "--frames=11", "--video", "--parts=arm_l")   # refused (no owner ok) but the map is printed first
    fm = o["result"].get("frame_map")
    assert fm and fm[0].startswith("v11 = anim 11"), o
    o = T._ap(env, "verify", "m")
    assert o["exit"] == 2


if __name__ == "__main__":
    for n, f in sorted(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)

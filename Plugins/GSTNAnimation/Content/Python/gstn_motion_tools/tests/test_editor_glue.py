"""Tests of the editor glue that do not need the editor: channel-name parsing, key thinning and the panel's base64
JSON round trip. A stub `unreal` module stands in for the editor's."""
import base64
import json
import sys
import types
import unittest


def _install_stub_unreal():
    if "unreal" in sys.modules:
        return
    u = types.ModuleType("unreal")
    u.MovieSceneTimeUnit = types.SimpleNamespace(DISPLAY_RATE="display")
    u.MovieSceneKeyInterpolation = types.SimpleNamespace(AUTO="auto")
    u.FrameNumber = lambda f: f
    sys.modules["unreal"] = u


_install_stub_unreal()
from gstn_motion_tools import panel, sequencer_io  # noqa: E402


class FakeKey:
    def __init__(self, f):
        self.f = f

    def get_time(self, unit):
        return types.SimpleNamespace(frame_number=types.SimpleNamespace(value=self.f))


class FakeChannel:
    def __init__(self, name):
        self.channel_name = name
        self.keys = []
        self.default = None

    def get_keys(self):
        return list(self.keys)

    def remove_key(self, k):
        self.keys.remove(k)

    def set_default(self, v):
        self.default = v

    def add_key(self, frame, value, sub, unit, interp):
        self.keys.append(FakeKey(frame))


class GlueTests(unittest.TestCase):
    def test_parse_channel_names(self):
        p = sequencer_io._parse_channel_name
        self.assertEqual(p("head_ctrl.Rotation.X"), ("head_ctrl", "rot", 0))
        self.assertEqual(p("head_ctrl.Rotation.Yaw"), ("head_ctrl", "rot", 2))
        self.assertEqual(p("spine.01.Location.Y"), ("spine.01", "loc", 1))
        self.assertIsNone(p("Weight"))
        self.assertIsNone(p("ik_switch"))

    def test_write_thins_zero_runs(self):
        ch = FakeChannel("hand_l_fk_ctrl.Rotation.Y")
        sec = types.SimpleNamespace(get_all_channels=lambda: [ch])
        b = sequencer_io.UnrealBackend.__new__(sequencer_io.UnrealBackend)
        b._layer_section = lambda layer: sec
        values = [0, 0, 0, 0, 1.0, 2.0, 0, 0, 0, 0]
        b.write("Overlap", "hand_l_fk_ctrl", "rot", 1, list(range(10)), values)
        frames = sorted(k.f for k in ch.keys)
        self.assertEqual(frames, [0, 3, 4, 5, 6, 9])          # ends + the zeros that bound the non-zero stretch
        self.assertEqual(ch.default, 0.0)

    def test_panel_round_trip(self):
        req = base64.b64encode(json.dumps({"op": "presets"}).encode()).decode()
        out = json.loads(base64.b64decode(panel.run(req)))
        self.assertTrue(out["ok"])
        self.assertIn("frusta", [p["id"] for p in out["presets"]["overlap"]])
        bad = json.loads(base64.b64decode(panel.run(base64.b64encode(b'{"op": "boh"}').decode())))
        self.assertFalse(bad["ok"])
        self.assertIn("boh", bad["error"])


if __name__ == "__main__":
    unittest.main()

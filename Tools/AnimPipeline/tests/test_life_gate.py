"""Skill audit 2026-11-23 option A: anti-stop gate on every clip (settle/loop too, inherited too), STOP marks and speed strip on
the velocity sheets, startup slowmo tile and pack order (nothing of the final review is cut). Offline."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "gstn_anim_mcp"))
sys.path.insert(0, str(HERE))

import capture_move  # noqa: E402
import motion_overlay  # noqa: E402
import no_stop_gate  # noqa: E402
import review_pack  # noqa: E402
from test_t0_prep import BASE, VAR, CLIP_BASE, CLIP_VAR, FakeRunner, TestCaptureMove, _make_repo, _runs, _write_joints  # noqa: E402


def _body(pelvis_z):
    """Joints where every point follows the pelvis (limbs still relative to it) plus a hand that swings 6 cm/frame."""
    n = len(pelvis_z)
    j = {"pelvis": [[0.0, 95.0, z] for z in pelvis_z], "head": [[0.0, 165.0, z] for z in pelvis_z]}
    for s, x in (("l", 15.0), ("r", -15.0)):
        j[f"foot_MainPoint_{s}"] = [[x, 10.0, z] for z in pelvis_z]
        j[f"lowerarm_MainPoint_{s}"] = [[x, 115.0, z] for z in pelvis_z]
        j[f"hand_MainPoint_{s}"] = [[x, 95.0, z] for z in pelvis_z]
    return j


SETTLE_STOP = [5.0 * min(i, 6) + 5.0 * max(0, i - 12) for i in range(20)]   # still on steps 6..11 (6 frames), then moves again


class TestNoStopGate(unittest.TestCase):
    def test_settle_stop_blocks_and_hold_declared_passes(self):
        r = no_stop_gate.check(_body(SETTLE_STOP))
        self.assertFalse(r["ok"])
        self.assertEqual(r["still_runs"], [(6, 11)])
        self.assertEqual(r["stills"], list(range(6, 12)))
        self.assertTrue(no_stop_gate.check(_body(SETTLE_STOP), holds="6-12")["ok"])
        self.assertTrue(no_stop_gate.check(_body([5.0 * i for i in range(20)]))["ok"])

    def test_startup_window_blocks_a_single_still_step(self):
        z = [5.0 * i for i in range(8)] + [35.0] + [35.0 + 5.0 * i for i in range(1, 11)]   # one still step 7->8
        self.assertTrue(no_stop_gate.check(_body(z))["ok"])            # a single still step is not a run in a settle
        self.assertFalse(no_stop_gate.check(_body(z), startup="0-12")["ok"])   # but the startup allows none

    def test_no_pelvis_is_a_failure_not_a_pass(self):
        self.assertFalse(no_stop_gate.check({"head": [[0, 0, i] for i in range(5)]})["ok"])

    def test_cli(self):
        with tempfile.TemporaryDirectory() as t:
            p = Path(t) / "j.json"
            p.write_text(json.dumps({"fps": 30, "joints": _body(SETTLE_STOP)}))
            self.assertEqual(no_stop_gate.main([str(p)]), 1)
            self.assertEqual(no_stop_gate.main([str(p), "--holds=6-12"]), 0)


class TestGateOnEveryClip(unittest.TestCase):
    def test_inherited_settle_is_rejudged_and_blocks(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            _write_joints(root, "AS_K6_SettleShared", _body(SETTLE_STOP))
            shared = "/Game/Anim/K6/AS_K6_SettleShared"
            g = capture_move.precapture_gate(root, VAR, BASE, h, [CLIP_VAR, shared], (0, 19), "settle", hitbox_required=False,
                                             base_clips=[CLIP_BASE, shared])
            ns = next(c for c in g["checks"] if c["check"] == "no_stop")
            self.assertFalse(g["ok"])
            self.assertFalse(ns["ok"])
            self.assertEqual(ns["clips_failed"], ["AS_K6_SettleShared"])
            self.assertIn("ereditato", ns["msg"])
            # the owner-declared hold is the only way through
            g2 = capture_move.precapture_gate(root, VAR, BASE, h, [CLIP_VAR, shared], (0, 19), "settle", hitbox_required=False,
                                              base_clips=[CLIP_BASE, shared], no_stop={"holds": {"AS_K6_SettleShared": "6-12"}})
            self.assertTrue(g2["ok"], g2["error"])

    def test_missing_dump_blocks(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            g = capture_move.precapture_gate(root, VAR, BASE, h, [CLIP_VAR, "/Game/X/AS_Loop"], (0, 19), "settle",
                                             hitbox_required=False)
            self.assertFalse(g["ok"])
            self.assertIn("AS_Loop: joints dump missing", g["error"])

    def test_stop_and_go_never_inherited(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            (root / "docs/notes").mkdir(parents=True)
            (root / "docs/notes/anim-approvate.md").write_text("AS_Base K7\n")
            chk = root / "base_check.json"
            chk.write_text(json.dumps({"block": {"stop_and_go": {"ok": False, "line": "L"}, "contact_faf": {"ok": False, "line": "C"}}}))
            inh, _ = capture_move.inherited_blocks(root, {"stop_and_go": "L", "contact_faf": "C"}, "AS_Base", "x.json", chk)
            self.assertNotIn("stop_and_go", inh)


class TestLifeImages(unittest.TestCase):
    def test_velocity_sheet_marks_stops_and_has_speed_strip(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            _write_joints(root, "AS_stop", _body(SETTLE_STOP))
            r = motion_overlay.anim_motion_overlay("AS_stop", None, "az90", root=root, out="rev", prefix="velocity_v")
            self.assertTrue(r["ok"], r)
            self.assertEqual(r["stops"], list(range(6, 12)))
            self.assertEqual(r["still_runs_blocking"], [[6, 12]])
            self.assertIn("stop steps", r["summary"][0])
            from PIL import Image
            im = Image.open(root / "rev" / "velocity_v_az90.png")
            self.assertEqual(im.size, (720, 560 + 190))
            # the strip has the STOP band (light red) and the trails area has the STOP dots (red)
            strip = im.crop((0, 560, 720, 750)).getcolors(1 << 20)
            self.assertTrue(any(c == (255, 205, 205) for _, c in strip))
            self.assertTrue(any(c == (230, 0, 0) for _, c in im.crop((0, 0, 720, 560)).getcolors(1 << 20)))

    def test_startup_frames_every_tick(self):
        self.assertEqual(review_pack.startup_frames((95, 124), 110), list(range(95, 110)))
        self.assertEqual(review_pack.startup_frames((0, 124), 110, 24), list(range(86, 110)))

    def test_order_pack_never_trims(self):
        o = review_pack.order_pack(["s"], ["m1", "m2"], [f"g{i}" for i in range(6)])
        self.assertEqual(o["images"][:3], ["s", "m1", "m2"])
        self.assertEqual(o["count"], 9)
        self.assertTrue(o["over_budget"])
        self.assertFalse(review_pack.order_pack(["s"], ["m1", "m2"], ["a", "b", "c"])["over_budget"])

    def test_capture_writes_life_pack(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            r = capture_move.anim_capture_move(variant_runs=_runs(h), root=root, runner=FakeRunner(root), **TestCaptureMove.KW)
            self.assertTrue(r["ok"], r)
            out = root / "docs/notes/reviews/dk6/ft1"
            pack = json.loads((out / "pack.json").read_text())
            names = [Path(p).name for p in pack["images"]]
            self.assertEqual(names[:3], ["slowmo_dropkick_hit_az90.png", "velocity_ft_az90.png", "velocity_ft_az180.png"])
            self.assertEqual(len([n for n in names if n.startswith("grid_")]), 3)   # every grid kept
            self.assertEqual(pack["count"], 6)
            self.assertFalse(pack["over_budget"])
            self.assertIn("docs/notes/reviews/dk6/ft1/ft_dropkick_hit_slow4x.mp4", pack["slowmo_mp4"])
            self.assertEqual(pack["errors"], [])
            self.assertTrue((out / "slowmo_dropkick_hit_az90.png").exists())


if __name__ == "__main__":
    unittest.main()

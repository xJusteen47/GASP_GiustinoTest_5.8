"""Offline synthetic tests for the follow_through intent (pure Python, no UE)."""
from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path

MCP_DIR = Path(__file__).resolve().parents[1] / "gstn_anim_mcp"
if str(MCP_DIR) not in sys.path:
    sys.path.insert(0, str(MCP_DIR))

import anim_ops  # noqa: E402
import follow_through as ft  # noqa: E402

N = 60
HIT = 24


def _swing_track(amp_deg: float = 60.0, axis=(0.0, 1.0, 0.0)) -> list:
    """A limb that swings in, STOPS at HIT, holds, then recovers slowly (the authored clip)."""
    out = []
    for f in range(N):
        ang = -math.radians(amp_deg) * (1.0 - ft.ss(HIT - 6, HIT, f)) + math.radians(amp_deg) * 0.4 * ft.ss(HIT + 12, HIT + 30, f)
        out.append(ft.qexp((axis[0] * ang, axis[1] * ang, axis[2] * ang)))
    return out


def _clip(bones) -> dict:
    return {b: _swing_track() for b in bones}


def _cfg(**kw):
    base = {"chain": "arm_l", "anchor": "hit", "hit_frame": HIT}
    base.update(kw)
    return ft.validate_follow_through(base)


class TestFollowThrough(unittest.TestCase):
    def test_overshoot_maps_to_damping_exactly(self) -> None:
        for pct in (5, 25, 50, 80):
            self.assertAlmostEqual(ft.overshoot_from_zeta(ft.zeta_from_overshoot(pct)), pct, places=6)
        self.assertEqual(ft.zeta_from_overshoot(0), 1.0)

    def test_chain_resolution_and_refusals(self) -> None:
        c = ft.resolve_chain("shoulder>elbow>wrist>fingers", "l")
        self.assertEqual(c[0]["links"][0], ["clavicle_l"])
        self.assertEqual(c[0]["links"][3][0], "thumb_01_l")
        self.assertEqual(len(ft.resolve_chain("arms", None)), 2)
        self.assertEqual(ft.resolve_chain("spine_head", None)[0]["links"][-1], ["head"])
        for bad in ("pelvis>spine_01", "thigh_l>calf_l", "root", "ik_foot_l", "upperarm_l>foot_l"):
            with self.assertRaises(ValueError, msg=bad):
                ft.resolve_chain(bad, "l")

    def test_validation(self) -> None:
        for bad in (
            {"chain": "arm_l"},  # no anchor
            {"chain": "arm_l", "anchor": "hit"},  # hit without hit_frame
            {"chain": "arm_l", "anchor": 10, "frame_range": [10, 20]},
            {"chain": "arm_l", "anchor": 10, "overshoot_pct": 20, "damping": 0.4},
            {"chain": "arm_l", "anchor": 10, "overshoot_pct": 200},
            {"chain": "arm_l", "anchor": 10, "mass": "tiny"},
            {"chain": "arm_l", "anchor": 10, "bogus": 1},
        ):
            with self.assertRaises(ValueError, msg=str(bad)):
                ft.validate_follow_through(bad)
        c = ft.validate_follow_through({"chain": "arm_l", "frame_range": [20, 36]})
        self.assertEqual((c["anchor_frame"], c["settle_frames"]), (20, 16))

    def test_mass_heavier_is_slower_and_later(self) -> None:
        light, heavy = _cfg(mass="light"), _cfg(mass="heavy")
        self.assertLess(ft.link_period(light, 0), ft.link_period(heavy, 0))
        self.assertLess(ft.link_delay(light, 2), ft.link_delay(heavy, 2))
        self.assertGreater(ft.default_settle_frames(heavy), ft.default_settle_frames(light) - 1)

    def test_anchor_pose_bit_identical_and_outside_window_untouched(self) -> None:
        c = _cfg(chain="arm_fingers_l", mass="normal", overshoot_pct=35)
        bones = [b for ch in c["chains"] for l in ch["links"] for b in l]
        rot = _clip(bones)
        new, rep = ft.solve_chain(rot, c, N)
        a, e = rep["window"]
        for b in bones:
            self.assertEqual(new[b][a], rot[b][a])  # exact: the hit frame does not move
            for f in list(range(0, a + 1)) + list(range(e, N)):
                self.assertEqual(new[b][f], rot[b][f])
        cons = ft.conservation_report(rot, new, bones, a, e)
        self.assertTrue(cons["ok"], cons)
        self.assertTrue(cons["anchor_frame_unchanged"])

    def test_it_actually_moves_and_overshoot_scales_with_param(self) -> None:
        peaks = []
        for pct in (5, 30, 60):
            c = _cfg(overshoot_pct=pct, delay_frames=0.0)
            rot = _clip(["upperarm_l", "lowerarm_l", "hand_l"])
            _, rep = ft.solve_chain(rot, c, N)
            peaks.append(max(l["peak_deg"] for l in rep["links"]))
        self.assertGreater(peaks[0], 0.5)
        self.assertLess(peaks[0], peaks[1] + 1e-9)
        self.assertLess(peaks[1], peaks[2] + 1e-9)

    def test_delay_lags_distal_links(self) -> None:
        c = _cfg(delay_frames=3.0, overshoot_pct=0)
        rot = _clip(["upperarm_l", "lowerarm_l", "hand_l"])
        _, rep = ft.solve_chain(rot, c, N)
        d = [l["delay_frames"] for l in rep["links"]]
        self.assertEqual(d, [0.0, 3.0, 6.0])

    def test_max_extra_deg_caps_and_settles(self) -> None:
        c = _cfg(overshoot_pct=70, max_extra_deg=5.0, link_gain=2.0)
        rot = _clip(["upperarm_l", "lowerarm_l", "hand_l"])
        new, rep = ft.solve_chain(rot, c, N)
        for b in rot:
            for f in range(N):
                self.assertLessEqual(ft.q_angle_deg(new[b][f], rot[b][f]), 5.0 + 1e-6)
        self.assertLess(max(l["residual_deg"] for l in rep["links"]), 1.0)

    def test_window_clipped_by_clip_end_and_anchor_outside(self) -> None:
        c = _cfg(hit_frame=50, settle_frames=30)
        _, rep = ft.solve_chain(_clip(["upperarm_l", "lowerarm_l", "hand_l"]), c, N)
        self.assertEqual(rep["window"], [50, N - 1])
        self.assertTrue(any("cut by the clip end" in w for w in rep["warnings"]))
        with self.assertRaises(ValueError):
            ft.solve_chain(_clip(["upperarm_l"]), _cfg(hit_frame=59), N)

    def test_conservation_detects_violations(self) -> None:
        orig = {"hand_l": _swing_track(), "pelvis": _swing_track()}
        moved = {"hand_l": list(orig["hand_l"]), "pelvis": list(orig["pelvis"])}
        moved["pelvis"][30] = ft.qexp((0.0, 0.2, 0.0))
        cons = ft.conservation_report(orig, moved, ["hand_l"], HIT, HIT + 16)
        self.assertFalse(cons["ok"])
        self.assertIn("pelvis", cons["forbidden_bones"])
        moved2 = {"hand_l": list(orig["hand_l"])}
        moved2["hand_l"][HIT] = ft.qexp((0.0, 0.3, 0.0))  # hit frame altered
        self.assertFalse(ft.conservation_report(orig, moved2, ["hand_l"], HIT, HIT + 16)["anchor_frame_unchanged"])
        moved3 = {"hand_l": list(orig["hand_l"])}
        moved3["hand_l"][HIT + 20] = ft.qexp((0.0, 0.3, 0.0))  # outside the window
        self.assertFalse(ft.conservation_report(orig, moved3, ["hand_l"], HIT, HIT + 16)["ok"])

    def test_plan_via_anim_ops_dry_run_and_validate_intent(self) -> None:
        intent = {"follow_through": {"chain": "arms", "mass": "heavy", "overshoot_pct": 30, "anchor": "hit", "hit_frame": 20}}
        clean = anim_ops.validate_intent(intent)
        plan = anim_ops.plan_intent(clean, 62)
        self.assertEqual(plan["kind"], "follow_through")
        self.assertEqual(plan["window"][0], 20)
        self.assertIn("hand_l", plan["bones"])
        self.assertTrue(all(l["frames"] for l in plan["links"]))
        self.assertTrue(all("peak_per_100deg" in l for l in plan["links"]))
        with self.assertRaises(ValueError):  # must be sent alone
            anim_ops.validate_intent({"follow_through": intent["follow_through"], "arm_clr_deg": 5})
        with self.assertRaises(ValueError):
            anim_ops.validate_intent({"follow_through": {"chain": "pelvis>spine_01", "anchor": 5}})

    def test_apply_dry_run_has_no_side_effects(self) -> None:
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            res = anim_ops.anim_apply_intent(
                "w3", "w3ft", {"follow_through": {"chain": "spine_head", "anchor": 20, "mass": "heavy"}}, root=tmp, dry_run=True
            )
            self.assertTrue(res["ok"] and res["dry_run"])
            self.assertEqual(res["plan"]["kind"], "follow_through")
            self.assertFalse((Path(tmp) / "Saved").exists())

    def test_run_intent_verdict_from_report(self) -> None:
        import json
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            rp = Path(tmp) / "r.json"
            rp.write_text(json.dumps({"ok": True, "window": [20, 36], "links": [
                {"bone": "hand_l", "peak_deg": 12.0, "peak_frame": 24, "frames": [21, 35], "residual_deg": 0.1}], "conservation": {"ok": True}}))
            v = anim_ops.follow_through_verdict({"follow_through_report": str(rp)}, "a", "b")
            self.assertEqual(v["verdict"], "PASS")
            rp.write_text(json.dumps({"ok": False, "links": [], "conservation": {"ok": False}}))
            self.assertEqual(anim_ops.follow_through_verdict({"follow_through_report": str(rp)}, "a", "b")["verdict"], "FAIL")
            self.assertEqual(anim_ops.follow_through_verdict({"follow_through_report": str(rp) + "x"}, "a", "b")["verdict"], "FAIL")


if __name__ == "__main__":
    unittest.main()

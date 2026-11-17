"""Cloud tests of GSTN Motion Tools (no editor): python -m unittest discover -s <this folder> -t <Content/Python>.
From Content/Python:  python -m unittest gstn_motion_tools.tests.test_motion_tools
"""
import math
import unittest

from gstn_motion_tools import api, core, presets
from gstn_motion_tools.tests.fake_backend import FakeBackend


def swing(f):
    """A strike-like swing: rest, fast rotation over frames 10..16, hold, stop at 30."""
    if f < 10:
        return 0.0
    if f < 16:
        return 90.0 * core.smoothstep((f - 10) / 6.0)
    return 90.0


def make_chain(n_frames=40, keys=(0, 10, 16, 39)):
    base = {
        "upperarm_l_fk_ctrl": lambda f: ((0, 0, 0), (0, swing(f), 0)),
        "lowerarm_l_fk_ctrl": lambda f: ((0, 0, 0), (0, 0.0, 0)),
        "hand_l_fk_ctrl": lambda f: ((0, 0, 0), (0, 0.0, 0)),
        "head_ctrl": lambda f: ((0, 0, 0), (0, 0.0, 0)),
    }
    k = {c: set(keys) for c in base}
    return FakeBackend(base, k, n_frames, chain_order=["upperarm_l_fk_ctrl", "lowerarm_l_fk_ctrl", "hand_l_fk_ctrl"])


class CoreTests(unittest.TestCase):
    def test_unwrap(self):
        self.assertEqual(core.unwrap_degrees([170, 179, -179, -170]), [170, 179, 181, 190])

    def test_spring_settles_on_constant(self):
        target = [0.0] * 5 + [10.0] * 80
        y = core.spring_follow(target, 0.5, 1.0)
        self.assertAlmostEqual(y[-1], 10.0, places=2)
        self.assertLessEqual(max(y), 10.0 + 1e-3)          # zeta 1: no overshoot

    def test_bounce_overshoots(self):
        target = [0.0] * 5 + [10.0] * 80
        y = core.spring_follow(target, 0.5, core.bounce_to_zeta(1.0))
        self.assertGreater(max(y), 10.5)

    def test_spring_lag_matches_delay(self):
        ramp = [float(t) for t in range(120)]
        y = core.overlap_signal(ramp, 4.0, 0.0, 0.0, core.MODE_SPRING)
        self.assertAlmostEqual(ramp[100] - y[100], 4.0, delta=0.3)

    def test_offset_mode_lag(self):
        ramp = [float(t) for t in range(120)]
        y = core.overlap_signal(ramp, 3.0, 0.0, 0.0, core.MODE_OFFSET)
        self.assertAlmostEqual(ramp[100] - y[100], 3.0, delta=0.3)

    def test_smoothing_has_no_lag(self):
        ramp = [float(t) for t in range(60)]
        y = core.zero_phase_smooth(ramp, 1.0)
        self.assertAlmostEqual(y[30], 30.0, delta=0.05)

    def test_mask_zero_on_keys_and_ends(self):
        m = core.key_weight_mask(20, [8], falloff=2.0)
        self.assertEqual(m[0], 0.0)
        self.assertEqual(m[8], 0.0)
        self.assertEqual(m[19], 0.0)
        self.assertEqual(m[4], 1.0)
        self.assertTrue(0.0 < m[9] < 1.0)

    def test_mask_none(self):
        self.assertEqual(core.key_weight_mask(5, [2], protect=core.PROTECT_NONE), [1.0] * 5)

    def test_easing_endpoints(self):
        for name in core.EASING_PRESETS:
            self.assertAlmostEqual(core.ease(name, 0.0), 0.0, places=6, msg=name)
            self.assertAlmostEqual(core.ease(name, 1.0), 1.0, places=6, msg=name)

    def test_easing_shapes(self):
        self.assertLess(core.ease("anticipo", 0.2), 0.0)          # goes back first
        self.assertGreater(max(core.ease("overshoot", u / 50.0) for u in range(51)), 1.03)
        self.assertLess(core.ease("scatto", 0.5), 0.2)            # still building at half time
        with self.assertRaises(ValueError):
            core.ease("boh", 0.5)

    def test_easing_delta_keeps_keys(self):
        vals = [0.0, 1.0, 5.0, 9.0, 10.0]
        d = core.easing_delta(vals, 0, 4, "lineare")
        self.assertEqual(d[0], 0.0)
        self.assertEqual(d[4], 0.0)
        self.assertAlmostEqual(vals[2] + d[2], 5.0)

    def test_floor_guard_scale(self):
        s = core.floor_guard_scale([20.0, 20.0, 3.0], [10.0, 0.0, 1.0], 0.0, 5.0)
        self.assertEqual(s[0], 1.0)                                # still above floor + radius
        self.assertAlmostEqual(s[1], 0.75)                         # 20 -> 5 needs 3/4 of the way
        self.assertEqual(s[2], 0.0)                                # base already under: layer removed
        soft = core.soften_scale([1, 1, 1, 0.5, 1, 1, 1])
        self.assertLessEqual(soft[3], 0.5)
        self.assertLess(soft[2], 1.0)


class ApiTests(unittest.TestCase):
    def test_presets_resolve(self):
        b = make_chain()
        self.assertEqual(presets.resolve_chain("braccio_l", b.controls()),
                         ["upperarm_l_fk_ctrl", "lowerarm_l_fk_ctrl", "hand_l_fk_ctrl"])
        self.assertEqual(presets.resolve_chain("Testa", b.controls()), ["head_ctrl"])
        self.assertNotIn("mantello", presets.available_chains(b.controls()))

    def test_overlap_keeps_base_and_keys(self):
        b = make_chain()
        base_before = [b.base["upperarm_l_fk_ctrl"](f) for f in range(40)]
        r = api.apply_overlap(b, chain="braccio_l", preset="pesante")
        self.assertTrue(r["ok"])
        self.assertEqual([b.base["upperarm_l_fk_ctrl"](f) for f in range(40)], base_before)
        for f in (0, 10, 16, 39):                                  # key poses exact
            for c in r["controls"]:
                self.assertAlmostEqual(b.value(c, f)[1][1], b.value(c, f, mute=["Overlap"])[1][1], places=6)
        # Between keys the arm lags behind the swing.
        self.assertLess(b.value("upperarm_l_fk_ctrl", 13)[1][1], b.value("upperarm_l_fk_ctrl", 13, ["Overlap"])[1][1])
        self.assertGreater(r["peak_delta"], 1.0)

    def test_reapply_is_idempotent(self):
        b = make_chain()
        api.apply_overlap(b, chain="braccio_l", preset="frusta")
        first = b.value("upperarm_l_fk_ctrl", 13)
        api.apply_overlap(b, chain="braccio_l", preset="frusta")
        self.assertEqual(first, b.value("upperarm_l_fk_ctrl", 13))
        self.assertEqual(b.settings["overlap"]["braccio_l"]["preset"], "frusta")

    def test_sliders_override_preset(self):
        b = make_chain()
        r = api.apply_overlap(b, chain="braccio_l", preset="leggero", delay=6, mode="sfasamento")
        self.assertEqual(r["settings"]["delay"], 6.0)
        self.assertEqual(r["settings"]["mode"], core.MODE_OFFSET)

    def test_easing_layer_under_overlap(self):
        b = make_chain(keys=(0, 39))
        api.apply_easing(b, ["upperarm_l_fk_ctrl"], 10, 16, "anticipo")
        eased = b.value("upperarm_l_fk_ctrl", 11)[1][1]
        self.assertLess(eased, 0.0)                                # anticipation goes back
        self.assertAlmostEqual(b.value("upperarm_l_fk_ctrl", 16)[1][1], 90.0)
        api.apply_overlap(b, controls=["upperarm_l_fk_ctrl"], preset="rigido", protect="none")
        # Overlap read base + easing: with the overlap muted we see the eased value again.
        self.assertAlmostEqual(b.value("upperarm_l_fk_ctrl", 11, ["Overlap"])[1][1], eased)

    def test_floor_guard_limits_layer(self):
        # The arm swings down from 60 to 30 degrees; a bouncy overlap overshoots past 30, toward the floor.
        b = make_chain(keys=(0, 39))
        down = lambda f: 60.0 - 30.0 * core.smoothstep((f - 10) / 6.0) if f >= 10 else 60.0
        b.base["upperarm_l_fk_ctrl"] = lambda f: ((0, 0, 0), (0, down(f), 0))
        b.root_z = 90.0                                            # base lowest tip: 90 - 90*cos30 = 12.1 cm
        kw = dict(chain="braccio_l", preset="frusta", delay=6, bounce=1.0, protect="ends")
        free = make_chain(keys=(0, 39))
        free.base, free.root_z = b.base, b.root_z
        api.apply_overlap(free, **kw)
        floor_z, r_hand = 7.0, presets.radius_for("hand_l_fk_ctrl")
        self.assertLess(min(free.world_z("hand_l_fk_ctrl", range(40), [])), floor_z + r_hand)   # would go through
        r = api.apply_overlap(b, floor={"z": floor_z, "parts": ["hand_l_fk_ctrl"]}, **kw)
        self.assertGreater(r["floor"]["frames_reduced"], 0)
        self.assertGreaterEqual(min(b.world_z("hand_l_fk_ctrl", range(40), [])), floor_z + r_hand - 0.3)
        self.assertLess(r["floor"]["residual_cm"], 0.3)

    def test_remove_and_layer_switch(self):
        b = make_chain()
        api.apply_overlap(b, chain="braccio_l")
        api.set_layer(b, "Overlap", active=False, weight=0.5)
        self.assertFalse(b.layers["Overlap"]["active"])
        api.remove(b, "Overlap", chain="braccio_l")
        self.assertNotIn("braccio_l", b.settings.get("overlap", {}))
        api.remove(b, "Overlap")
        self.assertFalse(b.has_layer("Overlap"))

    def test_dense_base_falls_back_to_ends_and_explicit_keys(self):
        b = make_chain(keys=tuple(range(40)))                     # baked clip: a key on every frame
        r = api.apply_overlap(b, chain="braccio_l", preset="pesante")
        self.assertIn("warnings", r)
        self.assertGreater(r["peak_delta"], 1.0)                   # not pinned everywhere
        r = api.apply_overlap(b, chain="braccio_l", preset="pesante", key_frames=[0, 13, 39])
        self.assertNotIn("warnings", r)
        for c in r["controls"]:
            self.assertAlmostEqual(b.value(c, 13)[1][1], b.value(c, 13, ["Overlap"])[1][1], places=6)

    def test_errors(self):
        b = make_chain()
        with self.assertRaises(ValueError):
            api.apply_overlap(b, chain="coda")                    # not on this rig
        with self.assertRaises(ValueError):
            api.apply_overlap(b, chain="braccio_l", preset="boh")
        with self.assertRaises(ValueError):
            api.apply_easing(b, ["head_ctrl"], 30, 50, "morbido")
        with self.assertRaises(ValueError):
            api.set_layer(b, "Easing", active=True)


if __name__ == "__main__":
    unittest.main()

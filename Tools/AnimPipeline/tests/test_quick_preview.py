"""quick_preview (1C): view axes match pie_capture_move's azimuths, the grid renders, the change check flags a too-small edit. Offline."""
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import quick_preview as Q  # noqa: E402

PARENT = {"pelvis": None, "spine": "pelvis", "head": "spine", "hand_l": "spine", "hand_r": "spine", "foot_l": "pelvis", "foot_r": "pelvis"}


def pose(dx=0.0):
    return {"pelvis": np.array([0.0, 0.0, 100.0]), "spine": np.array([0.0, 0.0, 150.0]), "head": np.array([0.0, 0.0, 170.0]),
            "hand_l": np.array([30.0 + dx, 10.0, 140.0]), "hand_r": np.array([-30.0, 10.0, 140.0]),
            "foot_l": np.array([15.0, 0.0, 0.0]), "foot_r": np.array([-15.0, 0.0, 0.0])}


class TestQuickPreview(unittest.TestCase):
    def test_axes(self):
        sx, look = Q.view_axes(90)            # on the pawn's right: forward (+Y) to the screen right
        np.testing.assert_allclose(sx, [0, 1, 0], atol=1e-9)
        sx, look = Q.view_axes(180)           # behind: the pawn's right (-X) to the screen right, looking forward
        np.testing.assert_allclose(sx, [-1, 0, 0], atol=1e-9)
        np.testing.assert_allclose(look, [0, 1, 0], atol=1e-9)

    def test_render_and_change(self):
        var, base = {0: pose(5.0), 5: pose(1.0)}, {0: pose(), 5: pose()}
        with tempfile.TemporaryDirectory() as t:
            out = Q.render(var, base, [0, 5], [90, 180], PARENT, str(Path(t) / "p.png"), 120, "t")
            from PIL import Image
            self.assertEqual(Image.open(out).size, (240, 258))
        cm, joint, f = Q.peak_change(var, base)
        self.assertEqual((round(cm, 1), joint, f), (5.0, "hand_l", 0))
        self.assertLess(Q.peak_change({5: pose(1.0)}, base)[0], Q.MIN_MOVE_CM)


if __name__ == "__main__":
    unittest.main()

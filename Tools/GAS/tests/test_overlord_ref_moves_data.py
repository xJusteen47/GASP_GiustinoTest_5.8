# Pure checks of Tools/GAS/overlord_ref_moves_data.py (no editor): python3 -m unittest Tools/GAS/tests/test_overlord_ref_moves_data.py
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import overlord_ref_moves_data as D  # noqa: E402


class RefMoves(unittest.TestCase):
    def test_windows_inside_the_move(self):
        for key, m in D.MOVES.items():
            self.assertLess(m['faf'], m['total'], key)
            for first, last, _path, boxes in m['windows']:
                self.assertLessEqual(first, last, key)
                self.assertLess(D.tl(last), m['faf'], f'{key}: a hitbox after the first actionable frame')
                for hit, *_ in boxes:
                    self.assertIn(hit, m['hits'], key)

    def test_centred_on_the_facing(self):
        """Owner 2026-11-25: swings centred, or precision suffers. Vertical paths stay at y 0; a horizontal swing is symmetric about the facing"""
        for key, m in D.MOVES.items():
            for window in m['windows']:
                path = window[2]
                if path[0] == 'horizontal':
                    self.assertAlmostEqual(path[2], -path[3], msg=key)
                    continue
                for _hit, _r, _half, keys in D.window_keys(window):
                    for _f, centre, _rot in keys:
                        self.assertAlmostEqual(centre[1], 0.0, places=6, msg=key)

    def test_landing_windows_tile_the_aerials(self):
        for key, m in D.MOVES.items():
            if not m['air']:
                self.assertIsNone(m['landing'], key)
                continue
            frame = 1
            for first, last, _what in m['landing']:
                self.assertEqual(first, frame, key)
                frame = (last or m['total']) + 1
            self.assertGreater(frame, m['total'], key)

    def test_smash_conversions(self):
        self.assertEqual(D.hitlag(24.0), 21)          # floor(24 x 0.65 + 6)
        self.assertEqual(D.hit_numbers('FS', 'Clean')['bkb_cms'], 61 * 22.5)


if __name__ == '__main__':
    unittest.main()

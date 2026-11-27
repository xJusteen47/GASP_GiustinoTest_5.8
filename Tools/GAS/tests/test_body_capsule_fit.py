"""Tools/GAS/body_capsule_fit.py without the editor: the aerial capsule's bottom on the lowest foot, never under the mesh root, held once the
feet reached their lowest, the minimum height, and the runs. Run: python Tools/GAS/tests/test_body_capsule_fit.py (or python -m pytest)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import body_capsule_fit as F  # noqa: E402


def sample(foot_l, foot_r, head=160.0, torso=(5.0, -10.0, 100.0)):
    return {'torso': torso, 'head_z': head, 'foot_z': (foot_l, foot_r)}


def test_bottom_on_lowest_foot_minus_sole():
    s = F.spans([sample(60.0, 40.0)], sole=8.0, skull=15.0, min_hh=30.0)
    assert s == [(5.0, -10.0, 32.0, 175.0)]


def test_never_under_the_mesh_root():
    assert F.spans([sample(3.0, 50.0)])[0][2] == 0.0


def test_bottom_held_after_the_deepest_frame():
    sp = F.spans([sample(80.0, 80.0), sample(30.0, 30.0), sample(90.0, 90.0)])
    assert [v[2] for v in sp] == [72.0, 22.0, 22.0]


def test_minimum_height_when_tucked():
    # Feet tucked up to the head: the capsule is still 2 x min_hh tall
    sp = F.spans([sample(140.0, 140.0, head=150.0)], min_hh=30.0)
    assert (sp[0][2], sp[0][3]) == (132.0, 192.0)


def test_runs_cut_on_tolerance_and_drop_lateral():
    frames = [sample(80.0, 80.0)] * 3 + [sample(40.0, 40.0)] * 2
    tracks = F.fit(frames, tol=8.0)
    assert [(a, b) for a, b, *_ in tracks] == [(0, 3), (3, 5)]
    assert all(t[4] == 0.0 for t in tracks)
    assert F.fit(frames, lateral=True)[0][4] == -10.0
    a, b, hh, x, y, z = tracks[1]
    assert (z - hh, z + hh) == (32.0, 175.0)


def test_empty():
    assert F.fit([]) == []


if __name__ == '__main__':
    for name, fn in list(globals().items()):
        if name.startswith('test_'):
            fn()
    print('body_capsule_fit: all tests passed')

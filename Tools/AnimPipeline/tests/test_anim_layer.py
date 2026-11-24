"""Offline tests for anim_layer (pure Python, no UE): validation, envelope, w3 equivalence, conservation, stack/undo.

The w3 equivalence tests re-implement, literally, the three steps that the w3 cycle wrote by hand in runstart_w3_ue.py
(c1 passo-02 22a318f1, c1 passo-03 d5cfea6a, c2 passo-03 f3978762 on claude/runstart-generic-desktop) and check that
the layer JSON a flash model would send gives the SAME rotations on a synthetic mannequin. Run: python -m pytest tests.
"""
from __future__ import annotations

import json
import math
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

MCP_DIR = Path(__file__).resolve().parents[1] / "gstn_anim_mcp"
if str(MCP_DIR) not in sys.path:
    sys.path.insert(0, str(MCP_DIR))

import anim_layer as L  # noqa: E402
import anim_layer_ops as OPS  # noqa: E402
import mcp_server  # noqa: E402

N = 30
I = (0.0, 0.0, 0.0, 1.0)


def rx(deg):
    return L.axis_quat((1.0, 0.0, 0.0), deg)


def rz(deg):
    return L.axis_quat((0.0, 0.0, 1.0), deg)


def synthetic_clip(root_yaw=0.0, arm_r_back=(-35.0, 12.0)):
    """Mannequin-like clip in asset space: faces +Y, left = +X, Z up. Arms swing (arm_r behind, arm_l in front)."""
    off = {
        "pelvis": (0, 0, 95), "spine_01": (0, 0, 8), "spine_02": (0, 0, 10), "spine_03": (0, 0, 10), "spine_04": (0, 0, 9),
        "spine_05": (0, 0, 9), "neck_01": (0, 1, 10), "neck_02": (0, 0, 5), "head": (0, 0, 6),
        "thigh_l": (9, 0, -4), "calf_l": (0, 0, -45), "foot_l": (0, 0, -42), "ball_l": (0, 12, -5),
        "thigh_r": (-9, 0, -4), "calf_r": (0, 0, -45), "foot_r": (0, 0, -42), "ball_r": (0, 12, -5),
    }
    for s, x in (("l", 1.0), ("r", -1.0)):
        off.update({f"clavicle_{s}": (3 * x, 2, 4), f"upperarm_{s}": (14 * x, -1, -2), f"lowerarm_{s}": (1.5 * x, 0, -28),
                    f"hand_{s}": (0, 0, -25), f"middle_metacarpal_{s}": (0, 0, -8)})
    local = {"root": []}
    for b in off:
        local[b] = []
    for f in range(N):
        ph = f / 6.0
        local["root"].append(((0.0, 1.5 * f, 0.0), rz(root_yaw)))
        for b, t in off.items():
            q = I
            if b == "spine_01":
                q = rx(-8.0 - 6.0 * math.sin(ph))
            elif b == "upperarm_r":
                q = rx(arm_r_back[0] - arm_r_back[1] * math.sin(ph))
            elif b == "upperarm_l":
                q = rx(30.0 + 12.0 * math.sin(ph))
            elif b.startswith("lowerarm_"):
                q = rx(45.0 + 10.0 * math.cos(ph))
            elif b == "thigh_l":
                q = rx(25.0 * math.sin(ph))
            elif b == "thigh_r":
                q = rx(-25.0 * math.sin(ph))
            local[b].append((tuple(float(c) for c in t), L.qnorm(q)))
    return N, local


def world_rot(local, f):
    return L.fk({b: local[b][f] for b in local}, L.resolve_parents(list(local)))


def apply_world_delta(local, f, bone, d):
    parents = L.resolve_parents(list(local))
    w = world_rot(local, f)
    t, _ = local[bone][f]
    local[bone][f] = (t, L.qnorm(L.qmul(L.qinv(w[parents[bone]][1]), L.qmul(d, w[bone][1]))))


def copy_local(local):
    return {b: list(rows) for b, rows in local.items()}


# ---- literal re-implementation of the w3 step code (unreal.Quat replaced by tuples; world axes X left, Y fwd, Z up)
def w3_yaw(deg):
    r = math.radians(deg)
    return (0.0, 0.0, math.sin(0.5 * r), math.cos(0.5 * r))


def w3_pitch(deg, ref):
    r = math.radians(deg)
    qp, qn = (math.sin(0.5 * r), 0.0, 0.0, math.cos(0.5 * r)), (-math.sin(0.5 * r), 0.0, 0.0, math.cos(0.5 * r))
    return qp if L.qrot(qp, ref)[1] >= L.qrot(qn, ref)[1] else qn


def lat_vec(w):
    d = L.vsub(w["thigh_l"][0], w["thigh_r"][0])
    return L.vnorm((d[0], d[1], 0.0))


def w3_arm_out(local, f, s, deg):
    """22a318f1 / d5cfea6a: yaw out + roll about +Y out on upperarm, env ss(1,5)*(1-ss(13,21)), sign from la_dir0."""
    sign = 1.0 if s == "l" else -1.0
    e = L.ss(1.0, 5.0, f) * (1.0 - L.ss(13.0, 21.0, f))
    if e <= 0.0:
        return
    w = world_rot(local, f)
    lat = lat_vec(w)
    la_dir0 = L.vsub(w[f"lowerarm_{s}"][0], w[f"upperarm_{s}"][0])
    q_out_pos = w3_yaw(deg * e)
    q_out = q_out_pos if L.vdot(L.vsub(L.qrot(q_out_pos, la_dir0), la_dir0), L.vmul(lat, sign)) >= 0 else L.qinv(q_out_pos)
    ra = math.radians(deg * e)
    qp_a = (0.0, math.sin(0.5 * ra), 0.0, math.cos(0.5 * ra))
    q_abd = qp_a if L.vdot(L.vsub(L.qrot(qp_a, la_dir0), la_dir0), L.vmul(lat, sign)) >= 0 else L.qinv(qp_a)
    apply_world_delta(local, f, f"upperarm_{s}", L.qmul(q_abd, q_out))


def w3_arm_l_extra(local, f):
    """d5cfea6a: upperarm_l forward 14 deg env ss(8,13)*(1-ss(13,21)); lowerarm_l hook 16 deg ss(1,5)*(1-ss(7,12))
    toward lat_vec, then forward 7 deg (0.5 * 14) with the f13 envelope; signs from ha_dir0 (after the parent)."""
    w = world_rot(local, f)
    la_dir0 = L.vsub(w["lowerarm_l"][0], w["upperarm_l"][0])
    e13 = L.ss(8.0, 13.0, f) * (1.0 - L.ss(13.0, 21.0, f))
    if e13 > 1e-4:
        apply_world_delta(local, f, "upperarm_l", w3_pitch(14.0 * e13, la_dir0))
    w = world_rot(local, f)
    lat = lat_vec(w)
    ha_dir0 = L.vsub(w["hand_l"][0], w["lowerarm_l"][0])
    q = I
    e_hook = L.ss(1.0, 5.0, f) * (1.0 - L.ss(7.0, 12.0, f))
    if e_hook > 1e-4:
        qh = w3_yaw(16.0 * e_hook)
        q = qh if L.vdot(L.vsub(L.qrot(qh, ha_dir0), ha_dir0), lat) >= 0 else L.qinv(qh)
    if e13 > 1e-4:
        q = L.qmul(w3_pitch(7.0 * e13, ha_dir0), q)
    if q != I:
        apply_world_delta(local, f, "lowerarm_l", q)


def w3_head_up(local, f):
    """f3978762: neck_01 chin lift 6.5 deg about X, env ss(5,13)*(1-ss(13,21)), sign so that (0,10,0) goes up."""
    e = L.ss(5.0, 13.0, f) * (1.0 - L.ss(13.0, 21.0, f))
    if e <= 1e-4:
        return
    rh = math.radians(6.5 * e)
    qp = (math.sin(0.5 * rh), 0.0, 0.0, math.cos(0.5 * rh))
    qn = L.qinv(qp)
    vf = (0.0, 10.0, 0.0)
    apply_world_delta(local, f, "neck_01", qp if L.qrot(qp, vf)[2] >= L.qrot(qn, vf)[2] else qn)


# the layer JSON for the same three steps (what the flash model has to write)
STEP_C1_02 = [
    {"part": "arm_r", "motion": "turn_out", "deg": 10, "start": 1, "peak": [5, 13], "end": 21},
    {"part": "arm_r", "motion": "out", "deg": 10, "start": 1, "peak": [5, 13], "end": 21},
]
STEP_C1_03 = [
    {"part": "arm_l", "motion": "turn_out", "deg": 10, "start": 1, "peak": [5, 13], "end": 21},
    {"part": "arm_l", "motion": "out", "deg": 10, "start": 1, "peak": [5, 13], "end": 21},
    {"part": "arm_l", "motion": "forward", "deg": 14, "start": 8, "peak": 13, "end": 21},
    {"part": "forearm_l", "motion": "turn_out", "deg": 16, "start": 1, "peak": [5, 7], "end": 12},
    {"part": "forearm_l", "motion": "forward", "deg": 7, "start": 8, "peak": 13, "end": 21},
]
STEP_C2_03 = [{"part": "neck", "motion": "chin_up", "deg": 6.5, "start": 5, "peak": 13, "end": 21}]


def run_layers(local, raws, n=N):
    norm = [L.validate_layer(r, n) for r in raws]
    new, rep = L.apply_stack(n, local, norm)
    out = copy_local(local)
    for b, qs in new.items():
        out[b] = [(local[b][f][0], qs[f]) for f in range(n)]
    return out, rep, norm, new


def max_world_diff(a, b):
    worst = 0.0
    for f in range(N):
        wa, wb = world_rot(a, f), world_rot(b, f)
        for bone in wa:
            worst = max(worst, L.vlen(L.vsub(wa[bone][0], wb[bone][0])),
                        L.qdiff_deg(wa[bone][1], wb[bone][1]))
    return worst


class TestValidate(unittest.TestCase):
    def test_owner_example_and_italian(self):
        a = L.validate_layer({"part": "gomito_r", "motion": "fuori", "deg": 10, "start": 8, "peak": 13, "end": 19})
        self.assertEqual(a["bones"], [("upperarm_r", 1.0)])
        self.assertEqual((a["axis"], a["toward"]), ("fwd", "out"))
        self.assertEqual(L.validate_layer({"part": "elbow_l", "motion": "flex", "deg": 15, "start": 2, "peak": 6, "end": 12})["bones"],
                         [("lowerarm_l", 1.0)])

    def test_refusals_are_actionable(self):
        bad = [
            ({"part": "thigh_l", "motion": "forward", "deg": 5, "start": 1, "peak": 5, "end": 9}, "anim_run_intent"),
            ({"part": "elbow_r", "motion": "out", "deg": 10, "peak": 5, "end": 9}, "missing start"),
            ({"part": "elbow_r", "motion": "out", "deg": 10, "start": 4, "peak": 5, "end": 9}, "step key"),
            ({"part": "elbow_r", "motion": "out", "deg": 10, "start": 1, "peak": 5, "end": 6}, "step key"),
            ({"part": "elbow_r", "motion": "out", "deg": 60, "start": 1, "peak": 5, "end": 9}, "correction"),
            ({"part": "head", "motion": "out", "deg": 5, "start": 1, "peak": 5, "end": 9}, "midline"),
            ({"part": "elbow", "motion": "out", "deg": 5, "start": 1, "peak": 5, "end": 9}, "needs a side"),
            ({"part": "elbow_r", "motion": "wiggle", "deg": 5, "start": 1, "peak": 5, "end": 9}, "unknown motion"),
            ({"part": "head", "motion": "flex", "deg": 5, "start": 1, "peak": 5, "end": 9}, "elbow"),
        ]
        for layer, msg in bad:
            with self.assertRaises(ValueError) as cm:
                L.validate_layer(layer)
            self.assertIn(msg, str(cm.exception), layer)
        with self.assertRaises(ValueError) as cm:
            L.validate_layer({"part": "elbow_r", "motion": "out", "deg": 5, "start": 1, "peak": 5, "end": 40}, n=30)
        self.assertIn("clip-local 0..29", str(cm.exception))

    def test_clip_edges_may_have_no_ramp(self):
        L.validate_layer({"part": "arm_r", "motion": "in", "deg": 4, "start": 0, "peak": [0, 3], "end": 7}, n=30)
        L.validate_layer({"part": "arm_r", "motion": "in", "deg": 4, "start": 20, "peak": [25, 29], "end": 29}, n=30)


class TestEnvelope(unittest.TestCase):
    def test_same_curve_as_w3_and_anchored(self):
        for f in range(N):
            self.assertAlmostEqual(L.envelope(f, 1, 5, 13, 21), L.ss(1, 5, f) * (1 - L.ss(13, 21, f)), places=12)
        self.assertEqual(L.envelope(1, 1, 5, 13, 21), 0.0)
        self.assertEqual(L.envelope(21, 1, 5, 13, 21), 0.0)
        self.assertEqual(L.envelope(9, 1, 5, 13, 21), 1.0)

    def test_rate_measure(self):
        nl = L.validate_layer({"part": "elbow_r", "motion": "out", "deg": 30, "start": 3, "peak": 5, "end": 15})
        rate, at = L.max_rate(nl, N)
        self.assertGreater(rate, L.RATE_WARN_DEG)          # 30 deg over a 2-frame ramp: warned, not blocked (owner 2A)
        self.assertIn(at, (4, 5))


class TestW3Equivalence(unittest.TestCase):
    """Layers == the hand-written w3 code, frame by frame, on every bone (rotation and position)."""

    def _ref(self, fn):
        n, local = synthetic_clip()
        ref = copy_local(local)
        for f in range(n):
            fn(ref, f)
        return local, ref

    def test_c1_passo02_arm_r(self):
        local, ref = self._ref(lambda r, f: w3_arm_out(r, f, "r", 10.0))
        out, rep, _, _ = run_layers(local, STEP_C1_02)
        self.assertLess(max_world_diff(out, ref), 1e-6)
        mj = rep["layers"][1]["bones"][0]["moved_joint_at_peak"]
        self.assertGreater(mj["out_cm"], 0.0)               # the elbow went OUT (right side = -X)

    def test_c1_passo03_arm_l(self):
        def ref_fn(r, f):
            w3_arm_out(r, f, "l", 10.0)
            w3_arm_l_extra(r, f)
        local, ref = self._ref(ref_fn)
        out, _, _, _ = run_layers(local, STEP_C1_03)
        self.assertLess(max_world_diff(out, ref), 1e-6)

    def test_c2_passo03_head(self):
        local, ref = self._ref(w3_head_up)
        out, rep, _, _ = run_layers(local, STEP_C2_03)
        self.assertLess(max_world_diff(out, ref), 1e-6)
        self.assertEqual(rep["layers"][0]["bones"][0]["deg_at_peak"], 6.5)   # esito.json: head_up_f13_deg 6.5
        w0, w1 = world_rot(local, 13), world_rot(out, 13)
        f0 = L.qrot(w0["head"][1], (0.0, 1.0, 0.0))
        f1 = L.qrot(w1["head"][1], (0.0, 1.0, 0.0))
        pitch = math.degrees(math.atan2(f1[2], f1[1]) - math.atan2(f0[2], f0[1]))
        self.assertAlmostEqual(pitch, 6.5, places=6)                          # the face really looks 6.5 deg higher


class TestSafety(unittest.TestCase):
    def test_conservation_on_the_data(self):
        n, local = synthetic_clip()
        norm = [L.validate_layer(r, n) for r in STEP_C1_03 + STEP_C2_03]
        new, _ = L.apply_stack(n, local, norm)
        c = L.conservation(n, local, new, norm)
        self.assertTrue(c["ok"], c)
        self.assertEqual(c["pelvis_feet_max_move_cm"], 0.0)
        for f in (0, 1, 21, 25, 29):                       # anchors and outside: bit-identical
            for b, qs in new.items():
                self.assertEqual(qs[f], local[b][f][1], (b, f))
        bad = dict(new)
        bad["upperarm_l"] = [L.qmul(rx(3.0), q) for q in new["upperarm_l"]]
        self.assertFalse(L.conservation(n, local, bad, norm)["ok"])

    def test_body_axes_follow_the_character_not_the_world(self):
        """Same clip turned 90 deg: body-frame layers give the same LOCAL result; clip-frame layers do not."""
        n, a = synthetic_clip(0.0)
        _, b = synthetic_clip(90.0)
        _, _, _, na = run_layers(a, STEP_C1_02)
        _, _, _, nb = run_layers(b, STEP_C1_02)
        self.assertLess(max(L.qdiff_deg(x, y)
                            for x, y in zip(na["upperarm_r"], nb["upperarm_r"])), 1e-6)
        clip = [dict(x, frame="clip") for x in STEP_C1_02]
        _, _, _, cb = run_layers(b, clip)
        self.assertGreater(max(L.qdiff_deg(x, y)
                               for x, y in zip(na["upperarm_r"], cb["upperarm_r"])), 1.0)

    def test_sign_is_fixed_for_the_whole_layer(self):
        """arm_r swings across the vertical inside the window (f16-20): the per-frame sign choice of the w3 code flips
        turn_out there (a ~5 deg jump to the other side at f16); the layer keeps the peak's direction (no pop)."""
        n, local = synthetic_clip(arm_r_back=(15.0, 25.0))

        def yaw_signs(lc_after, lc_before):
            out = set()
            for f in range(2, 21):
                d = L.qmul(world_rot(lc_after, f)["upperarm_r"][1], L.qinv(world_rot(lc_before, f)["upperarm_r"][1]))
                if L.q_angle(d) > 0.2:
                    out.add(1 if d[2] * (1 if d[3] >= 0 else -1) > 0 else -1)
            return out
        ref = copy_local(local)
        for f in range(n):                                  # w3 code, yaw part only, sign chosen per frame
            e = L.ss(1.0, 5.0, f) * (1.0 - L.ss(13.0, 21.0, f))
            if e > 0:
                w = world_rot(ref, f)
                la = L.vsub(w["lowerarm_r"][0], w["upperarm_r"][0])
                q = w3_yaw(10.0 * e)
                q = q if L.vdot(L.vsub(L.qrot(q, la), la), L.vmul(lat_vec(w), -1.0)) >= 0 else L.qinv(q)
                apply_world_delta(ref, f, "upperarm_r", q)
        self.assertEqual(yaw_signs(ref, local), {1, -1})     # the old code flips
        out, _, _, _ = run_layers(local, [STEP_C1_02[0]])
        self.assertEqual(len(yaw_signs(out, local)), 1)      # the layer does not

    def test_flex_bends_the_elbow(self):
        n, local = synthetic_clip()
        out, _, _, _ = run_layers(local, [{"part": "elbow_r", "motion": "flex", "deg": 15, "start": 2, "peak": 8, "end": 14}])

        def angle(lc, f):
            w = world_rot(lc, f)
            u = L.vsub(w["upperarm_r"][0], w["lowerarm_r"][0])
            v = L.vsub(w["hand_r"][0], w["lowerarm_r"][0])
            return math.degrees(math.acos(L.vdot(L.vnorm(u), L.vnorm(v))))
        self.assertAlmostEqual(angle(local, 8) - angle(out, 8), 15.0, places=6)   # inner angle closes by 15 deg


class TestFlashDryRun(unittest.TestCase):
    """Layers a cheap model (Haiku, standing in for DeepSeek flash) wrote from the 3 real w3 instructions, reading only the
    tool description (docs/notes/anim-layer/flash-dryrun-w3-2026-11-24.json): all valid, conservative, and they move the
    same joints the same way as the w3 code. Amplitudes differ: the instructions carry no numbers ("leggermente")."""
    FLASH = Path(__file__).resolve().parents[3] / "docs" / "notes" / "anim-layer" / "flash-dryrun-w3-2026-11-24.json"

    def test_valid_and_same_direction(self):
        fl = json.loads(self.FLASH.read_text(encoding="utf-8"))
        for key, ref_steps, joint, axis in (("c1_passo-02", STEP_C1_02, "lowerarm_r", 0), ("c1_passo-03", STEP_C1_03, "lowerarm_l", 0),
                                            ("c2_passo-03", STEP_C2_03, "head", 1)):
            n, local = synthetic_clip()
            norm = [L.validate_layer(x, n) for x in fl[key]]
            new, _ = L.apply_stack(n, local, norm)
            self.assertTrue(L.conservation(n, local, new, norm)["ok"], key)
            a, _, _, _ = run_layers(local, fl[key])
            b, _, _, _ = run_layers(local, ref_steps)
            w0, wa, wb = world_rot(local, 13), world_rot(a, 13), world_rot(b, 13)
            ax = L.body_axes(w0)[axis]
            da, db = L.vdot(L.vsub(wa[joint][0], w0[joint][0]), ax), L.vdot(L.vsub(wb[joint][0], w0[joint][0]), ax)
            self.assertGreater(da * db, 0.0, (key, da, db))


class TestStackOps(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        (self.root / "docs" / "notes").mkdir(parents=True)
        (self.root / "docs" / "notes" / "anim-approvate.md").write_text("| run start | w2 | AS_GSTN_RunStart_Generic_w2 (x) |\n")

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def _dump(self, src):
        n, local = synthetic_clip()
        p = OPS.dump_path(self.root, src)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps({"n": n, "local": {b: [list(t) + list(q) for t, q in rows] for b, rows in local.items()}}))

    def test_add_undo_list_and_guards(self):
        src, dst = "AS_Test_w3", "AS_Test_w3_L1"
        r = OPS.anim_layer("add", dst=dst, src=src, layer=STEP_C2_03[0], root=self.root)
        self.assertTrue(r["ok"] and r["id"] == 1 and not r["measured"])
        self.assertEqual(r["layer"]["bones"][0]["deg_at_peak"], 6.5)
        self._dump(src)
        dry = OPS.anim_layer("add", dst=dst, layer=STEP_C1_02[1], dry_run=True, root=self.root)
        self.assertTrue(dry["measured"] and dry["conservation"]["ok"])
        self.assertGreater(dry["layer"]["bones"][0]["moved_joint_at_peak"]["out_cm"], 0.0)
        self.assertEqual(len(OPS.anim_layer("list", dst=dst, root=self.root)["layers"]), 1)   # dry run added nothing
        for x in STEP_C1_02:
            OPS.anim_layer("add", dst=dst, layer=x, root=self.root)
        lst = OPS.anim_layer("list", dst=dst, root=self.root)
        self.assertEqual([e["id"] for e in lst["layers"]], [1, 2, 3])
        self.assertFalse(lst["built"])
        u = OPS.anim_layer("undo", dst=dst, root=self.root)
        self.assertEqual(u["removed"], [3])
        u = OPS.anim_layer("undo", dst=dst, to=0, root=self.root)
        self.assertEqual(u["removed"], [1, 2])
        self.assertFalse(OPS.anim_layer("undo", dst=dst, root=self.root)["ok"])
        with self.assertRaises(ValueError):
            OPS.anim_layer("add", dst="AS_GSTN_RunStart_Generic_w2", src=src, layer=STEP_C2_03[0], root=self.root)
        with self.assertRaises(ValueError):
            OPS.anim_layer("add", dst=src, src=src, layer=STEP_C2_03[0], root=self.root)
        with self.assertRaises(ValueError):
            OPS.anim_layer("add", dst=dst, src="AS_Other", layer=STEP_C2_03[0], root=self.root)

    def test_mcp_dispatch(self):
        res = mcp_server._dispatch("anim_layer", {"dst": "AS_T_L", "src": "AS_T", "layer": STEP_C2_03[0], "dry_run": True})
        self.assertTrue(res["ok"] and res["dry_run"])
        names = [t["name"] for t in mcp_server._tool_defs()]
        self.assertEqual(names[-1], "anim_layer")


if __name__ == "__main__":
    unittest.main()

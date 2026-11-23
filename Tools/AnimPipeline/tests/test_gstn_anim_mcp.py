"""Unit tests for gstn-anim-mcp (pure Python, no UE editor required)."""
from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve()
MCP_DIR = HERE.parents[1] / "gstn_anim_mcp"
if str(MCP_DIR) not in sys.path:
    sys.path.insert(0, str(MCP_DIR))

import anim_ops  # noqa: E402
import mcp_server  # noqa: E402


def _make_fake_joints(n: int = 20, scale: float = 1.0) -> dict:
    joints = {}
    for s, sx in (("l", 12.0), ("r", -12.0)):
        joints[f"thigh_MainPoint_{s}"] = [[sx, 90.0, float(i) * scale] for i in range(n)]
        joints[f"calf_MainPoint_{s}"] = [[sx, 48.0, float(i) * scale] for i in range(n)]
        joints[f"foot_MainPoint_{s}"] = [[sx, 10.0, float(i) * scale] for i in range(n)]
        joints[f"ball_MainPoint_{s}"] = [[sx, 3.0, float(i) * scale + 10.0] for i in range(n)]
        joints[f"upperarm_MainPoint_{s}"] = [[sx * 1.4, 140.0, float(i) * scale] for i in range(n)]
        joints[f"lowerarm_MainPoint_{s}"] = [[sx * 1.6, 115.0, float(i) * scale - 5.0] for i in range(n)]
        joints[f"hand_MainPoint_{s}"] = [[sx * 1.7, 92.0, float(i) * scale + 8.0] for i in range(n)]
    joints["root"] = [[0.0, 0.0, float(i) * scale] for i in range(n)]
    return {"fps": 30, "joints": joints}


class TestGstnAnimMcp(unittest.TestCase):
    def test_validate_intent_bounds_and_unknown(self) -> None:
        clean = anim_ops.validate_intent(
            {
                "com_shift_fwd_cm": 5.0,
                "torso_lean_pivot_deg": 12.0,
                "torso_bank_deg": -8.0,
                "shoulder_level_compensate": 0.5,
                "arm_back_arc": 15.0,
                "foot_pushoff_deg": 10.0,
                "w3_knee_drop": -6.0,
                "handoff_smooth_range": [12, 18],
                "frame_range": [0, 20],
            }
        )
        self.assertEqual(clean["torso_lean_pivot_deg"], 12.0)
        self.assertEqual(clean["handoff_smooth_range"], [12, 18])

        with self.assertRaises(ValueError):
            anim_ops.validate_intent({"torso_lean_pivot_deg": 180.0})
        with self.assertRaises(ValueError):
            anim_ops.validate_intent({"bogus_param": 1.0})

    def test_inspect_apply_dryrun_and_rollback(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            rs = root / "Saved" / "AnimPipeline" / "runstart"
            rs.mkdir(parents=True)
            (root / "Content").mkdir(parents=True)
            jpath = rs / "joints_AS_TestClip.json"
            original = _make_fake_joints(20, scale=1.5)
            jpath.write_text(json.dumps(original), encoding="utf-8")

            res = anim_ops.anim_inspect("AS_TestClip", frame_range="2-10", root=root)
            self.assertTrue(res["ok"])
            self.assertEqual(res["frame_range"], [2, 10])
            self.assertAlmostEqual(res["heights_cm"]["hip"]["mean"], 90.0, places=1)
            self.assertGreater(res["clearance_cm"]["ankle_gap"]["min"], 20.0)

            apply_res = anim_ops.anim_apply_intent(
                "AS_Base",
                "AS_TestClip",
                {"torso_lean_pivot_deg": 10.0, "com_shift_fwd_cm": 3.0},
                root=root,
                dry_run=True,
            )
            self.assertTrue(apply_res["ok"])
            self.assertTrue(apply_res["dry_run"])

            # Mutate joints file, then rollback
            jpath.write_text(json.dumps(_make_fake_joints(20, scale=0.1)), encoding="utf-8")
            rb = anim_ops.anim_rollback("AS_TestClip", root=root)
            self.assertTrue(rb["ok"])
            restored = json.loads(jpath.read_text(encoding="utf-8"))
            self.assertEqual(restored["joints"]["root"][10], original["joints"]["root"][10])

    def test_conservation_guard(self) -> None:
        ref = _make_fake_joints(20, scale=2.0)
        ok_cur = _make_fake_joints(20, scale=1.85)  # 7.5% loss -> OK
        bad_cur = _make_fake_joints(20, scale=1.2)  # 40% loss -> FAIL

        r_ok = anim_ops._joint_conservation_check(ref, ok_cur)
        self.assertEqual(r_ok["status"], "OK")

        r_bad = anim_ops._joint_conservation_check(ref, bad_cur)
        self.assertEqual(r_bad["status"], "FAIL")
        self.assertTrue(len(r_bad["failures"]) > 0)

    def test_mcp_protocol_roundtrip(self) -> None:
        init = mcp_server.process_request({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}})
        self.assertEqual(init["result"]["serverInfo"]["name"], "gstn-anim-mcp")

        lst = mcp_server.process_request({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
        names = [t["name"] for t in lst["result"]["tools"]]
        self.assertEqual(
            names,
            ["anim_inspect", "anim_apply_intent", "anim_gate_check", "anim_capture_ab",
             "anim_register_variant", "anim_unregister_variant", "anim_rollback"],
        )

    def test_validate_intent_v1_and_v2_bounds(self) -> None:
        clean = anim_ops.validate_intent(
            {
                "turn_side": "l",
                "turn_deg": 90.0,
                "start_from_frame": 2,
                "pelvis_fwd_cm": 8.0,
                "arm_st_tuck_in_deg": 26.0,
                "knee_drop": 0.15,
                "smooth_pelvis_z": True,
                "smooth_upper_passes": 4,
                "smooth_leg_passes": 2,
            }
        )
        self.assertEqual(clean["turn_side"], "L")
        self.assertEqual(clean["turn_deg"], 90.0)
        self.assertEqual(clean["start_from_frame"], 2)
        self.assertEqual(clean["pelvis_fwd_cm"], 8.0)
        self.assertEqual(clean["arm_st_tuck_in_deg"], 26.0)
        self.assertAlmostEqual(clean["knee_drop"], 0.15)
        self.assertTrue(clean["smooth_pelvis_z"])
        self.assertEqual(clean["smooth_upper_passes"], 4)
        self.assertEqual(clean["smooth_leg_passes"], 2)

        self.assertEqual(anim_ops.validate_intent({"turn_side": "l"})["turn_side"], "L")
        self.assertEqual(anim_ops.validate_intent({"turn_side": "r"})["turn_side"], "R")
        self.assertEqual(anim_ops.validate_intent({"turn_side": "NONE"})["turn_side"], "none")

        for bad in (
            {"arm_st_tuck_in_deg": 90.0},
            {"knee_drop": 1.5},
            {"start_from_frame": -1},
            {"start_from_frame": True},
            {"turn_side": "X"},
            {"bogus_param": 1.0},
        ):
            with self.assertRaises(ValueError):
                anim_ops.validate_intent(bad)

    def test_anim_capture_ab_single_and_multi_pair(self) -> None:
        def parse_runs(cmd) -> list:
            cmd_str = " ".join(map(str, cmd))
            body = cmd_str.split("-Runs @(", 1)[1].split(")", 1)[0]
            return re.findall(r"'([^']*)'", body)

        def fake_ab_runner(calls_list):
            def _run(cmd, **kwargs):
                calls_list.append(cmd)
                cmd_s = [str(c) for c in cmd]
                if any("runstart_ab_video.py" in c for c in cmd_s):
                    idx = next(i for i, c in enumerate(cmd_s) if "runstart_ab_video.py" in c)
                    outdir = Path(cmd_s[idx + 3])
                    outdir.mkdir(parents=True, exist_ok=True)
                    (outdir / "grid_ab_az180.jpg").write_bytes(b"\xff\xd8\xff\xd9")
                    (outdir / "ab_out_real.mp4").write_bytes(b"\x00\x00\x00\x18ftypmp42")
                return subprocess.CompletedProcess(args=cmd, returncode=0, stdout="ok", stderr="")
            return _run

        # Single-pair capture: az180 is prepended and one pwsh -Command call carries 5-field pipe runs.
        with tempfile.TemporaryDirectory() as tmp:
            ue59 = Path(tmp) / "ue59"
            cap = ue59 / "Saved" / "CaptureSpeed" / "batch"
            (cap / "27").mkdir(parents=True, exist_ok=True)
            (cap / "28").mkdir(parents=True, exist_ok=True)

            calls: list = []
            with mock.patch.object(anim_ops.subprocess, "run", side_effect=fake_ab_runner(calls)):
                res = anim_ops.anim_capture_ab(
                    variant_a="27",
                    variant_b="28",
                    views="az90,game",
                    run_gpu=True,
                    root=tmp,
                    ue59_dir=str(ue59),
                )

            self.assertTrue(res["ok"])
            self.assertEqual(res["views"][0], "az180")
            pwsh_calls = [c for c in calls if any("pwsh" in str(x).lower() for x in c)]
            self.assertEqual(len(pwsh_calls), 1)
            self.assertIn("-Command", pwsh_calls[0])
            runs = parse_runs(pwsh_calls[0])
            self.assertEqual(len(runs), 2)
            for run in runs:
                self.assertEqual(len(run.split("|")), 5)
                self.assertTrue(run.endswith("|85,120"))

        # Multi-pair capture: one batch captures call (6 runs) and 3 ab_* dirs.
        with tempfile.TemporaryDirectory() as tmp:
            ue59 = Path(tmp) / "ue59"
            cap = ue59 / "Saved" / "CaptureSpeed" / "turnall"
            for rname in ("t180R_27", "t180R_28", "t090L_27", "t090L_28", "t090R_27", "t090R_28"):
                (cap / rname).mkdir(parents=True, exist_ok=True)
                (cap / rname / "assert.txt").write_text("ASSERT: OK\n", encoding="utf-8")

            pairs = [
                {"name": "180R", "a": "t180R_27", "b": "t180R_28", "seq": "mv_turn_180b", "variant_a": "27", "variant_b": "28"},
                {"name": "090L", "a": "t090L_27", "b": "t090L_28", "seq": "mv_turn_090a", "variant_a": "27", "variant_b": "28"},
                {"name": "090R", "a": "t090R_27", "b": "t090R_28", "seq": "mv_turn_090b", "variant_a": "27", "variant_b": "28"},
            ]
            calls: list = []
            with mock.patch.object(anim_ops.subprocess, "run", side_effect=fake_ab_runner(calls)):
                res = anim_ops.anim_capture_ab(
                    pairs=pairs,
                    batch="turnall",
                    run_gpu=True,
                    root=tmp,
                    ue59_dir=str(ue59),
                )

            self.assertTrue(res["ok"])
            batch_calls = [c for c in calls if any("runstart_batch_captures.ps1" in str(x) for x in c)]
            self.assertEqual(len(batch_calls), 1)
            runs = parse_runs(batch_calls[0])
            self.assertEqual(len(runs), 6)
            for run in runs:
                self.assertEqual(len(run.split("|")), 5)

            ab_calls = [c for c in calls if any("runstart_ab_video.py" in str(x) for x in c)]
            self.assertEqual(len(ab_calls), 3)
            ab_joined = " ".join(" ".join(map(str, c)) for c in ab_calls)
            for name in ("ab_180R", "ab_090L", "ab_090R"):
                self.assertIn(name, ab_joined)

            self.assertEqual(len(res["pairs"]), 3)

    def test_mcp_v2_schemas_accept_multipair_and_v2_intent(self) -> None:
        lst = mcp_server.process_request({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
        tools = {t["name"]: t for t in lst["result"]["tools"]}

        ab = tools["anim_capture_ab"]
        schema = ab.get("inputSchema", {})
        self.assertIn("pairs", schema.get("properties", {}))
        required = schema.get("required", [])
        self.assertNotIn("variant_a", required)
        self.assertNotIn("variant_b", required)

        res = mcp_server.process_request(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {
                    "name": "anim_apply_intent",
                    "arguments": {
                        "src": "AS_Base",
                        "dst": "AS_TestClip",
                        "intent_json": {
                            "turn_side": "l",
                            "arm_st_tuck_in_deg": 26.0,
                            "start_from_frame": 2,
                        },
                        "dry_run": True,
                    },
                },
            }
        )
        self.assertNotIn("error", res)
        self.assertFalse(res["result"]["isError"])
        payload = json.loads(res["result"]["content"][0]["text"])
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["validated_intent"]["turn_side"], "L")

    def test_geom_intent_math_tuck_in_and_start_from_frame(self) -> None:
        import geom_intent_ue
        from math import atan2, cos, degrees, pi, radians, sin, sqrt

        Vec3 = geom_intent_ue.Vec3

        def _dot(a, b):
            return a.x * b.x + a.y * b.y + a.z * b.z

        def _sub(a, b):
            return Vec3(a.x - b.x, a.y - b.y, a.z - b.z)

        def _len(a):
            return sqrt(_dot(a, a))

        up_vec = Vec3(0.0, 0.0, 1.0)
        fwd_vec = Vec3(0.0, 1.0, 0.0)

        def _theta_deg(v, out_dir):
            return degrees(atan2(_dot(v, out_dir), -_dot(v, up_vec)))

        def _tuck_vec(deg, out_dir):
            return Vec3(
                out_dir.x * sin(radians(deg)),
                out_dir.y * sin(radians(deg)),
                -cos(radians(deg)),
            )

        for out_dir in (Vec3(1.0, 0.0, 0.0), Vec3(-1.0, 0.0, 0.0)):
            # Flared and high-wing vectors tuck inward (theta strictly decreases).
            for deg in (45.0, 105.0):
                v = _tuck_vec(deg, out_dir)
                q = geom_intent_ue.tuck_in_quat(fwd_vec, out_dir, up_vec, v, 24.0)
                v_after = q.rotate_vector(v)
                self.assertLess(_theta_deg(v_after, out_dir), _theta_deg(v, out_dir))

            # Already-tucked vectors never rotate outward: the quat is identity.
            for deg in (-20.0, -45.0):
                v = _tuck_vec(deg, out_dir)
                q = geom_intent_ue.tuck_in_quat(fwd_vec, out_dir, up_vec, v, 24.0)
                v_after = q.rotate_vector(v)
                self.assertLess(abs(v_after.x - v.x), 1e-5)

        # signed_axis_quat_directed flips its rotation direction with the sign of deg.
        axis = Vec3(0.0, 0.0, 1.0)
        probe = Vec3(1.0, 0.0, 0.0)
        target = Vec3(0.0, 1.0, 0.0)
        rot_pos = geom_intent_ue.signed_axis_quat_directed(axis, 30.0, probe, target).rotate_vector(probe)
        rot_neg = geom_intent_ue.signed_axis_quat_directed(axis, -30.0, probe, target).rotate_vector(probe)
        self.assertGreater(rot_pos.y, 0.0)
        self.assertLess(rot_neg.y, 0.0)

        # Character axes stay continuous across a 180-degree turn.
        n = 40
        pelvis = [Vec3(100.0 * cos(pi * i / (n - 1)), 100.0 * sin(pi * i / (n - 1)), 90.0) for i in range(n)]
        thigh_l, thigh_r = [], []
        for i in range(n):
            th = pi * i / (n - 1)
            # Tangent heading (-sin, cos), left vector (-cos, -sin)
            lx, ly = -cos(th) * 12.0, -sin(th) * 12.0
            thigh_l.append(Vec3(pelvis[i].x + lx, pelvis[i].y + ly, 90.0))
            thigh_r.append(Vec3(pelvis[i].x - lx, pelvis[i].y - ly, 90.0))
        _, fwd_vecs = geom_intent_ue.compute_character_axes(pelvis, thigh_l, thigh_r)
        for f in range(n - 1):
            self.assertGreater(_dot(fwd_vecs[f], fwd_vecs[f + 1]), 0.5)

        # Two-bone IK preserves both segment lengths.
        hip_old = Vec3(0.0, 0.0, 90.0)
        knee_old = Vec3(0.0, 10.0, 50.0)
        foot_old = Vec3(0.0, 0.0, 10.0)
        l1_old = _len(_sub(knee_old, hip_old))
        l2_old = _len(_sub(foot_old, knee_old))
        hip_new = Vec3(0.0, 6.0, 88.0)
        foot_tgt = Vec3(0.0, 14.0, 22.0)
        knee_new, foot_out = geom_intent_ue.solve_2bone_ik(hip_new, foot_tgt, hip_old, knee_old, foot_old)
        self.assertLess(abs(_len(_sub(knee_new, hip_new)) - l1_old), 1e-3)
        self.assertLess(abs(_len(_sub(foot_out, knee_new)) - l2_old), 1e-3)

        # Envelopes are strictly zero before start_from_frame and rise after it.
        env0 = geom_intent_ue.compute_envelopes(0, 40, start_from_frame=2)
        env1 = geom_intent_ue.compute_envelopes(1, 40, start_from_frame=2)
        env4 = geom_intent_ue.compute_envelopes(4, 40, start_from_frame=2)
        self.assertEqual(max(env0.values()), 0.0)
        self.assertEqual(max(env1.values()), 0.0)
        self.assertGreater(env4["w3"], 0.0)

        # v1 -> v2 intent mapping translates legacy keys.
        mapped = geom_intent_ue.map_v1_to_v2_intent(
            {"com_shift_fwd_cm": 5.0, "arm_back_arc": 15.0, "w3_knee_drop": -6.0, "foot_pushoff_deg": 10.0}
        )
        self.assertEqual(mapped["pelvis_fwd_cm"], 5.0)
        self.assertEqual(mapped["arm_st_back_deg"], 15.0)
        self.assertEqual(mapped["arm_pv_fwd_deg"], 7.5)
        self.assertAlmostEqual(mapped["knee_drop"], 6.0 / 25.0)
        self.assertEqual(mapped["pivot_heel_deg"], 10.0)

    def test_anim_inspect_head_neck_metrics(self) -> None:
        n = 20
        j = _make_fake_joints(n)["joints"]
        j["root"] = [[0.0, 0.0, -float(i)] for i in range(n)]
        j["spine_05"] = [[0.0, 150.0, -float(i)] for i in range(n)]
        j["neck_01"] = [[0.0, 160.0, -float(i)] for i in range(n)]
        j["head"] = [[0.0, 170.0, -float(i)] for i in range(n)]   # straight: pitch ~0
        with tempfile.TemporaryDirectory() as td:
            d = Path(td) / "Saved" / "AnimPipeline" / "runstart"
            d.mkdir(parents=True)
            (d / "joints_AS_hn.json").write_text(json.dumps({"fps": 30, "joints": j}), encoding="utf-8")
            res = anim_ops.anim_inspect("AS_hn", root=td)
            self.assertTrue(res["ok"])
            hn = res["head_neck"]["head_pitch_rel_neck_deg"]
            self.assertIsNotNone(hn)
            self.assertAlmostEqual(abs(hn["max"]), 0.0, places=3)

    def test_anim_inspect_v2_metrics_and_splits(self) -> None:
        n = 40

        def frame_z(i: int) -> float:
            return -float(i) * 5.0

        joints: dict = {}
        joints["root"] = [[0.0, 0.0, frame_z(i)] for i in range(n)]

        for side, sx in (("l", -12.0), ("r", 12.0)):
            joints[f"thigh_MainPoint_{side}"] = [[sx, 90.0, frame_z(i)] for i in range(n)]
            joints[f"calf_MainPoint_{side}"] = [[sx, 48.0, frame_z(i)] for i in range(n)]
            joints[f"foot_MainPoint_{side}"] = [[sx, 10.0, frame_z(i)] for i in range(n)]
            joints[f"ball_MainPoint_{side}"] = [[sx, 3.0, frame_z(i) - 10.0] for i in range(n)]

        # Shoulders shifted -20 cm along -Z (local forward) relative to mid_hip -> positive local lean, negative world +Z lean.
        # Left arm: wing pose (elbow flared horizontally outward along -X) with 90-degree elbow bend.
        joints["upperarm_MainPoint_l"] = [[-15.0, 140.0, frame_z(i) - 20.0] for i in range(n)]
        joints["lowerarm_MainPoint_l"] = [[-40.0, 140.0, frame_z(i) - 20.0] for i in range(n)]
        joints["hand_MainPoint_l"] = [[-40.0, 140.0, frame_z(i) - 45.0] for i in range(n)]
        # Right arm: pure sagittal back-swing (elbow behind along +Z, no lateral component).
        joints["upperarm_MainPoint_r"] = [[15.0, 140.0, frame_z(i) - 20.0] for i in range(n)]
        joints["lowerarm_MainPoint_r"] = [[15.0, 140.0, frame_z(i) + 5.0] for i in range(n)]
        joints["hand_MainPoint_r"] = [[15.0, 115.0, frame_z(i) + 5.0] for i in range(n)]

        # Single-frame spike on the right hand at f=10.
        joints["hand_MainPoint_r"][10] = [15.0, 115.0, frame_z(10) + 5.0 + 40.0]

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            rs = root / "Saved" / "AnimPipeline" / "runstart"
            rs.mkdir(parents=True)
            (root / "Content").mkdir(parents=True)
            (rs / "joints_AS_Run.json").write_text(
                json.dumps({"fps": 30, "joints": joints}), encoding="utf-8"
            )

            res = anim_ops.anim_inspect("AS_Run", frame_range="0-39", root=root)
            self.assertTrue(res["ok"])

            self.assertGreater(res["torso_angles_deg"]["lean_local_deg"]["mean"], 10.0)
            self.assertLessEqual(res["torso_angles_deg"]["lean"]["mean"], 0.0)

            abduction = res["arm_angles_deg"]["upperarm_abduction_deg"]
            self.assertGreater(abduction["l"]["max"], 70.0)
            self.assertLess(abduction["r"]["max"], 5.0)

            self.assertAlmostEqual(
                res["arm_angles_deg"]["elbow_flex_deg"]["l"]["mean"], 90.0, delta=1.0
            )

            self.assertGreater(res["jitter_max"]["value"], 5.0)
            self.assertEqual(res["jitter_max"]["joint"], "hand_r")

            self.assertEqual(res["splits"]["f0_15"]["frame_range"], [0, 15])
            self.assertEqual(res["splits"]["f16_35"]["frame_range"], [16, 35])

    def test_anim_capture_ab_variant_id_aliases_and_assert_win_edge_cases(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            ue = Path(tmp)
            cap = ue / "Saved" / "CaptureSpeed" / "batch"
            (cap / "base27").mkdir(parents=True)
            (cap / "v29").mkdir(parents=True)
            with mock.patch.object(
                anim_ops.subprocess,
                "run",
                return_value=subprocess.CompletedProcess(args=[], returncode=0, stdout="ok", stderr=""),
            ):
                res = anim_ops.anim_capture_ab(
                    pairs=[
                        {
                            "a": "base27",
                            "b": "v29",
                            "seq": "mv_turn_180b",
                            "variant_a_id": "27",
                            "variant_b_id": "29",
                            "label": "180R",
                            "assert_win": None,
                        }
                    ],
                    run_gpu=False,
                    root=tmp,
                    ue59_dir=tmp,
                )
            self.assertEqual(
                res["runs_sent"],
                [
                    "base27|27|base27|mv_turn_180b|85,120",
                    "v29|29|v29|mv_turn_180b|85,120",
                ],
            )
            self.assertTrue(res["pairs"][0]["out_dir"].endswith("ab_180R"))

            res_single = anim_ops.anim_capture_ab(
                variant_a="27",
                variant_b="29",
                assert_win=[90, 115],  # type: ignore[arg-type]
                run_gpu=False,
                root=tmp,
                ue59_dir=tmp,
            )
            self.assertTrue(res_single["runs_sent"][0].endswith("|90,115"))
            self.assertTrue(res_single["runs_sent"][1].endswith("|90,115"))

    def test_anim_inspect_zero_frames_unequal_joints_and_inverted_range(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            rs = root / "Saved" / "AnimPipeline" / "runstart"
            rs.mkdir(parents=True)

            zero_j = {"fps": 30, "joints": {"root": [], "thigh_MainPoint_l": [], "thigh_MainPoint_r": []}}
            (rs / "joints_AS_Zero.json").write_text(json.dumps(zero_j), encoding="utf-8")
            zres = anim_ops.anim_inspect("AS_Zero", root=root)
            self.assertFalse(zres["ok"])
            self.assertIn("zero frames", zres["error"])

            uneq = _make_fake_joints(20, scale=1.5)
            uneq["joints"]["thigh_MainPoint_l"] = uneq["joints"]["thigh_MainPoint_l"][:5]
            uneq["joints"]["thigh_MainPoint_r"] = uneq["joints"]["thigh_MainPoint_r"][:5]
            (rs / "joints_AS_Unequal.json").write_text(json.dumps(uneq), encoding="utf-8")
            ures = anim_ops.anim_inspect("AS_Unequal", frame_range="0-19", root=root)
            self.assertTrue(ures["ok"])

            with self.assertRaises(ValueError):
                anim_ops.anim_inspect("AS_Unequal", frame_range="10-2", root=root)
            with self.assertRaises(ValueError):
                anim_ops.anim_inspect("AS_Unequal", frame_range=[10, 2], root=root)

    def test_validate_intent_rejects_bool_for_float_bounds(self) -> None:
        with self.assertRaises(ValueError):
            anim_ops.validate_intent({"turn_deg": True})
        with self.assertRaises(ValueError):
            anim_ops.validate_intent({"pelvis_fwd_cm": False})

    def test_compute_character_axes_preserves_handedness_on_flip(self) -> None:
        import geom_intent_ue

        Vec3 = geom_intent_ue.Vec3
        pelvis = [Vec3(0.0, float(i) * 10.0, 90.0) for i in range(6)]
        thigh_l = [Vec3(-12.0, float(i) * 10.0, 90.0) for i in range(4)] + [
            Vec3(12.0, float(i) * 10.0, 90.0) for i in range(4, 6)
        ]
        thigh_r = [Vec3(12.0, float(i) * 10.0, 90.0) for i in range(4)] + [
            Vec3(-12.0, float(i) * 10.0, 90.0) for i in range(4, 6)
        ]
        lat_vecs, fwd_vecs = geom_intent_ue.compute_character_axes(pelvis, thigh_l, thigh_r)
        for f in range(6):
            handed = lat_vecs[f].y * fwd_vecs[f].x - lat_vecs[f].x * fwd_vecs[f].y
            self.assertGreater(handed, 0.9)

    def test_ps1_gate_fail_propagation_and_assets_validation(self) -> None:
        import os
        import shutil

        repo_real = HERE.parents[3]
        src_sparse = repo_real / "Tools" / "CI" / "Run-SparseAnimStep.ps1"
        src_pipe = repo_real / "Tools" / "AnimPipeline" / "cards" / "runstart" / "Invoke-RunStartPipeline.ps1"

        with tempfile.TemporaryDirectory() as tmp:
            tpath = Path(tmp)
            wt = tpath / "mock_wt"
            ue = tpath / f"mock_ue_proj_{os.getpid()}"
            for base in (wt, ue):
                (base / "Tools" / "CI").mkdir(parents=True, exist_ok=True)
                (base / "Tools" / "AnimPipeline" / "cards" / "runstart").mkdir(parents=True, exist_ok=True)
                (base / "Saved" / "AnimPipeline" / "runstart").mkdir(parents=True, exist_ok=True)
                (base / "Saved" / "AnimPipeline" / "runloop").mkdir(parents=True, exist_ok=True)
                (base / "Saved" / "Logs" / "ci").mkdir(parents=True, exist_ok=True)
                (base / "Content").mkdir(parents=True, exist_ok=True)

            shutil.copy2(src_sparse, wt / "Tools" / "CI" / "Run-SparseAnimStep.ps1")
            shutil.copy2(src_pipe, ue / "Tools" / "AnimPipeline" / "cards" / "runstart" / "Invoke-RunStartPipeline.ps1")

            (ue / "GASP_Giustino2.uproject").write_text("{}", encoding="utf-8")
            (ue / "Saved" / "AnimPipeline" / "runloop" / "loop_relaxed_dump.npz").write_bytes(b"NPZ")
            (wt / "Tools" / "AnimPipeline" / "cards" / "runstart" / "dummy_step.py").write_text("pass\n", encoding="utf-8")
            (ue / "Tools" / "AnimPipeline" / "cards" / "runstart" / "dummy_step.py").write_text("pass\n", encoding="utf-8")
            (ue / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_clip_joints_ue.py").write_text("pass\n", encoding="utf-8")

            fail_gate_py = (
                "import sys\n"
                "print('GATES: FAIL ankles f0 -4.0 cm')\n"
                "sys.exit(0)\n"
            )
            (wt / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_pivot_gates.py").write_text(fail_gate_py, encoding="utf-8")
            (ue / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_pivot_gates.py").write_text(fail_gate_py, encoding="utf-8")

            ue_str = str(ue).replace("'", "''")
            run_timed_ps1 = (
                "param([string]$Kind, [string]$Log, [string]$Exe, [string]$ArgLine, [string]$WatchFile = '', [int]$MaxMinutes = 10)\n"
                f"$ueRoot = '{ue_str}'\n"
                "if ($ArgLine -like '*runstart_clip_joints_ue*') {\n"
                "    Set-Content -Path (Join-Path $ueRoot 'Saved\\AnimPipeline\\runstart\\joints_AssetA.json') -Value '{}' -Encoding UTF8\n"
                "} else {\n"
                "    Set-Content -Path (Join-Path $ueRoot 'Content\\AssetA.uasset') -Value 'dummy' -Encoding UTF8\n"
                "}\n"
                "exit 0\n"
            )
            (wt / "Tools" / "CI" / "Run-Timed.ps1").write_text(run_timed_ps1, encoding="utf-8")
            (ue / "Tools" / "CI" / "Run-Timed.ps1").write_text(run_timed_ps1, encoding="utf-8")
            (ue / "Tools" / "CI" / "Save-Assets.ps1").write_text("param([string]$Script, [string]$Tag)\nexit 0\n", encoding="utf-8")
            (ue / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_batch_captures.ps1").write_text(
                f"param([string[]]$Runs, [string]$SavedDir, [string]$Batch, [string]$Views)\nSet-Content -Path '{ue_str}\\batch_called.txt' -Value 'called'\nexit 0\n",
                encoding="utf-8",
            )
            (ue / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_ab_video.py").write_text("import sys\nsys.exit(0)\n", encoding="utf-8")

            sparse_ps1 = str(wt / "Tools" / "CI" / "Run-SparseAnimStep.ps1")
            script_rel = r"Tools\AnimPipeline\cards\runstart\dummy_step.py"

            p_fail = subprocess.run(
                ["pwsh", "-NoProfile", "-File", sparse_ps1, "-WorktreeDir", str(wt), "-Script", script_rel, "-Ue59Dir", str(ue), "-Assets", "AssetA", "-GateWin", "18"],
                capture_output=True, text=True,
            )
            self.assertEqual(p_fail.returncode, 1, msg=p_fail.stdout + p_fail.stderr)
            self.assertIn("ESITO: FAIL - gates falliti", p_fail.stdout)

            pipe_ps1 = str(ue / "Tools" / "AnimPipeline" / "cards" / "runstart" / "Invoke-RunStartPipeline.ps1")
            p_pipe = subprocess.run(
                ["pwsh", "-NoProfile", "-File", pipe_ps1, "-Script", script_rel, "-Asset", "AssetA", "-Runs", "a|27|AssetA|mv|85,120", "-Ue59Dir", str(ue), "-GateWin", "18"],
                capture_output=True, text=True,
            )
            self.assertEqual(p_pipe.returncode, 1, msg=p_pipe.stdout + p_pipe.stderr)
            self.assertFalse((ue / "batch_called.txt").exists())

            for bad_assets in ("  ,  ", "/"):
                p_refuse = subprocess.run(
                    ["pwsh", "-NoProfile", "-File", sparse_ps1, "-WorktreeDir", str(wt), "-Script", script_rel, "-Ue59Dir", str(ue), "-Assets", bad_assets],
                    capture_output=True, text=True,
                )
                self.assertEqual(p_refuse.returncode, 2, msg=p_refuse.stdout + p_refuse.stderr)
                self.assertIn("ESITO: RIFIUTATO", p_refuse.stdout)

            p_slash = subprocess.run(
                ["pwsh", "-NoProfile", "-File", sparse_ps1, "-WorktreeDir", str(wt), "-Script", script_rel, "-Ue59Dir", str(ue), "-Assets", "/Game/Path/AssetA/", "-GateWin", "18"],
                capture_output=True, text=True,
            )
            self.assertEqual(p_slash.returncode, 1, msg=p_slash.stdout + p_slash.stderr)
            self.assertIn("ESITO: FAIL - gates falliti per AssetA", p_slash.stdout)


    def test_frame_range_outside_clip_is_an_error(self) -> None:
        import geom_intent_ue

        self.assertEqual(geom_intent_ue.check_frame_range([8, 14], 62), (8, 14))
        with self.assertRaises(ValueError):
            geom_intent_ue.check_frame_range([105, 109], 62)
        with self.assertRaises(ValueError):
            geom_intent_ue.check_frame_range([8, 62], 62)
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            jp = root / "Saved" / "AnimPipeline" / "runstart" / "joints_w3.json"
            jp.parent.mkdir(parents=True)
            jp.write_text(json.dumps(_make_fake_joints(n=62)), encoding="utf-8")
            with self.assertRaises(ValueError) as cm:
                anim_ops.anim_apply_intent("w3", "w3x", {"head_lift_deg": 5, "frame_range": [105, 109]}, root=root, dry_run=True)
            self.assertIn("clip-local", str(cm.exception))
            ok = anim_ops.anim_apply_intent("w3", "w3x", {"head_lift_deg": 5, "frame_range": [8, 20]}, root=root, dry_run=True)
            self.assertTrue(ok["ok"])

    def test_range_envelope_reaches_full_amplitude_and_moves_with_range(self) -> None:
        import geom_intent_ue

        def peak(a, b, key="arm_clr"):
            return max(geom_intent_ue.compute_envelopes(f, 62, turn_side="none", frame_range=[a, b], mode="range")[key]
                       for f in range(62))

        self.assertAlmostEqual(peak(8, 20), 1.0, places=6)
        self.assertAlmostEqual(peak(30, 45), 1.0, places=6)      # a later range is no longer a no-op (turnaround mode: 0)
        late = geom_intent_ue.compute_envelopes(36, 62, turn_side="none", frame_range=[30, 45], mode="turnaround")
        self.assertAlmostEqual(late["arm_clr"], 0.0, places=6)
        env = [geom_intent_ue.compute_envelopes(f, 62, turn_side="none", frame_range=[8, 14], mode="range")["head"] for f in range(62)]
        self.assertEqual([i for i, e in enumerate(env) if e > 0][0], 8)
        self.assertEqual(env[7], 0.0)
        self.assertEqual(env[15], 0.0)
        self.assertEqual(anim_ops.validate_intent({"frame_range": [8, 20], "envelope": "range"})["envelope"], "range")
        with self.assertRaises(ValueError):
            anim_ops.validate_intent({"envelope": "banana"})
        self.assertEqual(geom_intent_ue.map_v1_to_v2_intent({"frame_range": [8, 20]})["envelope"], "range")
        self.assertEqual(geom_intent_ue.map_v1_to_v2_intent({"head_lift_deg": 5})["envelope"], "turnaround")

    def test_tool_description_lists_envelope_timings(self) -> None:
        tools = {t["name"]: t for t in mcp_server._tool_defs()}
        d = tools["anim_apply_intent"]["description"]
        for needle in ("CLIP-LOCAL", "arm_clr_deg 6.5-9.5", "head_lift_deg 3.5-7.5", "'range'", "envelope"):
            self.assertIn(needle, d)

    def test_register_variant_guards_and_dry_run(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            wt = Path(td) / "wt"
            ue = Path(td) / "ue59"
            clip = wt / "Content" / "GSTN" / "Animation" / "RunStart" / "AS_test_clip.uasset"
            clip.parent.mkdir(parents=True)
            clip.write_bytes(b"x")
            ch = ue / "Content" / "GSTN" / "Locomotion" / "StockBase" / "CHT_GSTN_StockMover_PoseMatch.uasset"
            ch.parent.mkdir(parents=True)
            ch.write_bytes(b"chooser")
            r = anim_ops.anim_register_variant("AS_test_clip", 60, root=wt, worktree_dir=str(wt), ue59_dir=str(ue), dry_run=True)
            self.assertTrue(r["ok"], r)
            self.assertEqual(r["plan"]["handoff_variant_id"], 70)
            self.assertFalse(anim_ops.anim_register_variant("AS_missing", 60, root=wt, worktree_dir=str(wt), ue59_dir=str(ue), dry_run=True)["ok"])
            with self.assertRaises(ValueError):
                anim_ops.anim_register_variant("AS_test_clip", 21, root=wt, worktree_dir=str(wt), ue59_dir=str(ue))
            # unregister restores the exact chooser bytes and removes the clip copy
            import hashlib
            mf = anim_ops._variant_manifest(wt, 60)
            bak = mf.parent / "chooser_backup_60"
            bak.mkdir(parents=True)
            (bak / ch.name).write_bytes(b"chooser")
            copied = ue / "Content" / "GSTN" / "Animation" / "RunStart" / "AS_test_clip.uasset"
            copied.parent.mkdir(parents=True)
            copied.write_bytes(b"x")
            ch.write_bytes(b"chooser+variant")
            mf.write_text(json.dumps({"ue59": str(ue), "backup_dir": str(bak), "copied": [str(copied.relative_to(ue))],
                                      "chooser_sha_before": hashlib.sha256(b"chooser").hexdigest()}), encoding="utf-8")
            u = anim_ops.anim_unregister_variant(60, root=wt, worktree_dir=str(wt))
            self.assertTrue(u["ok"], u)
            self.assertEqual(ch.read_bytes(), b"chooser")
            self.assertFalse(copied.exists())
            self.assertFalse(mf.exists())


if __name__ == "__main__":
    unittest.main()

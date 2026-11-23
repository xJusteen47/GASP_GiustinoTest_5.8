"""Unit tests for gstn-anim-mcp (pure Python, no UE editor required)."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

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
            ["anim_inspect", "anim_apply_intent", "anim_gate_check", "anim_capture_ab", "anim_rollback"],
        )


if __name__ == "__main__":
    unittest.main()

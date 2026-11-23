"""Tests for the cheap/fast MCP actions: compact replies, stable prefix, dry_run plan, combined tool, cache log."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

MCP_DIR = Path(__file__).resolve().parents[1] / "gstn_anim_mcp"
if str(MCP_DIR) not in sys.path:
    sys.path.insert(0, str(MCP_DIR))

import anim_ops  # noqa: E402
import cache_log  # noqa: E402
import compact  # noqa: E402
import mcp_server  # noqa: E402
from test_gstn_anim_mcp import _make_fake_joints  # noqa: E402


class TestCompact(unittest.TestCase):
    def test_reply_max_5_lines_and_details_file(self) -> None:
        big = {"ok": True, "asset": "A", "gates": {"status": "OK", "output": "x\n" * 500},
               "conservation": {"status": "OK", "failures": []}}
        with tempfile.TemporaryDirectory() as tmp:
            text = compact.compact_reply("anim_gate_check", big, Path(tmp))
            lines = text.splitlines()
            self.assertLessEqual(len(lines), 5)
            self.assertTrue(lines[0].startswith("OK anim_gate_check"))
            saved = json.loads(Path(lines[-1].split("details: ", 1)[1]).read_text(encoding="utf-8"))
            self.assertEqual(saved["gates"]["output"], big["gates"]["output"])

    def test_error_keeps_message_and_fail_head(self) -> None:
        text = compact.compact_reply("anim_inspect", {"ok": False, "error": "joints file not found: /x"}, None)
        self.assertTrue(text.startswith("FAIL anim_inspect"))
        self.assertIn("joints file not found", text)
        self.assertLessEqual(len(text.splitlines()), 5)


class TestCompactIsOptIn(unittest.TestCase):
    def _call(self, args: dict) -> str:
        req = {"jsonrpc": "2.0", "id": 1, "method": "tools/call",
               "params": {"name": "anim_apply_intent",
                          "arguments": {"src": "A", "dst": "B", "intent_json": {"head_lift_deg": 5}, "dry_run": True, "allow_geom": True, **args}}}
        return mcp_server.process_request(req)["result"]["content"][0]["text"]

    def test_default_is_full_unindented_json(self) -> None:
        with mock.patch.dict("os.environ", {}, clear=False):
            text = self._call({})
        d = json.loads(text)
        self.assertEqual(len(text.splitlines()), 1)
        self.assertIn("plan", d)
        self.assertIn("bones", d["plan"])

    def test_opt_in_by_arg_and_env(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(mcp_server, "_out_dir", return_value=Path(tmp)):
            self.assertIn("details: ", self._call({"compact": True}))
            with mock.patch.dict("os.environ", {"GSTN_MCP_COMPACT": "1"}):
                self.assertIn("details: ", self._call({}))


class TestStablePrefix(unittest.TestCase):
    def test_tools_list_is_byte_identical_and_unique(self) -> None:
        a = json.dumps(mcp_server._tool_defs(), sort_keys=False)
        b = json.dumps(mcp_server._tool_defs(), sort_keys=False)
        self.assertEqual(a, b)
        names = [t["name"] for t in mcp_server._tool_defs()]
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(names[0], "anim_inspect")  # fixed order: new tools never go in front


class TestDryRunPlan(unittest.TestCase):
    def test_plan_reports_bones_frames_and_degrees(self) -> None:
        clean = anim_ops.validate_intent({"arm_clr_deg": 12, "frame_range": [8, 20]})
        plan = anim_ops.plan_intent(clean, 62)
        p = plan["params"]["arm_clr_deg"]
        self.assertAlmostEqual(p["reached"], 12.0, places=1)   # range envelope plateau reaches full value
        self.assertGreaterEqual(p["frames"][0], 7)
        self.assertLessEqual(p["frames"][1], 21)
        self.assertIn("upperarm_l", p["bones"])
        self.assertEqual(plan["bones"], ["upperarm_l", "upperarm_r"])

    def test_turnaround_envelope_reaches_partial_and_unset_params_skipped(self) -> None:
        plan = anim_ops.plan_intent(anim_ops.validate_intent({"turn_side": "L", "head_lift_deg": 10.0}), 62)
        self.assertEqual(list(plan["params"]), ["head_lift_deg"])
        self.assertTrue(0 < plan["params"]["head_lift_deg"]["reached"] <= 10.0)

    def test_dry_run_has_no_side_effects(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "Saved" / "AnimPipeline" / "runstart").mkdir(parents=True)
            (root / "Saved" / "AnimPipeline" / "runstart" / "joints_AS_Base.json").write_text(
                json.dumps(_make_fake_joints(30)), encoding="utf-8")
            before = sorted(p.name for p in root.rglob("*"))
            res = anim_ops.anim_apply_intent("AS_Base", "AS_New", {"head_lift_deg": 5, "frame_range": [4, 20]},
                                             root=root, dry_run=True, allow_geom=True)
            self.assertTrue(res["dry_run"])
            self.assertEqual(res["plan"]["n_frames"], 30)
            self.assertFalse(res["plan"]["n_frames_assumed"])
            self.assertEqual(before, sorted(p.name for p in root.rglob("*")))


class TestRunIntent(unittest.TestCase):
    def test_pass_and_fail_summary(self) -> None:
        ok_apply = {"ok": True, "returncode": 0, "intent_file": "i", "snapshot_dir": "s"}
        with mock.patch.object(anim_ops, "anim_apply_intent", return_value=ok_apply), \
             mock.patch.object(anim_ops, "anim_gate_check",
                               return_value={"ok": True, "gates": {"status": "OK"}, "conservation": {"status": "OK"}}):
            r = anim_ops.anim_run_intent("A", "B", {"head_lift_deg": 3})
            self.assertEqual(r["verdict"], "PASS")
        with mock.patch.object(anim_ops, "anim_apply_intent", return_value=ok_apply), \
             mock.patch.object(anim_ops, "anim_gate_check",
                               return_value={"ok": False, "gates": {"status": "FAIL"}, "conservation": {"status": "OK"}}):
            r = anim_ops.anim_run_intent("A", "B", {"head_lift_deg": 3})
            self.assertEqual(r["verdict"], "FAIL")
            self.assertIn("anim_rollback", r["next"])

    def test_apply_failure_skips_gates(self) -> None:
        with mock.patch.object(anim_ops, "anim_apply_intent", return_value={"ok": False, "error": "boom"}), \
             mock.patch.object(anim_ops, "anim_gate_check") as gc:
            r = anim_ops.anim_run_intent("A", "B", {"head_lift_deg": 3})
            self.assertFalse(r["ok"])
            gc.assert_not_called()

    def test_registered_in_server(self) -> None:
        with mock.patch.object(anim_ops, "anim_run_intent", return_value={"ok": True}) as m:
            mcp_server.handle_tool_call("anim_run_intent", {"src": "A", "dst": "B", "intent_json": {}, "dry_run": True})
            self.assertTrue(m.call_args.kwargs["dry_run"])


class TestCacheLog(unittest.TestCase):
    def test_records_hit_miss_and_ratio_and_missing_fields(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "c.jsonl"
            e = cache_log.record_usage({"prompt_cache_hit_tokens": 900, "prompt_cache_miss_tokens": 100,
                                        "prompt_tokens": 1000}, "t1", log)
            self.assertEqual(e["hit_ratio"], 0.9)
            cache_log.record_usage({"prompt_tokens": 50}, "no-fields", log)
            s = cache_log.summary(log)
            self.assertEqual((s["calls"], s["no_cache_fields"], s["hit"], s["miss"]), (2, 1, 900, 100))
            self.assertEqual(s["hit_ratio"], 0.9)


if __name__ == "__main__":
    unittest.main()

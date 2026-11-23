"""T0 preparation tests: pre-capture gate, capture plan/cache, review pack, manifest, silhouette + motion tools, fixes. Offline."""
from __future__ import annotations

import copy
import hashlib
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
import capture_move  # noqa: E402
import manifest  # noqa: E402
import mcp_server  # noqa: E402
import motion_overlay  # noqa: E402
import review_pack  # noqa: E402
import silhouette  # noqa: E402
import viewproj  # noqa: E402
from test_gstn_anim_mcp import _make_fake_joints  # noqa: E402

BASE, VAR = "AS_K6_Settle", "AS_K6_Settle_ft"
CLIP_BASE, CLIP_VAR = "/Game/Anim/K6/AS_K6_Settle", "/Game/Anim/K6/AS_K6_Settle_ft"


def _write_joints(root: Path, name: str, joints: dict) -> None:
    d = root / "Saved" / "AnimPipeline" / "runstart"
    d.mkdir(parents=True, exist_ok=True)
    (d / f"joints_{name}.json").write_text(json.dumps({"fps": 30, "joints": joints}), encoding="utf-8")


def _shifted(delta_cm: float) -> dict:
    j = copy.deepcopy(_make_fake_joints(20)["joints"])
    for k in ("hand_MainPoint_l", "hand_MainPoint_r"):
        j[k] = [[p[0], p[1] + delta_cm, p[2]] for p in j[k]]
    return j


def _make_repo(root: Path, delta_cm: float = 5.0, hitbox: str = "fresh") -> str:
    """Repo with two clips, joints dumps, a PASS report + dry run for the variant; returns the intent hash."""
    for n, data in ((BASE, b"base-bytes"), (VAR, b"var-bytes")):
        f = root / "Content" / "Anim" / "K6" / f"{n}.uasset"
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_bytes(data)
    _write_joints(root, BASE, _shifted(0.0))
    _write_joints(root, VAR, _shifted(delta_cm))
    h = manifest.intent_hash(BASE, VAR, {"follow_through": {"chain": "arms", "anchor": 20}})
    manifest.record_dry_run(root, VAR, h)
    manifest.record_report(root, VAR, h, {"ok": True, "verdict": "PASS", "links": [], "conservation": {"status": "OK"}})
    if hitbox:
        sha = hashlib.sha256(b"var-bytes" if hitbox == "fresh" else b"old").hexdigest()
        hb = root / "Saved" / "AnimR2" / "setup_dropkick_hitboxes.json"
        hb.parent.mkdir(parents=True, exist_ok=True)
        hb.write_text(json.dumps({"timeline": "TL", "clips": {CLIP_VAR: sha}}), encoding="utf-8")
    return h


def _runs(h: str) -> list:
    return [{"name": "base", "asset": BASE, "clips": [CLIP_BASE], "cvars": "a 1"},
            {"name": "ft", "asset": VAR, "clips": [CLIP_VAR], "cvars": "a 1", "intent_hash": h}]


class TestGate(unittest.TestCase):
    def _gate(self, root: Path, h: str, **kw):
        return capture_move.precapture_gate(root, VAR, BASE, h, [CLIP_VAR], (0, 19), "settle", **kw)

    def test_all_pass(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            g = self._gate(root, h)
            self.assertTrue(g["ok"], g)
            self.assertEqual({c["check"] for c in g["checks"]}, {"report_pass", "dry_run_exists", "hitbox_fresh", "readable", "attempts"})

    def test_report_not_pass_or_other_intent(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            manifest.record_report(root, VAR, h, {"ok": False, "verdict": "FAIL", "links": []})
            self.assertFalse(self._gate(root, h)["ok"])
            manifest.record_report(root, VAR, h, {"ok": True, "verdict": "PASS", "links": [], "conservation": {"status": "FAIL"}})
            self.assertIn("conservation_ok", self._gate(root, h)["error"])
            manifest.record_report(root, VAR, h, {"ok": True, "verdict": "PASS", "links": []})
            self.assertTrue(self._gate(root, h)["ok"])
            self.assertFalse(self._gate(root, "deadbeefdeadbeef")["ok"])   # no dry_run, report for another intent

    def test_missing_dry_run(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            (root / "Saved" / "AnimPipeline" / "manifest" / f"{VAR}.json").unlink()
            manifest.record_report(root, VAR, h, {"ok": True, "verdict": "PASS", "links": []})
            g = self._gate(root, h)
            self.assertFalse(g["ok"])
            self.assertIn("dry_run", g["error"])

    def test_stale_or_missing_hitbox_sync(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root, hitbox="stale")
            g = self._gate(root, h)
            self.assertFalse(g["ok"])
            self.assertIn("rerun the hitbox script", g["error"])
            hb = root / "Saved" / "AnimR2" / "setup_dropkick_hitboxes.json"
            hb.unlink()
            self.assertIn("rerun the hitbox script", self._gate(root, h)["error"])
            self.assertTrue(self._gate(root, h, hitbox_required=False)["ok"])
            # a different generic file naming the clip with the current sha is accepted
            (hb.parent / "setup_other_hitboxes.json").write_text(json.dumps(
                {"timeline": "T", "clips": {CLIP_VAR: hashlib.sha256(b"var-bytes").hexdigest()}}), encoding="utf-8")
            self.assertTrue(self._gate(root, h)["ok"])

    def test_too_small_to_read(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root, delta_cm=1.0)
            g = self._gate(root, h)
            self.assertFalse(g["ok"])
            self.assertIn("too small to read", g["error"])
            self.assertTrue(self._gate(root, h, min_move_cm=0.5)["ok"])   # configurable

    def test_rotation_alone_is_enough(self) -> None:
        j0 = _make_fake_joints(20)["joints"]
        j1 = copy.deepcopy(j0)
        # tilt the forearm: hand moves < 2 cm only, but the lowerarm->hand direction turns more than 3 deg
        j1["hand_MainPoint_l"] = [[p[0] - 1.8, p[1], p[2]] for p in j1["hand_MainPoint_l"]]
        ch = capture_move.measure_change(j1, j0)
        self.assertLess(ch["peak_cm"], 2.0)
        self.assertGreater(ch["peak_deg"], 3.0)

    def test_third_attempt_refused(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            manifest.bump_attempt(root, VAR, "settle")
            self.assertTrue(self._gate(root, h)["ok"])
            manifest.bump_attempt(root, VAR, "settle")
            g = self._gate(root, h)
            self.assertFalse(g["ok"])
            self.assertIn("max 2", g["error"])


class FakeRunner:
    def __init__(self, repo: Path, rc: int = 0) -> None:
        self.calls: list = []
        self.repo, self.rc = repo, rc

    def __call__(self, cmd, env, cwd):
        self.calls.append((cmd, env))
        if cmd[0] == "pwsh":   # emulate the capture: frames for every run of runs.json
            runs = json.loads(Path(env["PIE_CAPTURE_RUNS"]).read_text(encoding="utf-8"))
            from PIL import Image
            for r in runs:
                for s in r["sequences"]:
                    for v in env["PIE_CAPTURE_VIEWS"].split(","):
                        d = Path(env["PIE_CAPTURE_RUNS"]).parent / r["name"] / s / v
                        d.mkdir(parents=True, exist_ok=True)
                        for f in range(95, 125):
                            Image.new("RGB", (32, 24), (r["name"] == "base" and 200 or 60, 80, 80)).save(d / f"f{f:03d}.png")
        return {"returncode": self.rc if cmd[0] == "pwsh" else 0, "stdout": ""}


class TestCaptureMove(unittest.TestCase):
    KW = dict(kit="KIT_Test_PlaytestTBK6", sequences=["dropkick_hit"], move="dk6", round="ft1", objective="settle",
              edit_window=[0, 19], anchor=110, faf=120, build="b1",
              assert_spec={"dropkick_hit": [{"clip": "AM_*", "window": [100, 124]}]})

    def test_gate_failure_blocks_everything(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root, hitbox="stale")
            fr = FakeRunner(root)
            r = capture_move.anim_capture_move(variant_runs=_runs(h), root=root, runner=fr, **self.KW)
            self.assertFalse(r["ok"])
            self.assertEqual(r["stage"], "precapture_gate")
            self.assertEqual(fr.calls, [])

    def test_dry_run_plan(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            r = capture_move.anim_capture_move(variant_runs=_runs(h), root=root, dry_run=True, **self.KW)
            self.assertTrue(r["ok"] and r["dry_run"], r)
            p = r["plan"]
            self.assertEqual(p["out_dir"], "docs/notes/reviews/dk6/ft1")
            self.assertEqual([x["name"] for x in p["runs_json"]], ["base", "ft"])
            self.assertEqual(p["runs_json"][0]["sequences"], ["dropkick_hit"])
            self.assertEqual(p["views"], "az90,az180,game")
            self.assertEqual(p["command"][:3], ["pwsh", "-NoProfile", "-File"])
            self.assertTrue(p["command"][3].endswith("pie_capture_timed.ps1"))
            self.assertIn("-Assert", p["command"])
            self.assertEqual(p["command"][p["command"].index("-Kit") + 1], "KIT_Test_PlaytestTBK6")
            self.assertEqual(manifest.attempts(root, VAR, "settle"), 0, "dry_run must not count as an attempt")
            self.assertFalse((root / "Saved" / "AnimPipeline" / "captures").exists())

    def test_real_run_then_cache(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            fr = FakeRunner(root)
            r = capture_move.anim_capture_move(variant_runs=_runs(h), root=root, runner=fr, **self.KW)
            self.assertTrue(r["ok"], r)
            self.assertTrue(r["captured"])
            self.assertEqual(manifest.attempts(root, VAR, "settle"), 1)
            self.assertEqual(sum(1 for c, _ in fr.calls if c[0] == "pwsh"), 1, "ONE editor session for base and variant")
            env = fr.calls[0][1]
            self.assertEqual(env["PIE_CAPTURE_VIEWS"], "az90,az180,game")
            out = root / "docs/notes/reviews/dk6/ft1"
            for v in ("az90", "az180", "game"):
                self.assertTrue((out / f"grid_dropkick_hit_{v}.png").exists())
            self.assertLessEqual(r["pack"]["frames"].__len__(), 8)
            videos = [c for c, _ in fr.calls if c[0] == "python"]
            self.assertEqual(len(videos), 4)   # base + variant, real + slow4x
            self.assertTrue(any("--fps=15" in c for c in videos) and any("--fps=60" in c for c in videos))
            self.assertIn("pwsh", fr.calls[0][0][0])
            # second round: identical clips/intent -> nothing to capture, no attempt consumed
            fr2 = FakeRunner(root)
            r2 = capture_move.anim_capture_move(variant_runs=_runs(h), root=root, runner=fr2, **self.KW)
            self.assertTrue(r2["ok"])
            self.assertFalse(r2["captured"])
            self.assertEqual(sorted(r2["reused"]), ["base", "ft"])
            self.assertFalse(any(c[0] == "pwsh" for c, _ in fr2.calls))
            self.assertEqual(manifest.attempts(root, VAR, "settle"), 1)

    def test_base_cached_new_variant_captures_variant_only(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            capture_move.anim_capture_move(variant_runs=_runs(h), root=root, runner=FakeRunner(root), **self.KW)
            # a new variant intent: same base, new clip bytes + new hash (gate requires its own report/dry run)
            h2 = manifest.intent_hash(BASE, VAR, {"follow_through": {"chain": "arms", "anchor": 22}})
            manifest.record_dry_run(root, VAR, h2)
            manifest.record_report(root, VAR, h2, {"ok": True, "verdict": "PASS", "links": []})
            (root / "Content/Anim/K6" / f"{VAR}.uasset").write_bytes(b"var-bytes-2")
            hb = root / "Saved/AnimR2/setup_dropkick_hitboxes.json"
            hb.write_text(json.dumps({"clips": {CLIP_VAR: hashlib.sha256(b"var-bytes-2").hexdigest()}}), encoding="utf-8")
            plan = capture_move.anim_capture_move(variant_runs=_runs(h2), root=root, dry_run=True, **dict(self.KW, round="ft2"))
            self.assertEqual(plan["plan"]["capture_runs"], ["ft"])
            self.assertEqual(list(plan["plan"]["reused"]), ["base"])
            # base cache key changes with views, cvars or build
            changed = capture_move.anim_capture_move(variant_runs=_runs(h2), root=root, dry_run=True, **dict(self.KW, build="b2"))
            self.assertEqual(changed["plan"]["capture_runs"], ["base", "ft"])

    def test_capture_failure_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            r = capture_move.anim_capture_move(variant_runs=_runs(h), root=root, runner=FakeRunner(root, rc=3), **self.KW)
            self.assertFalse(r["ok"])
            self.assertEqual(r["stage"], "capture")

    def test_mcp_tool_listed_last_and_validates(self) -> None:
        names = [t["name"] for t in mcp_server._tool_defs()]
        self.assertEqual(names[-3:], ["anim_capture_move", "anim_silhouette_check", "anim_motion_overlay"])
        r = capture_move.anim_capture_move(variant_runs=[{"name": "base", "asset": BASE}], root="/nonexistent", **self.KW)
        self.assertFalse(r["ok"])


class TestReviewPack(unittest.TestCase):
    def test_select_frames(self) -> None:
        fr = review_pack.select_frames((0, 200), anchor=100, faf=150, radius=4)
        self.assertLessEqual(len(fr), 8)
        self.assertIn(100, fr)
        self.assertIn(150, fr)
        self.assertTrue(all(abs(f - 100) <= 4 or abs(f - 150) <= 4 for f in fr))
        self.assertEqual(review_pack.select_frames((10, 12), 11, None, max_images=8), [10, 11, 12])
        even = review_pack.select_frames((0, 100), None, None)
        self.assertEqual(len(even), 8)
        self.assertEqual((even[0], even[-1]), (0, 100))
        self.assertLessEqual(len(review_pack.select_frames((0, 50), 25, 26, radius=30, max_images=5)), 5)

    def test_grid_has_row_per_run_and_marks_missing(self) -> None:
        from PIL import Image
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            for run in ("base", "ft"):
                d = root / run / "s" / "az90"
                d.mkdir(parents=True)
                for f in (10, 11):
                    Image.new("RGB", (40, 30), (255, 0, 0)).save(d / f"f{f:03d}.png")
            g = review_pack.build_grid({"base": root / "base", "ft": root / "ft"}, "s", "az90", [10, 11, 12], root / "g.png", cell=64)
            im = Image.open(g["png"])
            self.assertEqual(im.size, (3 * 64, 2 * 64))
            self.assertEqual(len(g["missing"]), 2)


class TestManifestAndBuild(unittest.TestCase):
    def test_needs_build_by_hash(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            src = root / "Source" / "Mod" / "A.cpp"
            src.parent.mkdir(parents=True)
            src.write_text("int a;")
            (root / "Plugins" / "P" / "Source").mkdir(parents=True)
            (root / "Plugins" / "P" / "Source" / "B.h").write_text("x")
            self.assertTrue(manifest.needs_build(root)["needs_build"])
            manifest.write_build_stamp(root)
            self.assertFalse(manifest.needs_build(root)["needs_build"])
            src.write_text("int b;")
            self.assertTrue(manifest.needs_build(root)["needs_build"])
            (root / "Plugins" / "P" / "Source" / "B.h").write_text("y")
            manifest.write_build_stamp(root)
            (root / "Plugins" / "P" / "Source" / "B.h").write_text("y")   # same content, new mtime: no rebuild
            self.assertFalse(manifest.needs_build(root)["needs_build"])

    def test_steps_usage_and_costi_line(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            manifest.record_step(root, VAR, "build", 12.5)
            manifest.record_step(root, VAR, "capture", 20.0,
                                 usage={"prompt_tokens": 1000, "completion_tokens": 200, "prompt_cache_hit_tokens": 800,
                                        "prompt_cache_miss_tokens": 200})
            m = manifest.load(root, VAR)
            self.assertEqual([s["step"] for s in m["steps"]], ["build", "capture"])
            self.assertTrue((root / "Saved" / "AnimPipeline" / "cache_usage.jsonl").exists())
            line = manifest.costi_line(root, VAR, "T1 dropkick, ft", "dropkick-k6-ft", "Sonnet medium", "PASS", usd=3.5, date="2026-11-24")
            self.assertEqual(line, '2026-11-24,"T1 dropkick, ft",dropkick-k6-ft,desktop,claude_max,Sonnet medium,32.5,1200,3.5,,PASS')
            self.assertEqual(len(line.split(",")), len(manifest.COSTI_COLUMNS.split(",")) + 1)   # +1 for the quoted comma

    def test_intent_hash_stable(self) -> None:
        a = manifest.intent_hash("x", "y", {"b": 1, "a": 2})
        self.assertEqual(a, manifest.intent_hash("/Game/p/x", "y", {"a": 2, "b": 1}))
        self.assertNotEqual(a, manifest.intent_hash("x", "y", {"a": 3, "b": 1}))

    def test_server_records_dry_run_and_run_intent_records_report(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            with mock.patch.object(anim_ops, "_repo_root", return_value=root):
                intent = {"follow_through": {"chain": "arms", "anchor": 5, "mass": "light"}}
                res = mcp_server.handle_tool_call("anim_apply_intent", {"src": "A", "dst": "B", "intent_json": intent, "dry_run": True})
                self.assertTrue(manifest.has_dry_run(root, "B", res["intent_hash"]))
                ok_apply = {"ok": True, "follow_through_report": None, "returncode": 0}
                with mock.patch.object(anim_ops, "anim_apply_intent", return_value=ok_apply), \
                     mock.patch.object(anim_ops, "anim_gate_check", return_value={"ok": True, "gates": {"status": "OK"}, "conservation": {"status": "OK"}}):
                    r = anim_ops.anim_run_intent("A", "B", intent, root=root)
                rep = manifest.load(root, "B")["last_report"]
                self.assertEqual(rep["intent_hash"], r["intent_hash"])
                self.assertTrue(manifest.report_is_pass(rep, r["intent_hash"])[0])


class TestFixes(unittest.TestCase):
    def test_variant_lower_bound_40(self) -> None:
        for vid in (30, 39):
            with self.assertRaises(ValueError):
                anim_ops.anim_register_variant("AS_x", vid, root="/nonexistent", dry_run=True)

    def test_geometric_refused_on_non_runstart(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(ValueError) as cm:
                anim_ops.anim_apply_intent("AS_Dropkick", "AS_Dropkick2", {"torso_bank_deg": 5}, root=t, dry_run=True)
            self.assertIn("in-place", str(cm.exception))
            ok = anim_ops.anim_apply_intent("AS_Dropkick", "AS_Dropkick2", {"torso_bank_deg": 5}, root=t, dry_run=True, allow_geom=True)
            self.assertTrue(ok["ok"])
            self.assertTrue(anim_ops.anim_apply_intent("AS_GSTN_RunStart_w3", "AS_GSTN_RunStart_w3x", {"torso_bank_deg": 5}, root=t, dry_run=True)["ok"])
            ft = anim_ops.anim_apply_intent("AS_Dropkick", "AS_Dropkick2", {"follow_through": {"chain": "arms", "anchor": 5}}, root=t, dry_run=True)
            self.assertTrue(ft["ok"])


def _pose(n: int = 12, arm_z: float = 50.0) -> dict:
    """Standing pose facing +x, arms out to the sides (z = character right), constant over n frames."""
    J: dict = {}

    def put(k, p):
        J[k] = [list(p) for _ in range(n)]

    put("root", (0, 0, 0)); put("pelvis", (0, 100, 0))
    for i, y in enumerate((110, 120, 130, 140, 150), 1):
        put(f"spine_0{i}", (0, y, 0))
    put("neck_01", (0, 160, 0)); put("head", (0, 175, 0))
    for s, sg in (("l", -1), ("r", 1)):
        put(f"thigh_MainPoint_{s}", (0, 95, 10 * sg)); put(f"calf_MainPoint_{s}", (0, 50, 10 * sg))
        put(f"foot_MainPoint_{s}", (0, 5, 10 * sg)); put(f"ball_MainPoint_{s}", (10, 2, 10 * sg))
        put(f"upperarm_MainPoint_{s}", (0, 150, 18 * sg)); put(f"lowerarm_MainPoint_{s}", (0, 150, 18 * sg + arm_z * sg / 2))
        put(f"hand_MainPoint_{s}", (0, 150, 18 * sg + arm_z * sg))
    return J


def _tuck(n: int = 12) -> dict:
    """Tight dropkick-like tuck facing +x: knees forward to the chest, heels at the glutes, arms folded on the chest, head down."""
    J: dict = {}

    def put(k, p):
        J[k] = [list(p) for _ in range(n)]

    put("root", (0, 0, 0)); put("pelvis", (0, 100, 0))
    for i, y in enumerate((110, 120, 130, 140, 150), 1):
        put(f"spine_0{i}", (0, y, 0))
    put("neck_01", (0, 160, 0)); put("head", (8, 158, 0))
    for s, sg in (("l", -1), ("r", 1)):
        put(f"thigh_MainPoint_{s}", (0, 95, 10 * sg)); put(f"calf_MainPoint_{s}", (35, 120, 9 * sg))
        put(f"foot_MainPoint_{s}", (5, 100, 8 * sg)); put(f"ball_MainPoint_{s}", (12, 100, 8 * sg))
        put(f"upperarm_MainPoint_{s}", (0, 150, 18 * sg)); put(f"lowerarm_MainPoint_{s}", (5, 135, 20 * sg))
        put(f"hand_MainPoint_{s}", (15, 145, 8 * sg))
    return J


class TestSilhouette(unittest.TestCase):
    def test_standing_is_readable_hidden_arms_only_listed(self) -> None:
        # skill-gaps 2026-11-23: arms on the torso line from the side are listed as hidden but the legs read: NOT flagged
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            _write_joints(root, "AS_pose", _pose(arm_z=50))
            r = silhouette.anim_silhouette_check("AS_pose", "0-11", "az90,az180", root=root)
            self.assertTrue(r["ok"], r)
            side, behind = r["views"]["az90"], r["views"]["az180"]
            self.assertEqual(side["flagged_frames"], [])
            self.assertEqual(behind["flagged_frames"], [])
            self.assertEqual(r["findings"], [])
            fr = json.loads(Path(r["details"]).read_text(encoding="utf-8"))["views"]["az90"]["frames"][0]
            self.assertIn("hand_l", fr["hidden"])
            self.assertGreaterEqual(fr["max_protrusion_cm"], 12.0)
            self.assertTrue(Path(side["png"]).exists() and Path(behind["png"]).exists())
            self.assertLessEqual(len(r["summary"]), 5)
            from PIL import Image
            self.assertLess(Image.open(side["png"]).size[0], 1100)

    def test_tuck_is_a_ball_from_behind_not_from_the_side(self) -> None:
        # the dropkick K6/K7 finding: a tight tuck reads from the side (knees out front) and is a shapeless ball from behind
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            _write_joints(root, "AS_tuck", _tuck())
            r = silhouette.anim_silhouette_check("AS_tuck", "0-11", "az90,az180", root=root, out="docs/notes/reviews/x/r1")
            side, behind = r["views"]["az90"], r["views"]["az180"]
            self.assertEqual(side["flagged_frames"], [])
            self.assertEqual(len(behind["flagged_frames"]), 12)
            self.assertEqual(len(r["findings"]), 1)
            self.assertIn("az180: 12/12 frames read as a shapeless ball", r["findings"][0])
            self.assertIn("max protrusion", r["findings"][0])
            self.assertTrue(r["summary"][0].startswith("FINDING az180"))
            # the JSON lands in the review folder (committable), with the per-frame metric
            self.assertEqual(Path(r["details"]), root / "docs/notes/reviews/x/r1/silhouette.json")
            fr = json.loads(Path(r["details"]).read_text(encoding="utf-8"))["views"]["az180"]["frames"][0]
            self.assertEqual(fr["reason"], "blob")
            self.assertLess(fr["max_protrusion_cm"], 12.0)
            self.assertGreaterEqual(fr["circularity"], 0.65)

    def test_delta_vs_base_and_game_view(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            _write_joints(root, "AS_pose", _pose(arm_z=50))
            _write_joints(root, "AS_tuck", _tuck())
            r = silhouette.anim_silhouette_check("AS_tuck", None, "az180,game", base_asset="AS_pose", root=root)
            v = r["views"]["az180"]
            self.assertGreater(v["delta_vs_base"]["flagged"], 0)
            self.assertLess(v["delta_vs_base"]["hull_area_mean_pct"], 0)
            self.assertIn("game", r["views"])

    def test_projection_conventions(self) -> None:
        # az90: the pawn's forward (+x) is screen right; az180 (behind): the pawn's right (+z) is screen right
        self.assertGreater(viewproj.project([10, 0, 0], 90)[0], 0)
        self.assertGreater(viewproj.project([0, 0, 10], 180)[0], 0)
        self.assertLess(viewproj.project([0, 0, 10], 0)[0], 0)
        self.assertEqual(viewproj.signed_dist((0, 0), viewproj.hull([(-1, -1), (1, -1), (1, 1), (-1, 1)])), -1.0)

    def test_missing_dump_is_error(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            self.assertFalse(silhouette.anim_silhouette_check("AS_none", None, "az90", root=t)["ok"])


class TestMotion(unittest.TestCase):
    def _clip(self, root: Path, name: str, fn) -> None:
        j = _pose(30)
        j["hand_MainPoint_r"] = [fn(f) for f in range(30)]
        _write_joints(root, name, j)

    def test_smooth_arc_no_flags_and_glitch_flagged(self) -> None:
        import math
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            self._clip(root, "AS_smooth", lambda f: [20 * math.cos(f / 10), 100 + 20 * math.sin(f / 10), 50])
            r = motion_overlay.anim_motion_overlay("AS_smooth", None, "az90", root=root)
            self.assertTrue(r["ok"], r)
            self.assertEqual(r["flags"]["wrist_r"]["arc_breaks"], [])
            self.assertEqual(r["flags"]["wrist_r"]["pops"], [])
            self.assertTrue(Path(r["images"]["az90"]).exists())
            self.assertLessEqual(len(r["summary"]), 5)
            # there-and-back glitch at frame 15: direction reverses (arc break)
            self._clip(root, "AS_glitch", lambda f: [3.0 * f + (25 if f == 15 else 0), 100, 50])
            g = motion_overlay.anim_motion_overlay("AS_glitch", None, "az90,az180", root=root)
            self.assertTrue(any(b["f"] in (15, 16) for b in g["flags"]["wrist_r"]["arc_breaks"]), g["flags"]["wrist_r"])

    def test_pop_detected(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            self._clip(root, "AS_pop", lambda f: [0.5 * f if f < 15 else 0.5 * f + 30, 100, 50])
            r = motion_overlay.anim_motion_overlay("AS_pop", None, "az315", base_asset="AS_pop", root=root)
            self.assertEqual(len(r["flags"]["wrist_r"]["pops"]), 1)

    def test_short_range_is_error(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            _write_joints(root, "AS_pose", _pose(10))
            self.assertFalse(motion_overlay.anim_motion_overlay("AS_pose", "2-3", "az90", root=root)["ok"])


if __name__ == "__main__":
    unittest.main()

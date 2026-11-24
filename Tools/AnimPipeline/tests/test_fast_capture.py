"""Fast capture (owner 2026-11-24, 1A + 2A): hitbox run only on the final round, fast env, content-based cache key with the capture
chain, hitstop pairs (hit video extended by the hitstop, side-by-side whiff vs hit). Offline (no editor, no pwsh)."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
for p in (str(HERE.parents[0] / "gstn_anim_mcp"), str(HERE)):
    if p not in sys.path:
        sys.path.insert(0, p)

import capture_move  # noqa: E402
from test_skill_gaps import KW, _card_repo, _card_runs  # noqa: E402
from test_t0_prep import FakeRunner  # noqa: E402


class HitRunner(FakeRunner):
    """FakeRunner whose captured rows have a 6-frame hitstop in the hit sequence (timelinePaused grows 6 times)."""

    def __call__(self, cmd, env, cwd):
        res = super().__call__(cmd, env, cwd)
        if cmd[0] == "pwsh":
            for r in json.loads(Path(env["PIE_CAPTURE_RUNS"]).read_text(encoding="utf-8")):
                for s in r["sequences"]:
                    rows = []
                    for f in range(95, 125):
                        paused = 0 if f < 105 else min(f - 104, 6) if s.endswith("hit") else 0
                        rows.append({"f": f, "combat": {"inTimeline": True, "timeline": 3, "timelinePaused": paused,
                                                        "historyFrame": f}, "montage": ["AM_X", f - 95.0]})
                    d = Path(env["PIE_CAPTURE_RUNS"]).parent / r["name"] / s
                    (d / "frames.jsonl").write_text("\n".join(json.dumps(x) for x in rows), encoding="utf-8")
        return res


class TestFastCapture(unittest.TestCase):
    def test_hitbox_run_only_on_final_round(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            runs = _card_runs(card) + [dict(_card_runs(card)[1], name="k7_hb", cvars="gstn.Combat.Debug.Hitboxes 1", hitbox_view=True)]
            r = capture_move.anim_capture_move(variant_runs=runs, root=root, dry_run=True, **KW)
            self.assertTrue(r["ok"], r)
            self.assertEqual(r["plan"]["capture_runs"], ["base", "k7"])
            self.assertEqual(r["plan"]["skipped_hitbox_runs"], ["k7_hb"])
            runs = _card_runs(card) + [dict(_card_runs(card)[1], name="k7_hb", cvars="gstn.Combat.Debug.Hitboxes 1", hitbox_view=True)]
            r = capture_move.anim_capture_move(variant_runs=runs, root=root, dry_run=True, final_round=True, **KW)
            self.assertIn("k7_hb", r["plan"]["capture_runs"])

    def test_fast_env_and_chain_in_key(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            r = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, dry_run=True, fast_window=[15, 70], **KW)
            self.assertEqual(r["plan"]["env"]["PIE_FASTCAP"], "1")
            self.assertEqual(r["plan"]["env"]["PIE_FASTCAP_WINDOW"], "15,70")
            slow = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, dry_run=True, fast=False, **KW)
            self.assertNotIn("PIE_FASTCAP", slow["plan"]["env"])
            self.assertNotEqual(r["plan"]["chain"], slow["plan"]["chain"])

    def test_base_reused_next_round_only_variant_recaptured(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            fr = FakeRunner(root)
            r1 = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, runner=fr, **KW)
            self.assertTrue(r1["ok"], r1)
            # next round: the variant's card changes (new intent hash), the base clip does not -> base from the cache
            data = json.loads(card.read_text(encoding="utf-8"))
            data["poses"] = [{"k": 1}]
            card.write_text(json.dumps(data), encoding="utf-8")
            chk = root / "Saved/AnimPipeline/animpipe" / f"{data['move']}_check.json"
            chk.write_text(chk.read_text(encoding="utf-8"), encoding="utf-8")   # fresh check, newer than the card
            r2 = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, dry_run=True, **dict(KW, objective="startup2"))
            self.assertTrue(r2["ok"], r2)
            self.assertEqual(sorted(r2["plan"]["reused"]), ["base"])
            self.assertEqual(r2["plan"]["capture_runs"], ["k7"])

    def test_hitstop_pairs_extend_and_side_by_side(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            fr = HitRunner(root)
            kw = dict(KW, sequences=["dropkick_sweet_whiff", "dropkick_sweet_hit"],
                      assert_spec={"dropkick_sweet_whiff": [{"clip": "AM_*", "window": [100, 110]}]})
            r = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, runner=fr,
                                               hitstop_pairs=[["dropkick_sweet_whiff", "dropkick_sweet_hit"]], **kw)
            self.assertTrue(r["ok"], r)
            self.assertEqual(r["hitstop_frames"]["k7"]["dropkick_sweet_hit"], 6)
            vids = [c for c, _ in fr.calls if c[0] == "python" and "dropkick_sweet_hit" in c[4]]
            whiff = [c for c, _ in fr.calls if c[0] == "python" and c[4] == "dropkick_sweet_whiff"]
            to = lambda c: int(next(x for x in c if x.startswith("--to="))[5:])
            self.assertEqual(to(vids[0]) - to(whiff[0]), 6)
            pair = [c for c, _ in fr.calls if c[0] == "ffmpeg"]
            self.assertEqual(len(pair), 4)   # 2 runs x real/slow4x
            self.assertTrue(pair[0][-1].endswith("_dropkick_sweet_whiff_vs_dropkick_sweet_hit_real.mp4"))

    def test_bad_pair_refused(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            r = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, dry_run=True, hitstop_pairs=[["a", "b"]], **KW)
            self.assertFalse(r["ok"])


class TestCameraJumps(unittest.TestCase):
    def test_smooth_follow_passes_jump_flagged(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            d = Path(t) / "run" / "seq"
            d.mkdir(parents=True)
            rows = []
            for f in range(100, 170):
                z = 500.0 - 0.12 * (f - 100) ** 2          # a fall, followed smoothly
                if f >= 157:
                    z -= 25.0                                # the v157 snap
                rows.append({"f": f, "cams": {"az90": [[0.0, 350.0, z], [0, 0, 0], 45.0, 480], "game": [[0.0, -300.0, 200.0], [0, 0, 0], 90.0, 480]}})
            (d / "frames.jsonl").write_text("\n".join(json.dumps(r) for r in rows), encoding="utf-8")
            j = capture_move.camera_jumps(Path(t) / "run", "seq")
            self.assertEqual([(x["view"], x["frame"]) for x in j], [("az90", 157), ("az90", 158)])
            self.assertEqual(capture_move.camera_jumps(Path(t) / "run", "seq", frames=(100, 150)), [])


if __name__ == "__main__":
    unittest.main()

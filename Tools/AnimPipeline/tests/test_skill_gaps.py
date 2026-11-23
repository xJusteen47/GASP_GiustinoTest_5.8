"""Skill gaps found by the dropkick K7 review (2026-11-23): card-authored poses through the capture gate, one kit per run,
hitboxes drawn into the captured pictures, montage-aligned A/B, silhouette JSON next to the review. Offline (no editor, no pwsh)."""
from __future__ import annotations

import json
import os
import sys
import tempfile
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
MCP_DIR = HERE.parents[0] / "gstn_anim_mcp"
GASNPP = HERE.parents[1] / "GASNPPTest"
for p in (str(MCP_DIR), str(GASNPP), str(HERE)):
    if p not in sys.path:
        sys.path.insert(0, p)

import capture_move  # noqa: E402
import capture_overlay  # noqa: E402
import manifest  # noqa: E402
import review_pack  # noqa: E402
from test_t0_prep import BASE, VAR, CLIP_VAR, FakeRunner, _make_repo  # noqa: E402

MOVE = "overlord_dropkick_startup_k7"


def _card_repo(root: Path, verdict: str = "PASS", anim: str = VAR, stale: bool = False) -> Path:
    """_make_repo without the run_intent report + a card and its animpipe check JSON."""
    _make_repo(root)
    m = manifest.load(root, VAR)
    m["last_report"], m["dry_runs"] = None, []
    manifest.save(root, VAR, m)
    card = root / "Tools/AnimPipeline/cards/overlord/dropkick" / f"{MOVE}.json"
    card.parent.mkdir(parents=True, exist_ok=True)
    out = root / "Saved/AnimPipeline/animpipe"
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{MOVE}_build.json").write_text("{}", encoding="utf-8")
    card.write_text(json.dumps({"move": MOVE, "anim": f"/Game/AnimPipeline/{anim}", "poses": []}), encoding="utf-8")
    chk = out / f"{MOVE}_check.json"
    chk.write_text(json.dumps({"verdict": verdict, "block": {"clearance": {"ok": verdict == "PASS", "line": "x"},
                                                            "co_motion": {"ok": True, "line": "y"}}}), encoding="utf-8")
    t = time.time()
    os.utime(card, (t - 20, t - 20))
    os.utime(out / f"{MOVE}_build.json", (t - 10, t - 10))
    os.utime(chk, (t - 15 if stale else t, t - 15 if stale else t))
    return card


def _card_runs(card: Path) -> list:
    return [{"name": "base", "asset": BASE, "clips": ["/Game/Anim/K6/AS_K6_Settle"], "kit": "KIT_K6", "cvars": "gstn.Combat.Debug.Hitboxes 0"},
            {"name": "k7", "asset": VAR, "clips": [CLIP_VAR], "card": str(card), "kit": "KIT_K7", "cvars": "gstn.Combat.Debug.Hitboxes 0"}]


KW = dict(kit="KIT_K6", sequences=["dropkick_sweet_whiff"], move="dk", round="k7", objective="startup", edit_window=[0, 19],
          anchor=110, faf=120, build="b1", assert_spec={"dropkick_sweet_whiff": [{"clip": "AM_*", "window": [100, 124]}]})


class TestCardGate(unittest.TestCase):
    def test_card_pass_opens_the_gate_with_a_report(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            r = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, dry_run=True, **KW)
            self.assertTrue(r["ok"], r)
            g = r["gates"]["k7"]
            self.assertTrue(g["card_report"]["ok"])
            self.assertEqual(g["intent_hash"], capture_move.card_hash(card))
            rep = json.loads((root / "docs/notes/reviews/dk/k7/gate_report.json").read_text(encoding="utf-8"))
            self.assertEqual(rep["verdict"], "PASS")
            self.assertEqual(manifest.load(root, VAR)["last_report"]["source"], "card")
            # one session, the kit of every run in runs.json
            self.assertEqual([x["kit"] for x in r["plan"]["runs_json"]], ["KIT_K6", "KIT_K7"])

    def test_card_check_fail_stale_or_other_clip_is_refused_and_reported(self) -> None:
        for kw, needle in ((dict(verdict="FAIL"), "animpipe check is FAIL (blocking: clearance)"),
                           (dict(stale=True), "check older than the build"),
                           (dict(anim="AS_Other"), "card anim AS_Other")):
            with self.subTest(**kw), tempfile.TemporaryDirectory() as t:
                root = Path(t)
                card = _card_repo(root, **kw)
                fr = FakeRunner(root)
                r = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, runner=fr, **KW)
                self.assertFalse(r["ok"])
                self.assertIn(needle, r["error"])
                self.assertEqual(fr.calls, [])
                rep = json.loads((root / r["gate_report"]).read_text(encoding="utf-8"))
                self.assertEqual(rep["verdict"], "FAIL")
                self.assertIsNone(manifest.load(root, VAR)["last_report"])

    def test_kit_defaults_to_the_session_kit(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            runs = _card_runs(card)
            del runs[0]["kit"]
            r = capture_move.anim_capture_move(variant_runs=runs, root=root, dry_run=True, **KW)
            self.assertEqual([x["kit"] for x in r["plan"]["runs_json"]], ["KIT_K6", "KIT_K7"])


def _rows(d: Path, seq: str, start: int, clip: str, cams: bool = False, hb: bool = False) -> None:
    (d / seq).mkdir(parents=True, exist_ok=True)
    with open(d / seq / "frames.jsonl", "w", encoding="utf-8") as f:
        for fr in range(95, 125):
            row = {"f": fr, "montage": [clip, float(fr - start)] if fr >= start else None}
            if cams:
                row["cams"] = {"az90": [[-350.0, 0.0, 100.0], [0.0, 0.0, 0.0], 90.0, 32]}
            if hb and fr >= start:
                row["hb"] = [{"i": 0, "s": 0, "t": 0, "c": [0.0, 0.0, 100.0], "q": [0, 0, 0, 1], "e": [50.0, 50.0, 50.0], "f": 0}]
            f.write(json.dumps(row) + "\n")


class TestAlignment(unittest.TestCase):
    def test_variant_starting_one_frame_later_is_shifted(self) -> None:
        # K7 whiff: T v138 vs v137 in the base
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            runs = {"base": root / "base", "k7": root / "k7"}
            _rows(runs["base"], "s", 107, "AM_K6")
            _rows(runs["k7"], "s", 108, "AM_K7")
            al = review_pack.run_offsets(runs, "s")
            self.assertEqual(al["offsets"], {"base": 0, "k7": 1})
            cmds = capture_move.video_commands(root, [{"name": "base"}, {"name": "k7"}], {k: str(v) for k, v in runs.items()}, ["s"],
                                               "out", (100, 120), {"s": al})
            self.assertIn("--from=101", cmds[2])
            self.assertIn("--to=121", cmds[2])
            self.assertIn("--from=100", cmds[0])

    def test_unknown_start_is_reported_not_guessed(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            runs = {"base": root / "base", "k7": root / "k7"}
            _rows(runs["base"], "s", 107, "AM_K6")
            al = review_pack.run_offsets(runs, "s")
            self.assertEqual(al["offsets"]["k7"], 0)
            self.assertEqual(al["unknown"], ["k7"])

    def test_capture_pack_aligns_and_writes_alignment(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)

            class Runner(FakeRunner):
                def __call__(self, cmd, env, cwd):
                    res = super().__call__(cmd, env, cwd)
                    if cmd[0] == "pwsh":
                        d = Path(env["PIE_CAPTURE_RUNS"]).parent
                        _rows(d / "base", "dropkick_sweet_whiff", 107, "AM_K6")
                        _rows(d / "k7", "dropkick_sweet_whiff", 108, "AM_K7")
                    return res
            r = capture_move.anim_capture_move(variant_runs=_card_runs(card), root=root, runner=Runner(root), **KW)
            self.assertTrue(r["ok"], r)
            self.assertEqual(r["pack"]["offsets"]["dropkick_sweet_whiff"], {"base": 0, "k7": 1})
            self.assertTrue((root / "docs/notes/reviews/dk/k7/alignment.json").exists())


class TestHitboxRunSeparate(unittest.TestCase):
    def test_hitbox_run_out_of_review_with_own_videos(self) -> None:
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            card = _card_repo(root)
            runs = _card_runs(card) + [dict(_card_runs(card)[1], name="k7_hb", cvars="gstn.Combat.Debug.Hitboxes 1", hitbox_view=True)]
            fr = FakeRunner(root)
            r = capture_move.anim_capture_move(variant_runs=runs, root=root, runner=fr, **KW)
            self.assertTrue(r["ok"], r)
            vids = [c for c, _ in fr.calls if c[0] == "python"]
            review = [c for c in vids if "--hitboxes=1" not in c]
            hb = [c for c in vids if "--hitboxes=1" in c]
            self.assertEqual(len(review), 4)
            self.assertFalse(any("k7_hb" in x for c in review for x in c))
            self.assertEqual(len(hb), 2)
            self.assertTrue(all("/hitbox/k7_hb_" in c[5].replace("\\", "/") for c in hb))
            from PIL import Image
            g = Image.open(root / "docs/notes/reviews/dk/k7/grid_dropkick_sweet_whiff_az90.png")
            self.assertEqual(g.size[1] // 256, 2, "grid rows = base + k7 only")


class TestHitboxOverlay(unittest.TestCase):
    CAM = [[-350.0, 0.0, 100.0], [0.0, 0.0, 0.0], 90.0, 480]

    def test_projection(self) -> None:
        x, y, d = capture_overlay.project([0.0, 0.0, 100.0], self.CAM)
        self.assertAlmostEqual(x, 240.0)
        self.assertAlmostEqual(y, 240.0)
        self.assertAlmostEqual(d, 350.0)
        # UE +Y (right) is screen right, +Z up is screen up
        self.assertGreater(capture_overlay.project([0.0, 50.0, 100.0], self.CAM)[0], 240)
        self.assertLess(capture_overlay.project([0.0, 0.0, 150.0], self.CAM)[1], 240)
        self.assertIsNone(capture_overlay.project([-400.0, 0.0, 100.0], self.CAM))
        # camera looking down 30 deg from above: a point under it projects below the centre
        cam = [[-300.0, 0.0, 273.2], [-30.0, 0.0, 0.0], 90.0, 480]
        self.assertAlmostEqual(capture_overlay.project([0.0, 0.0, 100.0], cam)[1], 240.0, places=0)

    def test_shapes(self) -> None:
        sph = capture_overlay.shape_2d({"t": 0, "c": [0, 0, 100], "e": [24, 24, 24]}, self.CAM)
        self.assertEqual(sph[0][0], "circle")
        self.assertAlmostEqual(sph[0][3], 240.0 * 24 / 350, places=3)
        cap = capture_overlay.shape_2d({"t": 1, "c": [0, 0, 100], "q": [0, 0, 0, 1], "e": [17, 17, 40]}, self.CAM)
        self.assertEqual([p[0] for p in cap], ["circle", "circle", "line", "line"])
        box = capture_overlay.shape_2d({"t": 2, "c": [0, 0, 100], "q": [0, 0, 0, 1], "e": [10, 10, 10]}, self.CAM)
        self.assertEqual(box[0][0], "poly")

    def test_grid_draws_logged_hitboxes(self) -> None:
        from PIL import Image
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            runs = {"base": root / "base", "k7": root / "k7"}
            for n, d in runs.items():
                _rows(d, "s", 100, "AM", cams=True, hb=(n == "k7"))
                (d / "s" / "az90").mkdir(parents=True)
                for f in range(95, 125):
                    Image.new("RGB", (32, 32), (0, 0, 0)).save(d / "s" / "az90" / f"f{f:03d}.png")
            clean = review_pack.build_grid(runs, "s", "az90", [110, 111], root / "c.png", cell=64)
            self.assertEqual(clean["hitbox_cells"], 0, "review grids stay clean by default")
            g = review_pack.build_grid(runs, "s", "az90", [110, 111], root / "g.png", cell=64, hitboxes=True)
            self.assertEqual(g["hitbox_cells"], 2)
            im = Image.open(root / "g.png").convert("RGB")
            red_k7 = sum(1 for x in range(128) for y in range(80, 128) if im.getpixel((x, y))[0] > 120 and im.getpixel((x, y))[1] < 60)
            red_base = sum(1 for x in range(128) for y in range(16, 64) if im.getpixel((x, y))[0] > 120 and im.getpixel((x, y))[1] < 60)
            self.assertGreater(red_k7, 10)
            self.assertEqual(red_base, 0)

    def test_pie_capture_grid_cell_draws_and_labels(self) -> None:
        from PIL import Image
        import pie_capture_grid as pcg
        with tempfile.TemporaryDirectory() as t:
            d = Path(t)
            (d / "s" / "az90").mkdir(parents=True)
            Image.new("RGB", (32, 32), (0, 0, 0)).save(d / "s" / "az90" / "f110.png")
            row = {"f": 110, "tl": None, "hitstop": False, "cams": {"az90": [[-350.0, 0.0, 100.0], [0, 0, 0], 90.0, 32]},
                   "hb": [{"t": 0, "s": 0, "c": [0, 0, 100], "e": [50, 50, 50]}]}
            self.assertFalse(any(pcg.cell(str(d), "s", "az90", row, 128).getpixel((x, 64))[0] > 120 for x in range(128)))
            pcg.HITBOXES = True
            try:
                im = pcg.cell(str(d), "s", "az90", row, 128)
            finally:
                pcg.HITBOXES = False
            self.assertTrue(any(im.getpixel((x, 64))[0] > 120 for x in range(128)))
            self.assertEqual(capture_overlay.has_overlay({"hb_err": "x"}, "az90")[0], False)


if __name__ == "__main__":
    unittest.main()

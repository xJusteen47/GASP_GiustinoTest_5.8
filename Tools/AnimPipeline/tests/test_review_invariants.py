"""Review invariants (owner 2026-11-24: quality equal to the OG skill, the final review is never cut for cost, a false alarm is fixed and
never softened). Fails when: a skill or the contract loses its INVARIANTI section or one invariant id; a skill caps the final review to
a number of images; the level-2 pack (final_review_pack) can be "complete" without one of its steps; an inherited block stops blocking;
a declared hold has no written reason. Source: Tools/AnimPipeline/contract/invarianti-review.md. Offline."""
import json
import re
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
AP = HERE.parent
REPO = AP.parents[1]
sys.path.insert(0, str(AP / "gstn_anim_mcp"))
sys.path.insert(0, str(AP))
sys.path.insert(0, str(HERE))

import capture_move  # noqa: E402
import final_review_pack as frp  # noqa: E402
from animpipe.card import holds_without_reason  # noqa: E402
from test_t0_prep import BASE, VAR, FakeRunner, TestCaptureMove, _make_repo, _runs, _shifted, _write_joints  # noqa: E402

SKILLS = {
    "reviewer": REPO / ".claude/skills/gstn-anim-reviewer/project.md",
    "executor": REPO / ".claude/skills/gstn-anim-executor/project.md",
    "cheap": REPO / ".claude/skills/gstn-anim-cycle-cheap/SKILL.md",
    "contract": AP / "contract/invarianti-review.md",
}
# every text a thread reads to run a review: none may cap the final review to a number of images
CAP_SCAN = list(SKILLS.values()) + [REPO / ".claude/skills/gstn-anim-reviewer/SKILL.md",
                                    REPO / ".claude/skills/gstn-anim-executor/SKILL.md",
                                    REPO / "skills-public/anim-reviewer/SKILL.md", REPO / "skills-public/anim-executor/SKILL.md",
                                    AP / "contract/rubric.md", AP / "contract/README.md"]
CAP = re.compile(r"(\bmax(imum)?\b|\bmassimo\b|\bat most\b|\bfino a\b|<=)\s*~?\d+\s*(immagini|images|grids|griglie)"
                 r"|solo (le )?griglie|only (the )?grids|mai gli mp4|never the mp4|solo inizio e fine|start and end only", re.I)
LEVEL1 = re.compile(r"livello 1|level 1|level-1", re.I)


class TestSkillText(unittest.TestCase):
    def test_every_skill_has_the_invariants_section_and_all_ids(self):
        for name, p in SKILLS.items():
            txt = p.read_text(encoding="utf-8")
            self.assertRegex(txt, r"(?m)^#{1,2} INVARIANTI", f"{name}: '## INVARIANTI' section missing ({p})")
            sec = txt[txt.index("INVARIANTI"):]
            for k in frp.INVARIANTS:
                self.assertIn(k, sec, f"{name}: invariant {k} ({frp.INVARIANTS[k]}) missing from the INVARIANTI section")

    def test_invariants_section_comes_before_the_workflow(self):
        for name in ("reviewer", "executor", "cheap"):
            txt = SKILLS[name].read_text(encoding="utf-8")
            first_h2 = re.search(r"(?m)^## .*", txt).group(0)
            self.assertTrue(first_h2.startswith("## INVARIANTI"), f"{name}: INVARIANTI must be the first section, is {first_h2!r}")

    def test_the_key_words_of_each_step_are_there(self):
        must = {"slow4x": "INV-02 video", "az180": "INV-03 views", "game": "INV-03 views", "settle": "INV-03/04 whole move",
                "critic": "INV-07", "reason": "INV-12 hold reason"}
        txt = SKILLS["reviewer"].read_text(encoding="utf-8") + SKILLS["executor"].read_text(encoding="utf-8")
        for w, why in must.items():
            self.assertIn(w, txt, f"skill text lost '{w}' ({why})")

    def test_no_image_cap_on_the_final_review(self):
        bad = []
        for p in CAP_SCAN:
            if not p.exists():
                continue
            for i, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
                if CAP.search(line) and not LEVEL1.search(line):
                    bad.append(f"{p.relative_to(REPO)}:{i}: {line.strip()[:120]}")
        self.assertEqual(bad, [], "a skill caps the final review (INV-09); caps may only name level 1:\n" + "\n".join(bad))

    def test_inherited_blocks_are_not_described_as_non_blocking(self):
        for name in ("reviewer", "executor", "cheap"):
            txt = SKILLS[name].read_text(encoding="utf-8").lower()
            for bad in ("inherited, not blocking", "do not fail the variant for", "ereditati, non bloccanti", "non bloccano"):
                self.assertFalse(bad in txt, f"{name}: '{bad}' contradicts INV-10")


def _frames(cap: Path, seq: str, views, frames, skip=()):
    from PIL import Image
    for v in views:
        d = cap / seq / v
        d.mkdir(parents=True, exist_ok=True)
        for f in frames:
            if (v, f) not in skip:
                Image.new("RGB", (32, 32), (90, 90, 90)).save(d / f"f{f:03d}.png")
    rows = [json.dumps({"f": f, "montage": ["AM_X", 0.0]}) for f in frames]
    (cap / seq / "frames.jsonl").write_text("\n".join(rows), encoding="utf-8")


class TestFinalPack(unittest.TestCase):
    OUT = "docs/notes/reviews/dk/r9"

    def _setup(self, root: Path, skip=(), views=frp.REQUIRED_VIEWS):
        for n, d in (("base", 0.0), ("k9", 5.0)):
            _frames(root / "cap" / n, "dk_hit", views, range(100, 140), skip if n == "k9" else ())
        for c, d in (("AS_B_Startup", 0.0), ("AS_B_Settle", 0.0), ("AS_K9_Startup", 5.0), ("AS_K9_Settle", 5.0)):
            _write_joints(root, c, _shifted(d))
        (root / self.OUT).mkdir(parents=True, exist_ok=True)
        for n in ("base", "k9"):
            for t in ("real", "slow4x"):
                (root / self.OUT / f"{n}_dk_hit_{t}.mp4").write_bytes(b"mp4")

    def _kw(self, **over):
        kw = dict(out=self.OUT, runs={"base": "cap/base", "k9": "cap/k9"}, sequences=["dk_hit"], window=[100, 139],
                  identity=["gambe unite e dritte", "braccia conserte"], anchor=115, faf=130, key_frames=[108],
                  variant_clips=["AS_K9_Startup", "AS_K9_Settle"], base_clips=["AS_B_Startup", "AS_B_Settle"])
        kw.update(over)
        return kw

    def test_complete_pack_has_every_step(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            self._setup(root)
            r = frp.build_final_pack(root=root, **self._kw())
            self.assertTrue(r["complete"], r["missing"])
            pj = root / self.OUT / "final" / "review_pack.json"
            self.assertEqual(frp.check_pack(pj, root), [])
            inv = r["invariants"]
            self.assertEqual(len(inv["INV-03"]["files"]), 3 * 2)          # 3 views x 2 tiles of 32 cells over 40 frames: whole move
            self.assertEqual(len(inv["INV-04"]["files"]), 2 * 2)          # every clip, az90 + az180
            self.assertEqual(inv["INV-05"]["frames"], [108, 115, 130])
            self.assertTrue(r["order"][0].endswith(".mp4"))              # videos first: life before numbers
            brief = (root / self.OUT / "final" / "critico_brief.md").read_text(encoding="utf-8")
            self.assertIn("braccia conserte", brief)
            self.assertNotIn("verdetto: PASS", brief)

    def test_each_missing_step_makes_the_pack_incomplete(self):
        cases = {
            "INV-01": dict(identity=["solo uno"]),
            "INV-04": dict(variant_clips=[]),
            "INV-05": dict(key_frames=[], anchor=None, faf=None),
            "INV-06": dict(runs={"k9": "cap/k9"}),
            "INV-12": dict(holds={"AS_K9_Settle": "6-12"}),
        }
        for k, over in cases.items():
            with tempfile.TemporaryDirectory() as t:
                root = Path(t)
                self._setup(root)
                r = frp.build_final_pack(root=root, **self._kw(**over))
                self.assertFalse(r["complete"], k)
                self.assertIn(k, r["missing"], (k, r["missing"]))

    def test_videos_views_and_frames_are_all_required(self):
        with tempfile.TemporaryDirectory() as t:              # a missing slow4x video
            root = Path(t)
            self._setup(root)
            (root / self.OUT / "k9_dk_hit_slow4x.mp4").unlink()
            self.assertIn("INV-02", frp.build_final_pack(root=root, **self._kw())["missing"])
        with tempfile.TemporaryDirectory() as t:              # the game view not captured
            root = Path(t)
            self._setup(root, views=("az90", "az180"))
            self.assertIn("INV-03", frp.build_final_pack(root=root, **self._kw(views="az90,az180"))["missing"])
        with tempfile.TemporaryDirectory() as t:              # one settle frame missing from behind: a frame uncovered
            root = Path(t)
            self._setup(root, skip={("az180", 136)})
            r = frp.build_final_pack(root=root, **self._kw())
            self.assertIn("INV-03", r["missing"])
            self.assertIn("k9 f136", r["uncovered"]["dk_hit"]["az180"])

    def test_declared_hold_with_reason_goes_to_the_video_judgement(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            self._setup(root)
            r = frp.build_final_pack(root=root, **self._kw(holds={"AS_K9_Settle": "6-12"},
                                                          hold_reasons={"AS_K9_Settle": "owner: landing hold"}))
            self.assertTrue(r["complete"], r["missing"])
            self.assertEqual(r["invariants"]["INV-12"]["judge_on_video"][0]["frames"], "6-12")
            self.assertIn("6-12", (root / self.OUT / "final" / "critico_brief.md").read_text(encoding="utf-8"))

    def test_check_pack_catches_a_deleted_file_and_a_missing_section(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            self._setup(root)
            frp.build_final_pack(root=root, **self._kw())
            pj = root / self.OUT / "final" / "review_pack.json"
            first = json.loads(pj.read_text())["invariants"]["INV-03"]["files"][0]
            (root / first).unlink()
            self.assertIn("INV-03", frp.check_pack(pj, root))
            d = json.loads(pj.read_text())
            del d["invariants"]["INV-07"]
            pj.write_text(json.dumps(d))
            self.assertIn("INV-07", frp.check_pack(pj))

    def test_capture_move_builds_the_final_pack_and_refuses_review_without_identity(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            r = capture_move.anim_capture_move(variant_runs=_runs(h), root=root, runner=FakeRunner(root), **TestCaptureMove.KW)
            self.assertTrue(r["ok"], r)
            self.assertFalse(r["review_ready"])
            self.assertIn("INV-01", r["final_pack"]["missing"])
            self.assertIn("NO level-2 review", r["next"])
            self.assertTrue((root / r["final_pack"]["json"]).exists())


class TestInheritedBlocksBlock(unittest.TestCase):
    LINE = "FAIL contact_faf: hand 14 cm short at f15"

    def _repo(self, root: Path, exemption: str = ""):
        (root / "docs/notes").mkdir(parents=True)
        (root / "docs/notes/anim-approvate.md").write_text(
            "| dropkick | K7 | AS_Base |\n\n## Esenzioni dei controlli\n| base | blocco | riga | owner |\n|---|---|---|---|\n"
            + exemption, encoding="utf-8")
        card = root / "Tools/AnimPipeline/cards/x.json"
        card.parent.mkdir(parents=True)
        card.write_text(json.dumps({"move": "dk_k8", "anim": "/Game/A/AS_Var", "poses": []}))
        out = root / "Saved/AnimPipeline/animpipe"
        out.mkdir(parents=True)
        (out / "dk_k8_check.json").write_text(json.dumps({"verdict": "FAIL", "block": {"contact_faf": {"ok": False, "line": self.LINE},
                                                                                       "fists": {"ok": True, "line": "ok"}}}))
        (root / "base_check.json").write_text(json.dumps({"verdict": "FAIL", "block": {"contact_faf": {"ok": False, "line": self.LINE}}}))
        return card

    def _report(self, root, card):
        return capture_move.card_report(root, "AS_Var", card, base_asset="AS_Base", base_card="b.json",
                                        base_check_json=root / "base_check.json")

    def test_identical_failure_on_the_approved_base_still_blocks(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            r = self._report(root, self._repo(root))
            self.assertFalse(r["ok"])
            self.assertIn("contact_faf", r["inherited"])
            self.assertIn("still blocking", r["error"])

    def test_only_an_owner_row_for_the_exact_line_unblocks(self):
        row = f'| AS_Base | contact_faf | {self.LINE} | owner 2026-11-24: "va bene cosi\'" |\n'
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            r = self._report(root, self._repo(root, row))
            self.assertTrue(r["checks"][1]["ok"], r)
            self.assertEqual(list(r["exempted"]), ["contact_faf"])
        for bad in (row.replace('"va bene cosi\'"', "thread decided"),            # no owner words
                    row.replace("14 cm", "12 cm"),                                 # another line (numbers changed)
                    row.replace("contact_faf", "stop_and_go")):                    # never exemptable / other block
            with tempfile.TemporaryDirectory() as t:
                root = Path(t)
                r = self._report(root, self._repo(root, bad))
                self.assertFalse(r["checks"][1]["ok"], bad)

    def test_stop_and_go_and_hold_reason_are_never_inherited(self):
        self.assertIn("stop_and_go", capture_move.NON_INHERITABLE)
        self.assertIn("hold_reason", capture_move.NON_INHERITABLE)


class TestHoldReasons(unittest.TestCase):
    def test_holds_need_a_written_reason(self):
        c = {"poses": [{"frame": 4, "hold": True}, {"frame": 9, "hold": True, "hold_reason": "owner: pose for the camera"},
                       {"frame": 12, "impact": True}], "holds": [20, 22], "hold_reasons": {"22": "owner: landing"}}
        self.assertEqual(holds_without_reason(c), [4, 20])

    def test_capture_gate_refuses_a_hold_without_reason(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            h = _make_repo(root)
            g = capture_move.precapture_gate(root, VAR, BASE, h, ["/Game/Anim/K6/AS_K6_Settle_ft"], (0, 19), "settle",
                                             hitbox_required=False, no_stop={"holds": {"AS_K6_Settle_ft": "2-4"}})
            ns = [c for c in g["checks"] if c["check"] == "no_stop"][0]
            self.assertFalse(ns["ok"])
            self.assertIn("without a reason", ns["msg"])


if __name__ == "__main__":
    unittest.main()

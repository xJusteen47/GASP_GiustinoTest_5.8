"""INV-13: the animation principles, the fighting-game notes and Sakurai's notes of the OG skill are CONSIDERED in every final review
(owner 2026-11-24), as principles and not as automatic rules. Fails when: a mandatory principle disappears from
contract/principi-review.md; an entry loses its documents or cites a document that does not exist; the skills or the level-2 pack stop
carrying INV-13; the check accepts a review where a principle was not considered, or rejects a reasoned, intended deviation (the check
must test that the reasoning is written, never that a number is in range). Offline."""
import re
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
AP = HERE.parent
REPO = AP.parents[1]
PF = Path("/mnt/project-files")
sys.path.insert(0, str(AP / "gstn_anim_mcp"))
sys.path.insert(0, str(HERE))

import final_review_pack as frp  # noqa: E402
import principi_review as pr  # noqa: E402

CONTRACT = AP / "contract" / "principi-review.md"
INDEX = AP / "contract" / "indice-ricerca.md"
# what the owner asked to be checked for real (coordinator note 2026-11-24): topic -> words that must appear in a principle title
MANDATORY = {"anticipazione": "anticipazione", "tenuta viva": "tenuta con vita", "hitstop": "hitstop", "posa d'impatto": "impatto",
             "follow-through": "follow-through", "silhouette": "silhouette", "ritorno all'idle": "idle", "reazioni": "reazione",
             "timing/spacing": "spacing", "peso": "peso", "archi": "archi", "recovery": "recovery", "esagerazione": "esagerazione"}
# the documents the principles come from: every one must be cited by at least one entry
SOURCES = ("docs/notes/anim-theory/principi-animazione-corporea.md", "docs/notes/anim-theory/animazione-picchiaduro-action.md",
           'OG "Style principles"', 'OG "How the owner judges"', 'OG "Posing fundamentals"', "hitstop-sakurai-1.md",
           "moveset-template/ricerca.md", "silhouette-multidirezionali.md", "reazioni-ricerca.md", "ritorno-idle-ricerca.md")


def _filled(skip=(), override=None):
    """A review that considered every principle: varied, mostly reasoned text, one intended deviation, one not pertinent."""
    out = ["# Principi considerati: dk/r9", ""]
    for k, p in pr.principles().items():
        if k in skip:
            continue
        e = {"intento": f"dropkick pesante: {p['title'].lower()} deve dire forza e impegno",
             "previsione": "il principio predice carica lunga e contatto secco per una mossa pesante",
             "osservato": "f108-115 az180 e az90: la carica scende e il contatto arriva in un frame",
             "scostamento": "nessuno", "voluto": "-", "giudizio": "coerente"}
        if k == "PR-08":
            e.update(scostamento="tenuta di 6 frame senza rinculo a f116-121", voluto="si, tratto d'identita' gambe unite tenute (brief)")
        if k == "PR-18":
            e = {"giudizio": "non pertinente: mossa a terra, nessun atterraggio"}
        e.update((override or {}).get(k, {}))
        out += [f"### {k} {p['title']}"] + [f"- {f}: {v}" for f, v in e.items()] + [""]
    return "\n".join(out)


class TestContract(unittest.TestCase):
    def test_ids_are_consecutive_and_enough(self):
        ids = list(pr.principles())
        self.assertGreaterEqual(len(ids), 20, "principles lost from principi-review.md")
        self.assertEqual(ids, [f"PR-{i:02d}" for i in range(1, len(ids) + 1)], "PR ids must be consecutive (only the owner removes one)")

    def test_mandatory_topics_are_principles(self):
        titles = " | ".join(p["title"].lower() for p in pr.principles().values())
        for topic, word in MANDATORY.items():
            self.assertIn(word, titles, f"mandatory principle '{topic}' missing from principi-review.md")

    def test_every_entry_cites_documents_and_every_source_is_cited(self):
        allrefs = ""
        for k, p in pr.principles().items():
            self.assertRegex(p["refs"], r"`[^`]+\.md`|OG \"|KB P-", f"{k} cites no document")
            self.assertGreater(len(p["questions"]), 40, f"{k} has no questions to consider")
            allrefs += p["refs"] + "\n"
        for s in SOURCES:
            self.assertIn(s, allrefs, f"source {s} is no longer cited by any principle")

    def test_cited_documents_and_sections_exist(self):
        og = (AP / "contract/archive/gstn-move-animation_2026-11-15.md").read_text(encoding="utf-8")
        bad = []
        for k, p in pr.principles().items():
            for ref in re.findall(r"`([^`]+\.md)`", p["refs"]):
                if ref.startswith("m6s9y2:"):
                    continue                       # knowledge base branch, not in main (named in the index)
                path = PF / ref[3:] if ref.startswith("pf:") else REPO / ref
                if ref.startswith("pf:") and not PF.exists():
                    continue                       # project files not mounted (desktop): checked in cloud
                if not path.exists():
                    bad.append(f"{k}: {ref}")
            for sec in re.findall(r'OG "([^"]+)"', p["refs"]):
                if f"## {sec}" not in og:
                    bad.append(f"{k}: OG section {sec}")
        self.assertEqual(bad, [], "principi-review.md cites missing documents:\n" + "\n".join(bad))

    def test_principles_not_rules_and_the_four_steps(self):
        txt = CONTRACT.read_text(encoding="utf-8")
        self.assertIn("non applicarli come un automa", txt)
        self.assertIn("§13", txt)
        for w in ("intento", "previsione", "osservato", "scostamento", "voluto", "non pertinente"):
            self.assertIn(w, txt)

    def test_index_lists_the_research_and_the_owner_checks(self):
        txt = INDEX.read_text(encoding="utf-8")
        for name in ("hitstop-sakurai-1", "reazioni-ricerca", "attack-aiming-research", "ritorno-idle-ricerca",
                     "anim-physics-pass-ricerca", "review-feel-defaults", "feel-defaults-piano", "silhouette-vfx-ricerca",
                     "vfx-clarity-ricerca", "moveset-template/ricerca.md", "camera-mario-research", "ricerca-mario-astro",
                     "principi-animazione-corporea", "animazione-picchiaduro-action", "gstn-move-animation_2026-11-15"):
            self.assertIn(name, txt, f"research index lost {name}")
        self.assertIn("## Da far controllare all'owner", txt)


class TestWiring(unittest.TestCase):
    def test_inv13_in_skills_contract_and_pack(self):
        self.assertIn("INV-13", frp.INVARIANTS)
        self.assertIn("INV-13", frp.PACK_INVARIANTS)
        for p in (".claude/skills/gstn-anim-reviewer/project.md", ".claude/skills/gstn-anim-executor/project.md",
                  ".claude/skills/gstn-anim-cycle/SKILL.md", "Tools/AnimPipeline/contract/invarianti-review.md"):
            txt = (REPO / p).read_text(encoding="utf-8")
            self.assertIn("INV-13", txt, p)
            self.assertIn("principi", txt, p)
        for c in ("principi-review.md", "animazione-picchiaduro-action.md", "principi-animazione-corporea.md"):
            self.assertTrue(any(c in x for x in frp.CRITERIA), f"critic criteria lost {c}")

    def test_pack_writes_the_file_to_fill_and_the_critic_fills_it(self):
        from test_review_invariants import TestFinalPack
        tp = TestFinalPack()
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            tp._setup(root)
            r = frp.build_final_pack(root=root, **tp._kw())
            self.assertTrue(r["complete"], r["missing"])
            f = root / tp.OUT / "final" / "principi.md"
            self.assertTrue(f.exists())
            self.assertEqual(r["invariants"]["INV-13"]["files"], [f"{tp.OUT}/final/principi.md"])
            self.assertEqual(len(pr.check(f.read_text(encoding="utf-8"))), len(pr.principles()))   # blank = nothing considered
            self.assertIn("principi_review.py check", (root / tp.OUT / "final" / "critico_brief.md").read_text(encoding="utf-8"))


class TestCheck(unittest.TestCase):
    def test_every_principle_considered_passes(self):
        self.assertEqual(pr.check(_filled()), [])

    def test_a_missing_principle_fails(self):
        self.assertEqual(pr.check(_filled(skip=("PR-06",))), ["PR-06: missing"])

    def test_placeholders_and_empty_steps_fail(self):
        bad = pr.check(_filled(override={"PR-02": {"previsione": "<...>"}, "PR-03": {"giudizio": "ok"}}))
        self.assertTrue(any(b.startswith("PR-02: previsione") for b in bad), bad)
        self.assertTrue(any(b.startswith("PR-03: giudizio") for b in bad), bad)

    def test_observed_must_cite_frame_and_view(self):
        bad = pr.check(_filled(override={"PR-04": {"osservato": "il peso sembra a posto"}}))
        self.assertEqual(bad, ["PR-04: osservato must cite a frame (f/v/a<n>) and a view"])

    def test_not_pertinent_needs_a_reason(self):
        self.assertTrue(pr.check(_filled(override={"PR-18": {"giudizio": "non pertinente:"}})))

    def test_deviation_needs_intended_or_not_and_unintended_is_a_defect(self):
        bad = pr.check(_filled(override={"PR-09": {"scostamento": "polso e mano si fermano insieme a f120", "voluto": "-"}}))
        self.assertEqual(bad, ["PR-09: a deviation needs 'voluto: si, <motivo>' or 'voluto: no'"])
        bad = pr.check(_filled(override={"PR-09": {"scostamento": "polso e mano si fermano insieme a f120", "voluto": "no"}}))
        self.assertEqual(bad, ["PR-09: an unintended deviation is a 'difetto', not 'coerente'"])
        ok = pr.check(_filled(override={"PR-09": {"scostamento": "polso e mano si fermano insieme a f120", "voluto": "no",
                                                  "giudizio": "difetto medio posa"}}))
        self.assertEqual(ok, [])

    def test_principles_not_rules_an_intended_reasoned_deviation_stays_accepted(self):
        # PR-08 in _filled deviates from the principle on purpose, with the reason: the check must accept it
        self.assertEqual(pr.check(_filled()), [])
        self.assertNotIn("PR-08", " ".join(pr.check(_filled())))

    def test_cli_prints_at_most_five_lines(self):
        import subprocess
        with tempfile.TemporaryDirectory() as t:
            f = Path(t) / "p.md"
            f.write_text(pr.template("dk/r9"), encoding="utf-8")
            r = subprocess.run([sys.executable, str(AP / "gstn_anim_mcp/principi_review.py"), "check", str(f)],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertLessEqual(len(r.stdout.strip().splitlines()), 5)
            self.assertIn("PRINCIPI: INCOMPLETE", r.stdout)


if __name__ == "__main__":
    unittest.main()

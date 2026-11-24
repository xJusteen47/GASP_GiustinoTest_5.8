"""Offline tests of Tools/CI/Build-Indice.py."""
import importlib.util
import os
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("bi", os.path.join(os.path.dirname(__file__), "..", "Build-Indice.py"))
bi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bi)

GOOD = """# Thread uno
BRANCH: claude/x abc1234
ESITO: OK | Fatto il giro uno
SUBAGENTI: 1 (cattura, haiku)
COSTO: ~1,5 $ | 30 minuti desktop | 0 token review
"""


class BuildIndice(unittest.TestCase):
    def run_dir(self, files, since="2026-11-25"):
        d = tempfile.mkdtemp()
        h = os.path.join(d, "h")
        os.makedirs(h)
        for name, text in files.items():
            open(os.path.join(h, name), "w", encoding="utf-8").write(text)
        out = os.path.join(d, "out")
        rc = bi.main(["--handovers", h, "--out-dir", out, "--since", since])
        return rc, out

    def test_good_handover_is_indexed_with_cost(self):
        rc, out = self.run_dir({"2026-11-26-uno.md": GOOD})
        self.assertEqual(rc, 0)
        csv_text = open(os.path.join(out, "costi.csv"), encoding="utf-8").read()
        self.assertIn('claude/x,abc1234,OK,Fatto il giro uno,"1 (cattura, haiku)",1.5,30,0,ok', csv_text)
        self.assertIn("| 2026-W48 | 1 | 1.5 |", open(os.path.join(out, "INDICE.md"), encoding="utf-8").read())

    def test_pass_with_space_separator(self):
        rc, out = self.run_dir({"2026-11-26-tre.md": GOOD.replace("ESITO: OK | Fatto", "ESITO: PASS Fatto")})
        self.assertEqual(rc, 0)
        self.assertIn(",PASS,Fatto il giro uno,", open(os.path.join(out, "costi.csv"), encoding="utf-8").read())

    def test_new_handover_without_schema_fails(self):
        rc, _ = self.run_dir({"2026-11-26-due.md": "# Due\nCOSTO: circa tre dollari\n"})
        self.assertEqual(rc, 1)

    def test_old_handover_without_schema_is_legacy_not_error(self):
        rc, out = self.run_dir({"2026-11-10-vecchio.md": "# Vecchio\nCOSTO: ~6 $ (stima)\n"})
        self.assertEqual(rc, 0)
        row = open(os.path.join(out, "costi.csv"), encoding="utf-8").read().splitlines()[1]
        self.assertTrue(row.endswith(",6,,,legacy"))

    def test_check_writes_nothing(self):
        d = tempfile.mkdtemp()
        h = os.path.join(d, "h")
        os.makedirs(h)
        open(os.path.join(h, "2026-11-26-uno.md"), "w").write(GOOD)
        out = os.path.join(d, "out")
        self.assertEqual(bi.main(["--handovers", h, "--out-dir", out, "--check"]), 0)
        self.assertFalse(os.path.exists(out))


if __name__ == "__main__":
    unittest.main()

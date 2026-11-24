"""Offline tests of Tools/CI/precheck.py (run: python -I -m unittest discover Tools/CI/tests)."""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import precheck  # noqa: E402


class PreCheck(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.d, "Content", "Anim"))
        open(os.path.join(self.d, "Content", "Anim", "AS_Skel.uasset"), "w").close()

    def write(self, name, text):
        p = os.path.join(self.d, name)
        with open(p, "w") as f:
            f.write(text)
        return p

    def test_syntax_error_is_found_before_the_editor(self):
        p = self.write("bad.py", "def f(:\n    pass\n")
        out, bad = precheck.check_syntax([p])
        self.assertEqual(bad, 1)
        self.assertIn("line 1", out[0])

    def test_good_script_compiles_and_leaves_no_pycache(self):
        p = self.write("ok.py", "x = 1\n")
        out, bad = precheck.check_syntax([p])
        self.assertEqual(bad, 0)
        self.assertFalse(os.path.exists(os.path.join(self.d, "__pycache__")))

    def test_missing_asset_in_this_project(self):
        out, bad = precheck.check_assets(self.d, ["/Game/Anim/AS_Skel.AS_Skel", "/Game/Anim/Missing"])
        self.assertEqual(bad, 1)
        self.assertTrue(out[0].startswith("OK") or out[1].startswith("OK"))

    def test_assets_come_from_header_and_cmdline(self):
        p = self.write("d.py", "# REQUIRES_ASSET: /Game/Anim/Hdr\nprint(1)\n")
        got = precheck.collect_assets('x -ExecCmds="py d.py /Game/Anim/Cmd"', ["/Game/Anim/Exp"], [p])
        self.assertEqual(sorted(set(got)), ["/Game/Anim/Cmd", "/Game/Anim/Exp", "/Game/Anim/Hdr"])

    def test_vram_refuses_local_llm(self):
        out, bad = precheck.check_vram(["1234, ollama_llama_server.exe, 6000 MiB"])
        self.assertEqual(bad, 1)
        out, bad = precheck.check_vram(["99, UnrealEditor.exe, 3000 MiB"])
        self.assertEqual(bad, 0)

    def test_main_exit_code_and_failures_first(self):
        p = self.write("bad.py", "def f(:\n")
        rc = precheck.main(["--project", self.d, "--py", p, "--asset", "/Game/Anim/AS_Skel"])
        self.assertEqual(rc, 1)
        self.assertEqual(precheck.main(["--project", self.d, "--asset", "/Game/Anim/AS_Skel"]), 0)


if __name__ == "__main__":
    unittest.main()

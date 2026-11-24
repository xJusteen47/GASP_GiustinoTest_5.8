"""Offline test of Tools/CI/branch_elenco.py on a throwaway repo with a fake origin."""
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import branch_elenco as be  # noqa: E402


def g(repo, *a, env=None):
    e = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
    if env:
        e.update(env)
    subprocess.run(["git", "-C", repo] + list(a), check=True, capture_output=True, env=e)


class Elenco(unittest.TestCase):
    def test_categories(self):
        d = tempfile.mkdtemp()
        g(d, "init", "-q", "-b", "main")

        def commit(path, msg, date="2026-11-23T12:00:00"):
            os.makedirs(os.path.dirname(os.path.join(d, path)) or d, exist_ok=True)
            open(os.path.join(d, path), "w").write(msg)
            g(d, "add", "-A")
            g(d, "commit", "-qm", msg, env={"GIT_COMMITTER_DATE": date, "GIT_AUTHOR_DATE": date})

        commit("a.txt", "base")
        g(d, "checkout", "-q", "-b", "claude/in-main")           # A: same tip as main
        g(d, "checkout", "-q", "main")
        g(d, "checkout", "-q", "-b", "claude/docs-only")          # D
        commit("docs/x.md", "doc")
        g(d, "checkout", "-q", "main")
        g(d, "checkout", "-q", "-b", "claude/code-big")           # E, with a subset B
        commit("Source/a.cpp", "code1")
        g(d, "branch", "claude/code-sub")
        commit("Source/b.cpp", "code2")
        g(d, "checkout", "-q", "main")
        g(d, "checkout", "-q", "-b", "claude/old-one")            # C
        commit("Source/o.cpp", "old", date="2026-11-10T12:00:00")
        g(d, "checkout", "-q", "main")
        g(d, "checkout", "-q", "-b", "claude/artifacts")          # TIENI
        commit("docs/art.md", "art")
        g(d, "checkout", "-q", "main")
        for b in ("in-main", "docs-only", "code-big", "code-sub", "old-one", "artifacts"):
            g(d, "update-ref", "refs/remotes/origin/claude/" + b, "refs/heads/claude/" + b)
        g(d, "update-ref", "refs/remotes/origin/main", "refs/heads/main")
        out = os.path.join(d, "e.csv")
        self.assertEqual(be.main(["--repo", d, "--out", out, "--trunk", "origin/main"]), 0)
        got = {r.split(",")[0]: r.split(",")[4] for r in open(out).read().splitlines()[1:]}
        self.assertEqual(got["claude/in-main"], "A_GIA_IN_INTEGRAZIONE")
        self.assertEqual(got["claude/docs-only"], "D_SOLO_DOCS")
        self.assertEqual(got["claude/code-big"], "E_CODICE")
        self.assertEqual(got["claude/code-sub"], "B_SOTTOINSIEME")
        self.assertEqual(got["claude/old-one"], "C_STORIA_VECCHIA")
        self.assertEqual(got["claude/artifacts"], "TIENI")


if __name__ == "__main__":
    unittest.main()

"""Installs the kit's git hooks by pointing core.hooksPath at harness/scripts/git (idempotent, per clone and per worktree).
Run once in every clone (each PC, each cloud repo):  python harness/scripts/python/install_hooks.py"""
import os, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_config  # noqa: E402

root = kit_config.root()
hooks = os.path.relpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "git"), root).replace("\\", "/")
subprocess.run(["git", "-C", root, "config", "core.hooksPath", hooks], check=True)
try:
    os.chmod(os.path.join(root, hooks, "pre-commit"), 0o755)
except OSError:
    pass
print(f"OK: core.hooksPath = {hooks} (guardia pre-commit attiva)")

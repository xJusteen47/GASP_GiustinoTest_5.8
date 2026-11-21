"""Pre-commit guard: refuses a commit that stages a protected file (the owner's local tuning assets, secrets, big binaries).

Rules come from kit.config.json:
  "guard": {"protected_paths": ["Content/**/DA_*_Default.uasset", ".env"], "max_file_mb": 50}
A protected path may still be committed by the OWNER with  KIT_ALLOW_PROTECTED=1 git commit ...  (agents never set it).
Prints at most 5 lines; exit 1 blocks the commit.
"""
import fnmatch, os, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_config  # noqa: E402


def staged():
    out = subprocess.run(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"],
                         capture_output=True, text=True, check=True).stdout
    return [p for p in out.split("\0") if p]


def matches(path, pattern):
    # fnmatch '*' already crosses '/', so '**/' is the same as '*/' plus "no folder at all"
    return fnmatch.fnmatch(path, pattern) or fnmatch.fnmatch(path, pattern.replace("**/", ""))


def main():
    if os.environ.get("KIT_ALLOW_PROTECTED") == "1":
        print("GUARDIA: saltata (KIT_ALLOW_PROTECTED=1, solo owner)")
        return 0
    pats = kit_config.get("guard.protected_paths", []) or []
    max_mb = float(kit_config.get("guard.max_file_mb", 0) or 0)
    root = kit_config.root()
    bad = []
    for p in staged():
        hit = next((pat for pat in pats if matches(p, pat)), None)
        if hit:
            bad.append(f"{p} (protetto: {hit})")
            continue
        full = os.path.join(root, p)
        if max_mb and os.path.isfile(full) and os.path.getsize(full) > max_mb * 1024 * 1024:
            bad.append(f"{p} (> {max_mb:g} MB)")
    if not bad:
        return 0
    print(f"GUARDIA: commit BLOCCATO, {len(bad)} file non ammessi. Toglili con: git restore --staged <file>")
    for b in bad[:3]:
        print("  " + b)
    if len(bad) > 3:
        print(f"  ... altri {len(bad) - 3}")
    return 1


if __name__ == "__main__":
    sys.exit(main())

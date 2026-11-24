"""Classify the claude/* branches of origin against the trunk (cleanup plan 2026-11-24, point 6).

Cleanup-Branches.ps1 calls this to (re)write docs/notes/pulizia-2026-11-24/branch-elenco.csv from the state of the day; the
classification lives here, in Python, because it can be tested offline and the PowerShell script then only tags and deletes.

Categories (first match wins):
  TIENI              claude/artifacts, claude/approved-* (until the tag approved/* exists), the integration branches, --keep, open PRs
  A_GIA_IN_INTEGRAZIONE  the tip is already in the trunk (origin/main) or in an integration branch  -> delete
  B_SOTTOINSIEME     the tip is an ancestor of another branch that survives                          -> delete
  C_STORIA_VECCHIA   last commit older than --old-before (history before the 22/11 snapshot)         -> tag archivio/<name>, delete
  D_SOLO_DOCS        every file it changes against the trunk is a document                           -> tag, delete (after the docs-sweep)
  E_CODICE           the rest: code not in the trunk yet                                              -> NEVER deleted by default

Usage: python branch_elenco.py --out <csv> [--trunk origin/main] [--integration origin/claude/anim-skill-t3 ...]
       [--keep claude/x ...] [--open-pr claude/y ...] [--old-before 2026-11-22] [--repo <dir>]
"""
import argparse
import csv
import subprocess
import sys

DOC_PREFIXES = ("docs/", "handoffs/", "Artifacts/", "skills-public/")
DOC_SUFFIXES = (".md", ".txt", ".csv", ".png", ".jpg", ".jpeg", ".mp4", ".gif", ".json")
COLUMNS = ["branch", "sha", "ultimo_commit", "commit_unici", "categoria", "azione", "contenuto_in"]
ACTION = {
    "TIENI": "tenere",
    "A_GIA_IN_INTEGRAZIONE": "eliminare (gia nel tronco o nell'integrazione)",
    "B_SOTTOINSIEME": "eliminare",
    "C_STORIA_VECCHIA": "tag archivio/<nome> poi eliminare",
    "D_SOLO_DOCS": "tag archivio/<nome> poi eliminare (dopo il docs-sweep)",
    "E_CODICE": "tenere finche il codice non e nel tronco (merge dell'integrazione)",
}


def git(repo, *args):
    r = subprocess.run(["git", "-C", repo] + list(args), capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("git %s: %s" % (" ".join(args), r.stderr.strip()[:200]))
    return r.stdout


def list_branches(repo, remote="origin"):
    out = git(repo, "for-each-ref", "--format=%(refname:lstrip=3)|%(objectname)|%(committerdate:short)",
              "refs/remotes/%s/claude/" % remote)
    rows = []
    for l in out.splitlines():
        name, sha, date = l.split("|")
        rows.append({"branch": name, "sha": sha, "ultimo_commit": date})
    return rows


def is_doc(path):
    return path.startswith(DOC_PREFIXES) or path.endswith(DOC_SUFFIXES)


def classify(repo, rows, trunk, integration, keep, open_prs, old_before, remote="origin"):
    have_tag = set(git(repo, "tag", "--list", "approved/*").split())
    base_refs = [trunk] + list(integration)
    excl = ["^" + r for r in base_refs]
    by_name = {r["branch"]: r for r in rows}
    keepers = set(keep) | set(open_prs) | {"claude/artifacts"}
    keepers |= {i.split("/", 1)[1] for i in integration if i.startswith(remote + "/")}
    for r in rows:
        n = r["branch"]
        r["contenuto_in"] = ""
        if n in keepers:
            r["categoria"] = "TIENI"
            continue
        if n.startswith("claude/approved-"):
            # a locked clip lives in the tag approved/<suffix>; until the tag exists the branch IS the lock
            if "approved/" + n[len("claude/approved-"):] in have_tag:
                r["categoria"] = "A_GIA_IN_INTEGRAZIONE"
                r["contenuto_in"] = "tag approved/" + n[len("claude/approved-"):]
            else:
                r["categoria"] = "TIENI"
            continue
        uni = git(repo, "rev-list", "--count", r["sha"], *excl).strip()
        r["commit_unici"] = uni
        if uni == "0":
            r["categoria"] = "A_GIA_IN_INTEGRAZIONE"
    rest = [r for r in rows if "categoria" not in r]
    survivors = {r["branch"] for r in rows if r.get("categoria") == "TIENI"} | {r["branch"] for r in rest}
    # B: contained in another surviving branch (equal tips: only the first by name is kept)
    for r in sorted(rest, key=lambda r: r["branch"]):
        cont = git(repo, "for-each-ref", "--contains=" + r["sha"], "--format=%(refname:lstrip=3)",
                   "refs/remotes/%s/claude/" % remote).split()
        cont = sorted(c for c in cont if c != r["branch"] and c in survivors)
        same = [c for c in cont if by_name[c]["sha"] == r["sha"] and c < r["branch"]]
        if cont and (not [c for c in cont if by_name[c]["sha"] == r["sha"]] or same):
            target = (same or cont)[0]
            r["categoria"] = "B_SOTTOINSIEME"
            r["contenuto_in"] = target
            survivors.discard(r["branch"])
    for r in rest:
        if "categoria" in r:
            continue
        if r["ultimo_commit"] < old_before:
            r["categoria"] = "C_STORIA_VECCHIA"
            continue
        # only the files of the commits the trunk and the integration branches do not have (a branch built on the integration
        # branch must not be judged by what it inherited from it)
        files = [x for x in git(repo, "log", "--name-only", "--format=", r["sha"], *excl).splitlines() if x.strip()]
        r["categoria"] = "D_SOLO_DOCS" if files and all(is_doc(f) for f in files) else "E_CODICE"
    for r in rows:
        r.setdefault("commit_unici", "")
        r["azione"] = ACTION[r["categoria"]]
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out", required=True)
    ap.add_argument("--trunk", default="origin/main")
    ap.add_argument("--integration", nargs="*", default=[])
    ap.add_argument("--keep", nargs="*", default=[])
    ap.add_argument("--open-pr", nargs="*", default=[])
    ap.add_argument("--old-before", default="2026-11-22")
    a = ap.parse_args(argv)
    rows = classify(a.repo, list_branches(a.repo), a.trunk, a.integration, a.keep, a.open_pr, a.old_before)
    rows.sort(key=lambda r: (r["categoria"], r["branch"]))
    with open(a.out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=COLUMNS, extrasaction="ignore", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    counts = {}
    for r in rows:
        counts[r["categoria"]] = counts.get(r["categoria"], 0) + 1
    print("ELENCO: %d branch | %s" % (len(rows), " ".join("%s=%d" % (k.split("_")[0], v) for k, v in sorted(counts.items()))))
    return 0


if __name__ == "__main__":
    sys.exit(main())

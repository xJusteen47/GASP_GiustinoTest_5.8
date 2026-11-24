"""The clip registry (animpipe/registry.py, owner 3A 2026-11-24) offline: a temp git repo, no engine, no LFS server.
publish refuses without ASSERT: OK and a level-2 PASS that names the clip; approve refuses without the owner's words, before
publish and on uncommitted files, never moves a tag; the view keeps the owner's exemptions and every locked asset name.
Run: python tests/test_registry.py (or pytest)."""
import csv, json, os, subprocess, sys, tempfile

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(TOOLS))
sys.path.insert(0, TOOLS)
from animpipe import registry as R

ROW = {"mossa": "jab", "versione": "v13", "stato": "review", "responsabile": "thread", "asset_ue": "AS_JabV13_Build",
       "carta": "cards/pilot/jab_v13.json", "commit": "", "video_review": "", "data": "2026-11-24", "parole_owner": "", "note": ""}
OLD = {**ROW, "versione": "v12", "stato": "approvata", "asset_ue": "AS_JabV12_Build", "commit": "c699d46a",
       "parole_owner": "owner 2026-11-24: approvata"}
EXEMPT = '| AS_JabV12_Build | still_runs | `runs 18-37` | owner 2026-11-30: "va bene cosi\'" |'


def _git(root, *a):
    subprocess.run(["git", "-C", root, *a], check=True, capture_output=True)


def _repo(rows=(ROW, OLD)):
    root = tempfile.mkdtemp()
    _git(root, "init", "-q"); _git(root, "config", "user.email", "t@t"); _git(root, "config", "user.name", "t")
    os.makedirs(os.path.join(root, "Tools", "AnimPipeline")); os.makedirs(os.path.join(root, "docs", "notes"))
    with open(os.path.join(root, "docs", "notes", "anim-approvate.md"), "w", encoding="utf-8") as f:
        f.write("old rows\n\n" + R.EXEMPT_DEFAULT + EXEMPT + "\n")
    os.environ["ANIMPIPE_REPO"] = root
    R.write([dict(r) for r in rows])
    _git(root, "add", "-A"); _git(root, "commit", "-qm", "init")
    return root


def _asset(root, name="AS_JabV13_Build", data=b"uasset-bytes"):
    p = os.path.join(root, "work", f"{name}.uasset"); os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "wb").write(data)
    return p


def _review(root, assert_lines=("ASSERT: OK", "ASSERT: OK jab_hit AS_JabV13_Build f30-f50"), complete=True,
            verdict=None, clips=("AS_JabV13_Build",)):
    d = os.path.join(root, "docs", "notes", "reviews", "jab-v13"); os.makedirs(os.path.join(d, "final"), exist_ok=True)
    if assert_lines is not None:
        open(os.path.join(d, "assert.txt"), "w").write("\n".join(assert_lines) + "\n")
    json.dump({"complete": complete}, open(os.path.join(d, "final", "review_pack.json"), "w"))
    v = verdict or {"level": 2, "verdict": "PASS", "blind_critic": "PASS", "clips": list(clips)}
    json.dump(v, open(os.path.join(d, "final", "verdict.json"), "w"))
    return d


def _refused(fn, *a, has=""):
    try:
        fn(*a)
    except R.RegistryError as e:
        assert has in str(e), (has, str(e))
        return str(e)
    raise AssertionError(f"{fn.__name__}{a} was not refused")


def test_publish_needs_assert_ok_and_level2_pass_naming_the_clip():
    root = _repo(); a = _asset(root)
    _refused(R.publish, "jab", "v13", [a], None, has="--review")
    _refused(R.publish, "jab", "v13", [a], _review(root, assert_lines=None), has="ASSERT: OK")
    _refused(R.publish, "jab", "v13", [a], _review(root, assert_lines=("ASSERT: OK", "ASSERT: FAIL jab_hit idle owned f33")), has="ASSERT: OK")
    _refused(R.publish, "jab", "v13", [a], _review(root, complete=False), has="not complete")
    _refused(R.publish, "jab", "v13", [a], _review(root, verdict={"level": 2, "verdict": "PASS", "blind_critic": "FAIL", "clips": ["AS_JabV13_Build"]}), has="blind critic")
    _refused(R.publish, "jab", "v13", [a], _review(root, verdict={"level": 1, "verdict": "PASS", "blind_critic": "PASS", "clips": ["AS_JabV13_Build"]}), has="level-2")
    _refused(R.publish, "jab", "v13", [a], _review(root, clips=("AS_JabV12_Build",)), has="does not name")
    assert R.find(R.read(), "jab", "v13")["stato"] == "review"                         # nothing written by a refusal
    assert not os.path.exists(R.published_dir("jab", "v13"))
    res = R.publish("jab", "v13", [a], _review(root))
    assert res["stato"] == "PASS" and res["published"] == "Content/AnimPipeline/Published/jab/v13"
    man = json.load(open(os.path.join(R.published_dir("jab", "v13"), "publish.json")))
    assert set(man["files"]) == {"AS_JabV13_Build.uasset"} and not man["legacy_owner_approval"]
    row = R.find(R.read(), "jab", "v13")
    assert row["stato"] == "PASS" and row["video_review"] == "docs/notes/reviews/jab-v13"
    open(a, "wb").write(b"other bytes")                                               # a published version is never overwritten
    _refused(R.publish, "jab", "v13", [a], _review(root), has="never overwritten")


def test_approve_owner_words_commit_tag_view():
    root = _repo(); a = _asset(root)
    _refused(R.approve, "jab", "v13", "ok", has="publish")                           # review row: not published
    R.publish("jab", "v13", [a], _review(root))
    _refused(R.approve, "jab", "v13", "", has="owner")
    _refused(R.approve, "jab", "v13", "ok", None, False, has="commit the published files")
    _git(root, "add", "-A"); _git(root, "commit", "-qm", "publish jab v13")
    head = subprocess.run(["git", "-C", root, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    res = R.approve("jab", "v13", '"va bene, nuovo default"', None, False)
    assert res["tag"] == "approved/jab-v13" and res["commit"] == head[:8]
    tag = subprocess.run(["git", "-C", root, "rev-parse", "approved/jab-v13^{commit}"], capture_output=True, text=True).stdout.strip()
    assert tag == head
    row = R.find(R.read(), "jab", "v13")
    assert row["stato"] == "approvata" and row["parole_owner"] == '"va bene, nuovo default"'
    view = open(R.view_path(), encoding="utf-8").read()
    assert "AS_JabV13_Build" in view and "approved/jab-v13" in view and "AS_JabV12_Build" in view
    assert view.rstrip().endswith(EXEMPT) and "old rows" not in view                 # owner's exemptions kept verbatim
    assert R.registro("check")[0] == []


def test_tag_never_moves_and_locked_rows_never_published():
    root = _repo(); a = _asset(root)
    R.publish("jab", "v13", [a], _review(root))
    _git(root, "add", "-A"); _git(root, "commit", "-qm", "publish")
    _git(root, "tag", "approved/jab-v13", "HEAD~1")                                   # a tag already on another commit
    _refused(R.approve, "jab", "v13", "si", None, False, has="never moved")
    rows = R.read(); R.find(rows, "jab", "v12")["stato"] = "bloccata"; R.write(rows)
    _refused(R.publish, "jab", "v12", [_asset(root, "AS_JabV12_Build")], None, has="bloccata")


def test_legacy_owner_approval_stores_files_without_changing_state():
    root = _repo(); a = _asset(root, "AS_JabV12_Build")
    res = R.publish("jab", "v12", [a])                                               # approved before the registry existed
    assert res["legacy_owner_approval"] and res["stato"] == "approvata"
    assert R.find(R.read(), "jab", "v12")["parole_owner"] == OLD["parole_owner"]


def test_validate_and_cli_one_json_line():
    root = _repo()
    bad = [dict(ROW, stato="ok"), dict(OLD, parole_owner=""), dict(ROW)]
    probs = R.validate(bad)
    assert any("stato" in p for p in probs) and any("owner's words" in p for p in probs) and any("twice" in p for p in probs)
    env = dict(os.environ, ANIMPIPE_REPO=root, PYTHONIOENCODING="utf-8")
    for args, code in ((["registro", "check"], 1), (["approve", "jab", "v13"], 3), (["publish", "jab", "v13"], 1)):
        p = subprocess.run([sys.executable, "-m", "animpipe", *args], cwd=TOOLS, env=env, capture_output=True, text=True)
        lines = [l for l in p.stdout.splitlines() if l.strip()]
        assert len(lines) == 1 and json.loads(lines[0])["exit"] == code == p.returncode, (args, p.stdout, p.stderr[-400:])


def test_repo_registry_is_valid_and_view_in_sync():
    os.environ.pop("ANIMPIPE_REPO", None)
    rows = R.read()
    assert R.validate(rows) == [] and R.view_in_sync(), "run: python -m animpipe registro view"
    states = {(r["mossa"], r["versione"]): r["stato"] for r in rows}
    assert states[("dropkick", "k7")] == "bloccata"                                     # owner 11-24: no tag, not approved
    for k in [("runstart", "w3"), ("jab", "v12"), ("ftilt", "v31"), ("dair", "v9"), ("runstart", "w1"), ("runstart", "w2"),
              ("runstart", "v4r5"), ("runstart", "v4r7")]:
        assert states[k] == "approvata", k


if __name__ == "__main__":
    for n, f in list(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)

"""The public verbs (animpipe/verbs.py) on a synthetic move in a temp data + cards folder, no engine, no Cascadeur.
Build/check/render need the engine: their run records are written by hand to drive the status state machine.
Run: python tests/test_verbs.py (or pytest)."""
import json, os, shutil, subprocess, sys, tempfile, time

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(TOOLS, "tests", "fixtures")


def _setup():
    root = tempfile.mkdtemp()
    data, cards = os.path.join(root, "data"), os.path.join(root, "cards")
    os.makedirs(os.path.join(cards, "kit", "approved")); os.makedirs(os.path.join(data, "casc"))
    c = json.load(open(os.path.join(FIX, "overlord_dair_v4.json"), encoding="utf-8"))
    c["move"] = "m"
    for e in c["approved"]:
        e["ref"] = "kit/m.json"                                   # lock on itself: compared with approved/m.json
    json.dump(c, open(os.path.join(cards, "kit", "m.json"), "w", encoding="utf-8"), indent=1)
    snap = json.load(open(os.path.join(FIX, "approved", "overlord_dair_v4.json"), encoding="utf-8")); snap["move"] = "m"
    json.dump(snap, open(os.path.join(cards, "kit", "approved", "m.json"), "w"), indent=1)
    json.dump({"move": "m", "frames": [0, 15]}, open(os.path.join(data, "casc", "m_owner_keys.json"), "w"))
    return root, data, cards


def _ap(env, *args):
    p = subprocess.run([sys.executable, "-m", "animpipe", *args], cwd=TOOLS, env=env, capture_output=True, text=True, encoding="utf-8")
    lines = [l for l in p.stdout.splitlines() if l.strip()]
    assert len(lines) == 1, (args, p.stdout, p.stderr[-800:])       # the contract: one json line
    out = json.loads(lines[0])
    assert out["exit"] == p.returncode and set(out) >= {"ok", "verb", "move", "exit", "result", "next"}
    return out


def _env(data, cards):
    return dict(os.environ, ANIMPIPE_DATA=data, ANIMPIPE_CARDS=cards, PYTHONIOENCODING="utf-8")


def _card(cards):
    return json.load(open(os.path.join(cards, "kit", "m.json"), encoding="utf-8"))


def _inbetween(path, frame, why="arms keep loading", base=None):
    json.dump({"poses": [{"frame": frame, "why": why, "author": base}]}, open(path, "w"))


def _fake_run(data, verb, exit_code, result):
    d = os.path.join(data, "runs", "m"); os.makedirs(d, exist_ok=True)
    seq = f"{time.time_ns():020d}"
    json.dump({"verb": verb, "move": "m", "seq": seq, "exit": exit_code, "result": result, "log": ""},
              open(os.path.join(d, f"x_{seq}_{verb}.json"), "w"))
    return seq


def test_interp_bans_and_snapshot_restore():
    root, data, cards = _setup(); env = _env(data, cards)
    base = _card(cards)["author"]["poses"]["12"]
    sp = os.path.join(root, "p.json")
    before = open(os.path.join(cards, "kit", "m.json"), encoding="utf-8").read()
    _inbetween(sp, 15, base=base)                                   # an owner key (marked in Cascadeur)
    assert _ap(env, "interp", "m", f"--from={sp}")["exit"] == 3
    _inbetween(sp, 13, base=base)                                   # locked legs
    assert _ap(env, "interp", "m", f"--from={sp}")["exit"] == 3
    _inbetween(sp, 9, why="", base=base)                            # no why
    assert _ap(env, "interp", "m", f"--from={sp}")["exit"] == 2
    assert open(os.path.join(cards, "kit", "m.json"), encoding="utf-8").read() == before
    _inbetween(sp, 9, base=base)
    r = _ap(env, "interp", "m", f"--from={sp}")
    assert r["exit"] == 0 and r["result"]["added"] == [9] and r["result"]["snapshot"]
    q = next(q for q in _card(cards)["poses"] if q["frame"] == 9)
    assert q["origin"] == "claude" and q["why"]
    lst = _ap(env, "snapshot", "m", "--list")["result"]["snapshots"]
    assert lst and lst[-1]["verb"] == "interp"
    r = _ap(env, "restore", "m", lst[-1]["stamp"])
    assert r["exit"] == 0 and r["result"]["before"]
    assert open(os.path.join(cards, "kit", "m.json"), encoding="utf-8").read() == before
    assert _ap(env, "restore", "m", "1999")["exit"] == 2            # no match: usage, nothing touched
    shutil.rmtree(root)


def test_owner_ok_gates_approve():
    root, data, cards = _setup(); env = _env(data, cards)
    assert _ap(env, "approve", "m")["exit"] == 3                    # no owner ok
    os.makedirs(os.path.join(data, "visual", "m", "r1"))
    assert _ap(env, "owner-ok", "m")["exit"] == 2                   # no quote
    assert _ap(env, "owner-ok", "m", "--quote=ok vai")["exit"] == 0
    assert os.path.exists(os.path.join(data, "visual", "m", "r1", "owner_ok.json"))
    shutil.rmtree(root)


def test_variant_one_per_round_and_apply_needs_quote():
    root, data, cards = _setup(); env = _env(data, cards)
    r = _ap(env, "variant", "new", "m", "--why=arms higher in the charge")
    assert r["exit"] == 0 and r["result"]["variant"] == "m_varA" and r["result"]["anim"].endswith("_VarA")
    assert _ap(env, "variant", "new", "m", "--why=again")["exit"] == 1
    assert _ap(env, "status", "m")["next"] == "variant apply|discard"
    base = _card(cards)["author"]["poses"]["12"]
    sp = os.path.join(root, "p.json"); _inbetween(sp, 9, base=base)
    assert _ap(env, "interp", "m_varA", f"--from={sp}")["exit"] == 0
    _inbetween(sp, 15, base=base)                                   # the original's owner keys guard the variant too
    assert _ap(env, "interp", "m_varA", f"--from={sp}")["exit"] == 3
    orig = _card(cards)
    assert 9 not in [q["frame"] for q in orig["poses"]]             # original untouched
    assert _ap(env, "variant", "apply", "m")["exit"] == 3           # no owner words
    r = _ap(env, "variant", "apply", "m", "--quote=applica")
    assert r["exit"] == 0 and [x["frame"] for x in r["result"]["changed"]] == [9]
    new = _card(cards)
    assert new["move"] == "m" and new["anim"] == orig["anim"] and "variant" not in new
    assert 9 in [q["frame"] for q in new["poses"]] and not os.path.exists(os.path.join(cards, "kit", "m_varA.json"))
    assert _ap(env, "variant", "new", "m", "--why=b")["result"]["letter"] == "B"
    assert _ap(env, "variant", "discard", "m")["exit"] == 0
    shutil.rmtree(root)


def test_status_state_machine():
    root, data, cards = _setup(); env = _env(data, cards)
    assert _ap(env, "status", "m")["next"] == "build"
    sha = _ap(env, "status", "m")["result"]["card_sha"]
    b = _fake_run(data, "build", 0, {"card_sha": sha, "export_ok": True})
    assert _ap(env, "status", "m")["next"] == "check"
    assert _ap(env, "render", "m")["exit"] == 1                     # no render before the numeric gates
    _fake_run(data, "check", 1, {"build": b, "verdict": "FAIL", "blocking": ["clearance"]})
    assert _ap(env, "status", "m")["next"] == "interp"
    _fake_run(data, "check", 1, {"build": b, "verdict": "FAIL", "blocking": ["clearance"]})
    s = _ap(env, "status", "m")
    assert s["next"] == "ask_owner" and s["exit"] == 3              # max 2 attempts, then the owner
    b2 = _fake_run(data, "build", 0, {"card_sha": sha, "export_ok": True})
    _fake_run(data, "check", 0, {"build": b2, "verdict": "PASS", "blocking": []})
    assert _ap(env, "status", "m")["next"] == "render"
    os.makedirs(os.path.join(data, "visual", "m", "r1"))
    _fake_run(data, "render", 0, {"round": 1, "build": b2, "image": "x.png"})
    assert _ap(env, "status", "m")["next"] == "verdict"
    json.dump({"move": "m", "round": 1, "esito": "ok", "problemi": [], "segmenti": [{"da": 0, "a": 42, "stato": "ok"}]},
              open(os.path.join(data, "visual", "m", "r1", "verdict.json"), "w"))
    assert _ap(env, "verdict", "m")["exit"] == 0
    s = _ap(env, "status", "m")
    assert s["next"] == "owner-ok" and s["exit"] == 3
    _ap(env, "owner-ok", "m", "--quote=ok")
    assert _ap(env, "status", "m")["next"] == "approve"
    shutil.rmtree(root)


if __name__ == "__main__":
    for k, f in list(globals().items()):
        if k.startswith("test_"):
            f(); print("ok", k)

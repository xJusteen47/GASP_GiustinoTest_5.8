"""The public command surface: `python -m animpipe <verb> <move> [--options]`, one verb per concern.

    status   <move>                         state of the move and the next allowed verb (`next`)
    import   <move> [--bones=<json>]        the newest Cascadeur "Invia" request -> card (owner keys whole, never moved)
    interp   <move> --from=<poses.json>     in-betweens `origin: claude` on free frames only, each with a why
             <move> --none --why="..."      record that no in-between is needed;  --drop=6,11 removes claude in-betweens
    build    <move>                         card -> rig -> clip in the engine (needs the engine)
    check    <move>                         numeric gates only (no image, no interpolation)
    render   <move> [--force] [--standalone [--azs=90,0,45 --hitstop=8 --at=N]]   one composite image + manifest.json in visual/<move>/r<N>/ (refused while check fails)
    verdict  <move> [--round=N]             validate visual/<move>/r<N>/verdict.json written by the reviewer
    owner-ok <move> --quote="..." [--round=N]   record the owner's own words on a round (only the owner gives an ok)
    approve  <move> [--frames=] [--parts=] [--why=]   lock poses; refused without owner-ok on the current round
    export   <move>                         card -> Cascadeur scene (needs the engine)
    variant  new|show|apply|discard <move> [--why=] [--quote=]   one variant per round, the original is never touched
                                            until the owner's "applica" (apply needs --quote)
    (render/approve --frames, interp --drop/--from) take --video: the numbers are frames of the in-game video (v<N>); the
    result carries frame_map: "v13 = anim 13: between keys 11 and 14", printed before anything is written.
    verify   <move> --req=<file.json>       measurable requirements, PASS/FAIL per line (animpipe/verify.py)
    snapshot <move> [--why=] | --list       zip of card + owner keys + locks + manual layer (automatic before every write)
    restore  <move> <date>|last             put a snapshot back (snapshot of the current state first)
    revert   <move>                         = restore the snapshot taken before the last import

Contract: stdout is ONE json line {ok, verb, move, exit, result, next}; the full detail (and the legacy commands' text)
goes to <data>/runs/<move>/<stamp>_<verb>.json. Exit codes: 0 ok, 1 gate blocked, 2 wrong usage, 3 needs the owner,
4 needs an app (engine or Cascadeur not reachable). No network or model calls inside a verb: same input, same files.
The owner-key and locked-pose bans live here, not in the skill: a verb that would break them refuses.
"""
import contextlib, glob, io, json, os, re, shutil, socket, sys, time, traceback

from animpipe import config as K
from animpipe import snapshots as S

OK, GATE, USAGE, OWNER, APP = 0, 1, 2, 3, 4
VERBS = ("status", "import", "interp", "build", "check", "render", "verify", "verdict", "owner-ok", "approve", "export", "variant",
         "snapshot", "restore", "revert")
WRITERS = {"import", "interp", "build", "variant apply", "restore", "approve", "export"}     # take a snapshot first
MAX_FIX = 2                     # failed checks in a row before the owner is asked (token rule: max 2 attempts)


class Stop(Exception):
    def __init__(self, code, msg, **result):
        super().__init__(msg)
        self.code, self.result = code, dict(result, error=msg)


# --- moves, cards, runs ---
def card_path(move):
    if move.endswith(".json") or os.sep in move or "/" in move:
        raise Stop(USAGE, f"give the move name, not a path: {move}")
    hits = glob.glob(os.path.join(K.CARDS, "*", f"{move}.json")) + glob.glob(os.path.join(K.CARDS, f"{move}.json"))
    if len(hits) != 1:
        raise Stop(USAGE, f"move {move}: {len(hits)} cards found under {K.CARDS}")
    return os.path.abspath(hits[0])


def moves():
    out = []
    for p in sorted(glob.glob(os.path.join(K.CARDS, "*", "*.json"))):
        name = os.path.basename(p)[:-5]
        try:
            with open(p, encoding="utf-8") as f:
                if "poses" in json.load(f):
                    out.append(name)
        except (ValueError, OSError):
            pass
    return out


def read(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=1, ensure_ascii=False); f.write("\n")
    os.replace(tmp, path)


def stamp():
    return time.strftime("%Y%m%d_%H%M%S")


def runs(move, verb=None):
    """Run records oldest first (records of one second keep their write order through the _<n> suffix)."""
    d = os.path.join(K.RUNS, move)
    out = []
    for p in glob.glob(os.path.join(d, "*.json")):
        try:
            r = read(p)
        except (ValueError, OSError):
            continue
        if verb is None or r.get("verb") == verb:
            out.append(r)
    return sorted(out, key=lambda r: r.get("seq", ""))


def last(move, verb, ok=None, **match):
    for r in reversed(runs(move, verb)):
        if ok is not None and (r["exit"] == 0) != ok:
            continue
        if all(r.get("result", {}).get(k) == v for k, v in match.items()):
            return r
    return None


def record(move, verb, code, result, log):
    d = os.path.join(K.RUNS, move); os.makedirs(d, exist_ok=True)
    seq = f"{time.time_ns():020d}"          # sortable across processes, unique within one second
    p = os.path.join(d, f"{stamp()}_{seq[-9:-3]}_{verb}.json")
    write(p, {"verb": verb, "move": move, "seq": seq, "exit": code, "result": result, "log": log})
    return p


# --- owner keys and locks: the bans ---
def owner_frames(c, move):
    """Frames that belong to the owner: Cascadeur 'Segna chiave' marks, poses with a source, owner-approved locks."""
    fr = set()
    p = os.path.join(K.CASC, f"{move}_owner_keys.json")
    if os.path.exists(p):
        fr |= {int(f) for f in read(p).get("frames", [])}
    if c.get("variant"):                   # a variant keeps the original's marks
        fr |= owner_frames({"poses": [], "approved": []}, c["variant"]["of"])
    fr |= {int(q["frame"]) for q in c["poses"] if q.get("source") or q.get("origin") == "owner"}
    fr |= {int(f) for e in c.get("approved", []) if e.get("by", "owner") == "owner" for f in e["frames"]}
    return fr


def locked(c):
    return {(int(f), p) for e in c.get("approved", []) for f in e["frames"] for p in e["parts"]}


def _ban(c, move, frame, what):
    if frame in owner_frames(c, move):
        raise Stop(OWNER, f"{what}: frame {frame} is an owner key; never moved or edited, propose it to the owner instead",
                   frame=frame)
    lk = sorted(p for f, p in locked(c) if f == frame)
    if lk:
        raise Stop(OWNER, f"{what}: frame {frame} is locked ({', '.join(lk)}); ask the owner to unlock", frame=frame)


# --- status: the state machine ---
def pending_imports(move):
    d = os.path.join(K.CASC, "requests")
    return sorted(glob.glob(os.path.join(d, f"*_{move}_import.json")))


def open_variant(move, card=None):
    card = card or card_path(move)
    hits = sorted(glob.glob(os.path.join(os.path.dirname(card), f"{move}_var[A-Z].json")))
    return hits[-1] if hits else None


def _visual():
    from animpipe import visual
    return visual


def state(move):
    """{next, why, owner, round, card_sha}: `owner` = the next step needs the owner (status exits 3)."""
    card = card_path(move)
    c = read(card)
    sha = S.sha(card)
    st = {"card_sha": sha, "owner_keys": sorted(owner_frames(c, move)), "pending_imports": len(pending_imports(move))}

    def nx(verb, why, owner=False, **kw):
        return dict(st, next=verb, why=why, owner=owner, **kw)

    is_var = c.get("variant")
    var = None if is_var else open_variant(move, card)
    if var:
        return nx("variant apply|discard", f"variant {os.path.basename(var)[:-5]} open: the owner applies or discards it", True)
    if st["pending_imports"]:
        return nx("import", "a Cascadeur 'Invia' request is waiting")
    imp = last(move, "import", ok=True)
    b = last(move, "build", ok=True)
    if not b or b["result"].get("card_sha") != sha:
        it = last(move, "interp", ok=True)
        if imp and (not it or it["seq"] < imp["seq"]):
            return nx("interp", "new owner keys imported: add in-betweens on free frames, or interp --none with a why")
        return nx("build", "the card changed since the last build")
    since = imp["seq"] if imp else ""
    ch = last(move, "check", build=b["seq"])
    if not ch:
        return nx("check", "built, numeric gates not run on this build")
    if ch["exit"] != 0:
        fails = [r for r in runs(move, "check") if r["seq"] > since and r["exit"] != 0]
        oks = [r for r in runs(move, "owner-ok") if r["seq"] > since]
        if len(fails) >= MAX_FIX and not oks:
            return nx("ask_owner", f"check failed {len(fails)} times: stop, send the owner the diagnosis", True)
        return nx("interp", f"check blocked ({', '.join(ch['result'].get('blocking', []))}): fix ONE thing with interp, then build")
    rd = last(move, "render", ok=True, build=b["seq"])
    rounds = [r for r in runs(move, "render") if r["seq"] > since and r["exit"] == 0]
    if not rd:
        return nx("render", "numeric gates pass: render the composite image")
    rnd = rd["result"]["round"]
    vb = _visual().blocked(move, rnd)
    if vb is None:
        return nx("verdict", f"write visual/{move}/r{rnd}/verdict.json (review.md), then run verdict", round=rnd)
    okp = os.path.join(K.VISUAL, move, f"r{rnd}", "owner_ok.json")
    if vb[0]:
        if len(rounds) >= _visual().MAX_ROUNDS and not os.path.exists(okp):
            return nx("ask_owner", f"round {rnd} still blocked after {len(rounds)} rounds: send the owner image + verdict", True, round=rnd)
        return nx("interp", f"round {rnd} blocked: fix ONE thing with interp, then build again", round=rnd)
    if is_var:
        return nx("variant apply|discard", f"variant round {rnd} passes: show the owner original vs variant "
                  f"(variant show {is_var['of']}), he applies or discards it", True, round=rnd)
    if not os.path.exists(okp):
        return nx("owner-ok", f"send the owner the image of round {rnd} + a 5-line summary, then record his words", True, round=rnd)
    ok_rec = read(okp)
    ap = last(move, "approve", ok=True)
    if ok_rec.get("card_sha") == sha and (not ap or ap["seq"] < ok_rec.get("seq", "")):
        return nx("approve", f"the owner said ok on round {rnd}", round=rnd)
    ex = last(move, "export", ok=True)
    if not ex or (ap and ex["seq"] < ap["seq"]):
        return nx("export", "approved: send the scene back to Cascadeur", round=rnd)
    return nx("done", "approved and exported", round=rnd)


# --- legacy calls: their text goes to the run log ---
def _quiet(fn, *a, **kw):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        try:
            v = fn(*a, **kw)
        except SystemExit as e:
            code = e.code if isinstance(e.code, int) else (0 if e.code is None else 1)
            if code:
                raise Stop(GATE, f"{getattr(fn, '__name__', fn)}: {e.code}", log=buf.getvalue())
            v = None
    return v, buf.getvalue()


def _app_down(e):
    if isinstance(e, (ConnectionError, socket.timeout, TimeoutError)):
        return True
    return isinstance(e, OSError) and getattr(e, "winerror", None) in (10061, 10060) or "refused" in str(e).lower() and "approve" not in str(e)


# --- video frames: the owner counts frames of the in-game video, the card counts anim frames (animpipe/vframes.py) ---
def _vmap(move, o, frames, video):
    """frames as given -> anim frames. With --video they are video frames (--at/--hitstop set the freeze, default: first
    contact, 8). Stores the printed "requested frame -> key/segment touched" map in o["_fmap"] (result.frame_map)."""
    from animpipe import card as C, vframes as V
    c = read(card_path(move))
    a0, hit = V.params(c, o)
    try:
        anim = V.to_anim_list(frames, a0, hit) if video else list(frames)
    except ValueError as e:
        raise Stop(USAGE, str(e))
    rows = V.frame_map(c, anim, a0, hit, owner_frames(c, move))
    o.setdefault("_fmap", []).extend([V.line(r) for r in rows] + ([f"hitstop {hit} at anim {a0}"] if video else []))
    return anim


def _timeline_opts(move, verb, o):
    """--frames / --drop on render and approve and interp: honour --video, print the map. interp --from is done in v_interp."""
    video = "video" in o
    for key in ("frames", "drop"):
        if o.get(key) and (key == "drop") == (verb == "interp"):
            o[key] = ",".join(str(f) for f in _vmap(move, o, __import__("animpipe.vframes", fromlist=["x"]).parse(o[key]), video))


# --- verbs ---
def v_status(move, a, o):
    st = state(move)
    return (OWNER if st["owner"] else OK), st, ""


def v_snapshot(move, a, o):
    card = card_path(move)
    if "list" in o:
        return OK, {"snapshots": [{k: m.get(k) for k in ("stamp", "reason", "verb")} for m in S.listing(move)]}, ""
    s = S.take(move, card, o.get("why") or "manual", "snapshot")
    return OK, {"stamp": s}, ""


def v_restore(move, a, o):
    card = card_path(move)
    if not a:
        raise Stop(USAGE, "restore <move> <date>|last")
    try:
        s = S.resolve(move, a[0])
    except LookupError as e:
        raise Stop(USAGE, str(e))
    r = S.restore(move, card, s)
    return OK, dict(r, stamp=s), ""


def v_revert(move, a, o):
    imp = [m for m in S.listing(move) if m.get("verb") == "import"]
    if not imp:
        raise Stop(USAGE, f"no snapshot taken before an import of {move}")
    return v_restore(move, [imp[-1]["stamp"]], o)


def v_import(move, a, o):
    from animpipe import cascadeur as KC, card as C
    card = card_path(move)
    c = read(card)
    reqs = pending_imports(move)
    opt = {k: o[k] for k in ("bones", "keys", "dry") if k in o}
    req = None
    if not opt.get("bones"):
        if not reqs:
            raise Stop(USAGE, "no Cascadeur 'Invia' request waiting (or give --bones=<json> offline)")
        req = read(reqs[-1])
        opt.update(fbx=req["fbx"], keys=",".join(str(k) for k in req["keys"]))
    keys = {int(k) for k in opt["keys"].split(",")} if opt.get("keys") else None
    marked = set(int(f) for f in (req or {}).get("owner_keys", []))
    prev_owner = owner_frames(c, move)
    if keys is not None and not (marked | prev_owner) <= keys:
        miss = sorted((marked | prev_owner) - keys)
        raise Stop(OWNER, f"owner keys {miss} have no key in the Cascadeur scene: moved or deleted? ask the owner, nothing imported",
                   missing=miss)
    _, log = _quiet(KC.import_cmd, card, opt)
    after = read(card)
    have = set(C.key_frames(after))
    if not (marked | prev_owner) <= have:
        s = S.resolve(move, "last"); S.restore(move, card, s)
        raise Stop(GATE, f"import dropped owner keys {sorted((marked | prev_owner) - have)}: card restored", log=log)
    if req:
        done = os.path.join(os.path.dirname(reqs[-1]), "done"); os.makedirs(done, exist_ok=True)
        for p in reqs:
            shutil.move(p, os.path.join(done, os.path.basename(p)))
    return OK, {"keys": sorted(have), "owner_keys": sorted(marked | prev_owner), "card_sha": S.sha(card),
                "changed": S.sha(card) != o.get("_sha_before")}, log


def v_interp(move, a, o):
    from animpipe import card as C
    card = card_path(move)
    c = read(card)
    if "none" in o:
        if not o.get("why"):
            raise Stop(USAGE, "interp --none needs --why")
        return OK, {"added": [], "why": o["why"]}, ""
    n = int(c["author"]["frames"])
    drop = [int(x) for x in o.get("drop", "").split(",") if x]
    for f in drop:
        _ban(c, move, f, "interp --drop")
        q = next((q for q in c["poses"] if q["frame"] == f), None)
        if not q or q.get("origin") != "claude":
            raise Stop(USAGE, f"interp --drop: frame {f} is not an in-between of origin claude")
        c["poses"].remove(q); c["author"]["poses"].pop(str(f), None)
    added = []
    if o.get("from"):
        spec = read(o["from"])
        if spec.get("poses"):
            conv = _vmap(move, o, [p.get("frame") for p in spec["poses"] if isinstance(p.get("frame"), int)], "video" in o)
            for p, f in zip([p for p in spec["poses"] if isinstance(p.get("frame"), int)], conv):
                p["frame"] = f
        for p in spec.get("poses", []):
            f = p.get("frame")
            if not isinstance(f, int) or not 0 < f < n:
                raise Stop(USAGE, f"interp: frame {f!r} outside 1..{n - 1}")
            if not str(p.get("why", "")).strip() or not isinstance(p.get("author"), dict):
                raise Stop(USAGE, f"interp: frame {f} needs a why and an author pose")
            _ban(c, move, f, "interp")
            old = next((q for q in c["poses"] if q["frame"] == f), None)
            if old and old.get("origin") != "claude":
                raise Stop(OWNER, f"interp: frame {f} is a pose of origin {old.get('origin') or 'source'}, not an in-between")
            if old:
                c["poses"].remove(old)
            q = {"name": p.get("name", f"inbetween_{f}"), "frame": f, "source": None, "origin": "claude", "why": p["why"]}
            if p.get("spacing"):
                q["spacing"] = p["spacing"]
            c["poses"].append(q); c["author"]["poses"][str(f)] = p["author"]; added.append(f)
    if not added and not drop:
        raise Stop(USAGE, "interp needs --from=<poses.json>, --drop=<frames> or --none --why=")
    c["poses"].sort(key=lambda q: q["frame"])
    c["author"]["poses"] = {k: c["author"]["poses"][k] for k in sorted(c["author"]["poses"], key=int)}
    write(card, c)
    try:
        C.load(card)                       # the card's own guards (approved locks, fidelity of owner poses)
    except ValueError as e:
        S.restore(move, card, S.resolve(move, "last"))
        raise Stop(GATE, f"interp broke a card guard, card restored: {str(e)[:300]}")
    return OK, {"added": added, "dropped": drop, "card_sha": S.sha(card)}, ""


def _need_app(fn, *a):
    try:
        return _quiet(fn, *a)
    except Stop:
        raise
    except Exception as e:
        if _app_down(e):
            raise Stop(APP, f"engine/Cascadeur not reachable: {e}")
        raise


def v_build(move, a, o):
    from animpipe.__main__ import build, OUT
    card = card_path(move)
    _, log = _need_app(build, card)
    r = read(os.path.join(OUT, f"{move}_build.json"))
    ok = bool(r.get("export_ok"))
    return (OK if ok else GATE), {"card_sha": S.sha(card), "export_ok": ok, "sec": r.get("sec")}, log


def v_check(move, a, o):
    from animpipe.__main__ import OUT
    from animpipe.checks import run
    card = card_path(move)
    b = last(move, "build", ok=True)
    if not b or b["result"].get("card_sha") != S.sha(card):
        raise Stop(USAGE, "the card changed since the last build: run build first")
    _, log = _need_app(run, card)
    v = read(os.path.join(OUT, f"{move}_check.json"))
    bad = [k for k, x in v["block"].items() if not x["ok"]]
    return (OK if v["verdict"] == "PASS" else GATE), {"build": b["seq"], "verdict": v["verdict"], "blocking": bad}, log


def _render_standalone(move, card, o):
    """In-game videos from a second, separate editor process on the saved capture map (animpipe/standalone.py): no UnrealMCP,
    nothing of the owner's editor is used or touched. Writes sheets/ingame/<move>_az<N>_ingame{,_whiff,_compare}.mp4 + strip."""
    from animpipe import standalone as SA, vframes as V
    c = read(card)
    a0, hit = V.params(c, o)
    azs = [int(x) for x in (o.get("azs") or "90,0,45").split(",")]
    try:
        r = SA.run(c, card, azs, hit, int(o["at"]) if o.get("at") else None)
    except SA.Unavailable as e:
        raise Stop(APP, f"standalone render not possible here: {e}")
    except RuntimeError as e:
        raise Stop(GATE, str(e)[:400])
    return OK, {"standalone": True, "azs": azs, "hitstop": hit, "at": a0, "files": r["files"][:12], "editor_log": r["log"],
                "video_frames": "v<N> in every video = the owner's frame count (0-based); f<n> = anim frame"}, ""


def v_render(move, a, o):
    from animpipe import poses
    card = card_path(move)
    b = last(move, "build", ok=True)
    if not b or b["result"].get("card_sha") != S.sha(card):
        raise Stop(USAGE, "the card changed since the last build: run build first")
    ch = last(move, "check", build=b["seq"])
    if (not ch or ch["exit"] != 0) and "force" not in o:
        raise Stop(GATE, "no render while the numeric gates fail on this build (check first)")
    if "standalone" in o:
        return _render_standalone(move, card, o)
    _, log = _need_app(poses.run, card, {k: o[k] for k in ("frames", "take", "over") if k in o})
    img = [l for l in log.splitlines() if l.strip().lower().endswith(".png")]
    if not img or not os.path.exists(img[-1].strip()):
        raise Stop(GATE, "render wrote no image", log=log)
    vis = os.path.join(K.VISUAL, move)
    have = [int(m.group(1)) for d in glob.glob(os.path.join(vis, "r*")) if (m := re.fullmatch(r"r(\d+)", os.path.basename(d)))]
    rnd = max(have, default=0) + 1
    d = os.path.join(vis, f"r{rnd}"); os.makedirs(d)
    shutil.copyfile(img[-1].strip(), os.path.join(d, "grid.png"))
    c = read(card)
    from animpipe.card import key_frames
    write(os.path.join(d, "manifest.json"), {"move": move, "round": rnd, "card_sha": S.sha(card), "build": b["seq"],
          "image": "grid.png", "keys": key_frames(c), "owner_keys": sorted(owner_frames(c, move)),
          "inbetweens": [{"frame": q["frame"], "why": q.get("why")} for q in c["poses"] if q.get("origin") == "claude"]})
    return OK, {"round": rnd, "build": b["seq"], "image": os.path.join(d, "grid.png")}, log


def v_verify(move, a, o):
    """Measurable requirements -> PASS/FAIL per line (animpipe/verify.py). Read-only; exit 1 when a line fails."""
    from animpipe import verify as VF
    if not o.get("req") or not os.path.exists(o["req"]):
        raise Stop(USAGE, "verify needs --req=<file.json> (format: animpipe/verify.py docstring)")
    c = read(card_path(move))
    try:
        rows, take, src = VF.run(c, read(o["req"]), os.path.join(K.TOOLS, "sheets", "raw"))
    except FileNotFoundError as e:
        raise Stop(GATE, str(e))
    out = os.path.join(K.OUT, f"{move}_verify.json")
    write(out, {"take": take, "dump": src, "rows": rows})
    bad = [r for r in rows if not r["ok"]]
    return (OK if not bad else GATE), {"pass": len(rows) - len(bad), "fail": len(bad), "lines": [r["line"] for r in rows][:8], "file": out}, ""


def v_verdict(move, a, o):
    V = _visual()
    rnd = int(o["round"]) if o.get("round") else (V.rounds(move) or [None])[-1]
    if rnd is None:
        raise Stop(USAGE, f"no verdict.json in {os.path.join(K.VISUAL, move)}/r<N>/: write it first (review.md)")
    code, log = _quiet(V.gate, card_path(move), rnd)
    code = code if isinstance(code, int) else OK
    if code == 2:
        raise Stop(USAGE, f"verdict r{rnd}: missing or invalid", log=log)
    return code, {"round": rnd, "blocked": code == GATE}, log


def v_owner_ok(move, a, o):
    card = card_path(move)
    if not str(o.get("quote", "")).strip():
        raise Stop(USAGE, "owner-ok needs --quote=\"<the owner's own words>\"")
    rnd = int(o["round"]) if o.get("round") else max(
        [int(m.group(1)) for d in glob.glob(os.path.join(K.VISUAL, move, "r*")) if (m := re.fullmatch(r"r(\d+)", os.path.basename(d)))] or [0])
    if not rnd:
        raise Stop(USAGE, "no rendered round to say ok on")
    p = os.path.join(K.VISUAL, move, f"r{rnd}", "owner_ok.json")
    rec = {"move": move, "round": rnd, "quote": o["quote"], "by": o.get("by", "owner"), "at": stamp(), "card_sha": S.sha(card),
           "seq": f"{time.time_ns():020d}"}
    write(p, rec)
    return OK, {"round": rnd, "file": p}, ""


def v_approve(move, a, o):
    from animpipe import locks
    from animpipe.card import key_frames
    card = card_path(move)
    c = read(card)
    rnd = max([int(m.group(1)) for d in glob.glob(os.path.join(K.VISUAL, move, "r*")) if (m := re.fullmatch(r"r(\d+)", os.path.basename(d)))] or [0])
    okp = os.path.join(K.VISUAL, move, f"r{rnd}", "owner_ok.json")
    if not rnd or not os.path.exists(okp):
        raise Stop(OWNER, "approve needs the owner's ok on the current round (owner-ok --quote=...)")
    rec = read(okp)
    if rec.get("card_sha") != S.sha(card):
        raise Stop(OWNER, f"the card changed after the owner's ok on round {rnd}: show him the new round")
    frames = o["frames"].split(",") if o.get("frames") else [str(f) for f in key_frames(c)]
    parts = o["parts"].split(",") if o.get("parts") else list(locks.PARTS[:6])
    try:
        _, log = _quiet(locks.approve, card, frames, parts, o.get("why") or f"owner ok r{rnd}: {rec['quote']}", "owner")
    except ValueError as e:
        raise Stop(GATE, str(e))
    return OK, {"round": rnd, "frames": [int(f) for f in frames], "parts": parts, "card_sha": S.sha(card)}, log


def v_export(move, a, o):
    from animpipe import cascadeur as KC
    card = card_path(move)
    _, log = _need_app(KC.export_cmd, card, {k: v for k, v in o.items() if not k.startswith("_")})
    return OK, {"scene": os.path.join(K.CASC, f"{move}.casc")}, log


def v_variant(move, a, o):
    """new: copy of the card as <move>_var<A> (its own clip AS_..._Var<A>), the owner's keys and locks are guarded against
    the original card. show: frames that differ and their why. apply (--quote): the original takes the variant's poses.
    discard: the variant goes to variants/done. Variants are kept, never deleted."""
    sub = a[0] if a else ""
    card = card_path(move)
    c = read(card)
    var = open_variant(move, card)
    if sub == "new":
        if var:
            raise Stop(GATE, f"one variant per round: {os.path.basename(var)[:-5]} is open (apply or discard it)")
        if not str(o.get("why", "")).strip():
            raise Stop(USAGE, "variant new needs --why")
        used = {r["result"].get("letter") for r in runs(move, "variant") if r["exit"] == 0}
        letter = next(L for L in "ABCDEFGHIJKLMNOPQRSTUVWXYZ" if L not in used)
        v = json.loads(json.dumps(c))
        v["move"] = f"{move}_var{letter}"; v["anim"] = f"{c['anim']}_Var{letter}"
        v["variant"] = {"of": move, "letter": letter, "why": o["why"], "base_sha": S.sha(card), "at": stamp()}
        for e in v.get("approved", []):          # a lock on the card itself: the variant compares with the original
            if os.path.normcase(os.path.join(K.CARDS, e["ref"])) == os.path.normcase(card):
                e["ref"] = os.path.relpath(card, K.CARDS).replace(os.sep, "/")
        vp = os.path.join(os.path.dirname(card), f"{v['move']}.json")
        write(vp, v)
        return OK, {"letter": letter, "variant": v["move"], "anim": v["anim"]}, ""
    if not var:
        raise Stop(USAGE, f"no open variant of {move}")
    v = read(var); vname = v["move"]
    po = {q["frame"]: q for q in c["poses"]}; pv = {q["frame"]: q for q in v["poses"]}
    ao, av = c["author"]["poses"], v["author"]["poses"]
    changed = sorted({int(f) for f in set(ao) | set(av) if ao.get(f) != av.get(f)} | {f for f in set(po) | set(pv) if po.get(f) != pv.get(f)})
    info = {"variant": vname, "letter": v["variant"]["letter"], "why": v["variant"]["why"],
            "changed": [{"frame": f, "why": (pv.get(f) or {}).get("why", "removed" if f not in pv else "")} for f in changed]}
    if sub == "show":
        rd = last(vname, "render", ok=True)
        return OK, dict(info, image=rd["result"]["image"] if rd else None,
                        original_image=(last(move, "render", ok=True) or {}).get("result", {}).get("image")), ""
    done = os.path.join(K.DATA, "variants", "done"); os.makedirs(done, exist_ok=True)
    if sub == "discard":
        shutil.move(var, os.path.join(done, f"{vname}_{stamp()}.json"))
        return OK, dict(info, discarded=True), ""
    if sub == "apply":
        if not str(o.get("quote", "")).strip():
            raise Stop(OWNER, "variant apply needs the owner's explicit 'applica' (--quote=\"<his words>\")")
        if v["variant"]["base_sha"] != S.sha(card):
            raise Stop(GATE, f"{move} changed after the variant was made: discard it and make a new one")
        bad = sorted({f for f in changed if f in owner_frames(c, move) or any(lf == f for lf, _ in locked(c))})
        if bad:
            raise Stop(OWNER, f"the variant changes owner keys or locked poses {bad}: refused", frames=bad)
        new = json.loads(json.dumps(v))
        new["move"], new["anim"] = c["move"], c["anim"]; new.pop("variant")
        new["approved"] = c.get("approved", [])
        write(card, new)
        shutil.move(var, os.path.join(done, f"{vname}_{stamp()}.json"))
        return OK, dict(info, applied=True, quote=o["quote"], card_sha=S.sha(card)), ""
    raise Stop(USAGE, "variant new|show|apply|discard <move>")


TABLE = {"status": v_status, "snapshot": v_snapshot, "restore": v_restore, "revert": v_revert, "import": v_import,
         "interp": v_interp, "build": v_build, "check": v_check, "render": v_render, "verify": v_verify, "verdict": v_verdict,
         "owner-ok": v_owner_ok, "approve": v_approve, "export": v_export, "variant": v_variant}


def run(argv):
    """argv = [verb, move, ...]. Returns the exit code after printing the one json line."""
    verb = argv[0] if argv else ""
    pos = [x for x in argv[1:] if not x.startswith("--")]
    o = dict((x[2:].split("=", 1) + [""])[:2] for x in argv[1:] if x.startswith("--"))
    if verb == "variant" and pos and pos[0] in ("new", "show", "apply", "discard"):
        pos = [pos[1]] + [pos[0]] + pos[2:] if len(pos) > 1 else pos[::-1]
    move = pos[0] if pos else ""
    code, result, log, nxt = USAGE, {}, "", None
    try:
        if verb not in TABLE or not move:
            raise Stop(USAGE, f"usage: animpipe <{'|'.join(VERBS)}> <move> [--options]")
        key = f"{verb} {pos[1]}" if verb == "variant" and len(pos) > 1 else verb
        card = card_path(move)
        if key in WRITERS:
            o["_sha_before"] = S.sha(card)
            o["_snapshot"] = S.take(move, card, f"before {key}", verb)
        if verb in ("render", "approve", "interp"):
            _timeline_opts(move, verb, o)
        code, result, log = TABLE[verb](move, pos[1:], o)
        if "_fmap" in o:
            result["frame_map"] = o["_fmap"]
        if "_snapshot" in o:
            result["snapshot"] = o["_snapshot"]
    except Stop as e:
        code, result = e.code, e.result
        log = result.pop("log", "")
        if "_fmap" in o:
            result["frame_map"] = o["_fmap"]
    except Exception as e:
        code, result, log = GATE, {"error": f"{type(e).__name__}: {str(e)[:300]}"}, traceback.format_exc()
    rec = None
    if move and verb in TABLE and verb != "status" and (code != USAGE or log):
        try:
            rec = record(move, verb, code, result, log)
        except OSError:
            pass
    if move and verb != "status":
        try:
            nxt = state(move)["next"]
        except Exception:
            nxt = None
    elif verb == "status":
        nxt = result.get("next")
    line = {"ok": code == OK, "verb": verb, "move": move, "exit": code, "result": result, "next": nxt}
    if rec:
        line["detail"] = rec
    print(json.dumps(line, ensure_ascii=False, default=str))
    return code

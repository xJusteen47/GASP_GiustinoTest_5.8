"""One animpipe round as data: steps, timed, skipped when already done, one editor for all of them (import-veloce-1, 2026-11-24).

    python round_runner.py <round.json> [--keep] [--force=id1,id2] [--dry] [--no-cache]

Why (K8 round 2: animpipe + import 6 min, round 1 12, K7 15): every round re-ran every step, booted a new editor, and sampled the
same clip three times. This runner changes only HOW the steps run, never what they compute (owner 11-24: same quality as the OG):
  1. Offline steps first ("editor": false, e.g. card generation, card/fidelity gates): a FAIL stops the round before any editor.
     The editor boot starts in the background meanwhile when an editor step will run, so its 20 s - minutes overlap them.
  2. One editor session (editor_session.py) for every editor step; a live session from the previous round (--keep) is reused:
     no boot, no map load, no second shader warm-up.
  3. A step with "inputs" is skipped when its stamp (sha256 of the step definition + every input file's bytes) equals the last
     successful run and its "outputs" exist. A changed card, script or clip changes the stamp, so the step runs again. Steps without
     inputs always run. --force=<ids> runs them anyway.
  4. Bridge samples go through animpipe/samplecache.py (same answers, asked once per saved clip); --no-cache turns it off.
  5. Every step and bridge call is timed into Saved/AnimPipeline/timing/<round>_<stamp>.jsonl; import_timing_report.py reads it.
Limits (rule 5): each step has "timeout_min" (default 15; build/check steps declare theirs); a timeout ends the round with
ESITO: TIMEOUT and asks the session to quit. The session itself runs under Run-Timed (-MaxMinutes from the round file).

round.json: {"name": "k8r3", "cwd": "Tools/AnimPipeline", "session": {"max_minutes": 120},
  "steps": [{"id": "cards", "editor": false, "cmd": ["python", "x.py"], "inputs": ["x.py"], "outputs": ["card.json"]},
            {"id": "build", "editor": true, "cmd": ["python", "-m", "animpipe", "build", "card.json"], "inputs": [...], "timeout_min": 45},
            {"id": "variant", "job": {"script": "Tools/AnimPipeline/cards/.../ue_step.py", "entry": "run", "kwargs": {...}}},
            {"id": "capture", "job": {"script": "...boot.py", "mode": "boot", "env": {...}}, "assert": "<out>/assert.txt"}]}
"ok_pattern": a regex the step's output must contain to count as done (legacy animpipe commands exit 0 on failure).
Paths in cmd/inputs/outputs are relative to "cwd" (inputs/outputs may be globs); job scripts are relative to the repo root.
Prints at most 5 lines; exit 0 OK, 1 step FAIL, 3 ASSERT not OK, 124 TIMEOUT, 4 no editor.
"""
import glob, hashlib, json, os, re, subprocess, sys, time

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(TOOLS, "..", ".."))
SESSION = os.path.join(ROOT, "Saved", "AnimPipeSession")
JOBS = os.path.join(SESSION, "jobs")
DATA = os.path.join(ROOT, "Saved", "AnimPipeline")


def log(path, rec):
    with open(path, "a", encoding="utf-8") as f:
        f.write(json.dumps(dict(rec, t=round(time.time(), 3), pid=os.getpid())) + "\n")


def files_of(patterns, cwd):
    out = []
    for p in patterns:
        full = p if os.path.isabs(p) else os.path.join(cwd, p)
        hits = sorted(glob.glob(full, recursive=True))
        out += hits if hits else [full]          # a missing input still enters the stamp (as missing)
    return out


def stamp_of(step, cwd):
    h = hashlib.sha256(json.dumps({k: v for k, v in step.items() if k not in ("timeout_min",)}, sort_keys=True).encode())
    for p in files_of(step.get("inputs", []), cwd):
        h.update(p.replace("\\", "/").encode())
        if os.path.isfile(p):
            with open(p, "rb") as f:
                for chunk in iter(lambda: f.read(1 << 20), b""):
                    h.update(chunk)
        else:
            h.update(b"<missing>")
    return h.hexdigest()


def can_skip(step, cwd, stamp_dir, force):
    if step["id"] in force or step.get("always") or not step.get("inputs"):
        return False, None
    st = stamp_of(step, cwd)
    p = os.path.join(stamp_dir, step["id"] + ".stamp")
    try:
        old = open(p, encoding="utf-8").read().strip()
    except OSError:
        old = None
    outs = files_of(step.get("outputs", []), cwd)
    return (old == st and all(os.path.exists(o) for o in outs)), st


# --- the editor session ---
def submit(spec, jid=None):
    os.makedirs(JOBS, exist_ok=True)
    jid = jid or f"{time.strftime('%Y%m%d_%H%M%S')}_{time.time_ns() % 10**6:06d}"
    p = os.path.join(JOBS, jid + ".json")
    with open(p + ".tmp", "w", encoding="utf-8") as f:
        json.dump(spec, f)
    os.replace(p + ".tmp", p)
    return os.path.join(JOBS, jid + ".done.json")


def wait_done(done, timeout_s):
    end = time.time() + timeout_s
    while time.time() < end:
        if os.path.exists(done):
            try:
                return json.load(open(done, encoding="utf-8"))
            except ValueError:
                pass
        time.sleep(0.5)
    return None


def session_alive(timeout_s=20):
    """A live session answers a ping job (ready.txt alone can be left by a crashed editor)."""
    if not os.path.exists(os.path.join(SESSION, "ready.txt")):
        return False
    done = submit({"script": "Tools/AnimPipeline/animpipe/session_ping.py", "entry": "ping"}, jid="ping_" + str(os.getpid()))
    r = wait_done(done, timeout_s)
    if r is None:
        for p in (done[:-10] + ".json",):           # nobody took it: remove it, a later session must not run a stale ping
            if os.path.exists(p):
                os.remove(p)
        return False
    os.remove(done)
    return bool(r.get("ok"))


def boot_session(max_minutes, logf):
    """Start editor_session.py under Run-Timed (detached: it outlives this runner when --keep, still bounded by -MaxMinutes)."""
    for p in ("ready.txt", "quit.txt"):
        if os.path.exists(os.path.join(SESSION, p)):
            os.remove(os.path.join(SESSION, p))
    os.makedirs(os.path.join(ROOT, "Saved", "Logs", "ci"), exist_ok=True)
    args = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(ROOT, "Tools", "CI", "Run-Timed.ps1"),
            "-Preset", "Capture", "-Script", r"Tools\AnimPipeline\editor_session.py", "-Log", logf,
            "-MaxMinutes", str(max_minutes), "-SilentMinutes", "30"]
    # CREATE_NO_WINDOW: under DETACHED_PROCESS powershell does nothing and exits 0 (found by C5 2026-11-24)
    flags = (getattr(subprocess, "CREATE_NO_WINDOW", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)) if os.name == "nt" else 0
    return subprocess.Popen(args, cwd=ROOT, creationflags=flags, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def wait_ready(t0, timeout_s=900):
    p = os.path.join(SESSION, "ready.txt")
    while time.time() - t0 < timeout_s:
        if os.path.exists(p) and os.path.getmtime(p) >= t0 - 1:
            return True
        time.sleep(1)
    return False


def quit_session():
    os.makedirs(SESSION, exist_ok=True)
    open(os.path.join(SESSION, "quit.txt"), "w").write("quit\n")


def run_cmd(step, cwd, env, timeout_s):
    cmd = [sys.executable if c == "python" else c for c in step["cmd"]]
    out = os.path.join(DATA, "round_runner", "logs")
    os.makedirs(out, exist_ok=True)
    lp = os.path.join(out, f"{step['id']}.txt")
    with open(lp, "w", encoding="utf-8") as f:
        try:
            p = subprocess.run(cmd, cwd=cwd, env=env, stdout=f, stderr=subprocess.STDOUT, timeout=timeout_s)
            return p.returncode, lp
        except subprocess.TimeoutExpired:
            return 124, lp


def main(argv):
    if not argv or argv[0].startswith("--"):
        print(__doc__.splitlines()[2]); return 2
    spec = json.load(open(argv[0], encoding="utf-8"))
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in argv[1:] if a.startswith("--"))
    force = set(filter(None, opt.get("force", "").split(",")))
    name = spec["name"]
    cwd = os.path.join(ROOT, spec.get("cwd", "Tools/AnimPipeline"))
    stamp_dir = os.path.join(DATA, "round_runner", name)
    os.makedirs(stamp_dir, exist_ok=True)
    tdir = os.path.join(DATA, "timing"); os.makedirs(tdir, exist_ok=True)
    tlog = os.path.join(tdir, f"{name}_{time.strftime('%Y%m%d_%H%M%S')}.jsonl")
    env = dict(os.environ, ANIMPIPE_TIMING=tlog, MSYS_NO_PATHCONV="1", PYTHONIOENCODING="utf-8")
    if "no-cache" not in opt:
        env["ANIMPIPE_SAMPLE_CACHE"] = os.path.join(DATA, "sample_cache")
    t_round = time.time()
    log(tlog, {"phase": "round", "event": "start", "name": name, "cache": "no-cache" not in opt})

    plan = []
    for s in spec["steps"]:
        skip, st = can_skip(s, cwd, stamp_dir, force)
        plan.append((s, skip, st))
    if "dry" in opt:
        for s, skip, _ in plan:
            print(f"{s['id']}: {'skip (same inputs)' if skip else 'run'}{' [editor]' if s.get('editor') or 'job' in s else ''}")
        return 0

    need_editor = any((s.get("editor") or "job" in s) and not skip for s, skip, _ in plan)
    sess = {"state": "none", "t0": None}
    smax = int(spec.get("session", {}).get("max_minutes", 120))
    slog = os.path.join("Saved", "Logs", "ci", f"animpipe_session_{name}.log")
    if need_editor:
        t = time.time()
        if session_alive():
            sess.update(state="reused", t0=t)
            log(tlog, {"phase": "editor_session", "event": "reused", "sec": round(time.time() - t, 2)})
        else:
            boot_session(smax, slog)
            sess.update(state="booting", t0=t)
            log(tlog, {"phase": "editor_session", "event": "boot_started", "log": slog})

    def ensure_ready():
        if sess["state"] != "booting":
            return sess["state"] == "reused" or sess["state"] == "ready"
        ok = wait_ready(sess["t0"])
        log(tlog, {"phase": "editor_boot", "sec": round(time.time() - sess["t0"], 2), "ok": ok, "log": slog})
        sess["state"] = "ready" if ok else "failed"
        return ok

    lines, code = [], 0
    for s, skip, st in plan:
        sid = s["id"]
        if skip:
            log(tlog, {"phase": sid, "skipped": True}); lines.append(f"{sid} skip"); continue
        is_editor = s.get("editor") or "job" in s
        if is_editor and not ensure_ready():
            code = 4; lines.append(f"{sid}: no editor (session did not get ready, see {slog})"); break
        tout = float(s.get("timeout_min", 15)) * 60
        t = time.time()
        if "job" in s:
            j = dict(s["job"])
            if "assert" in s and os.path.exists(os.path.join(ROOT, s["assert"])):
                os.remove(os.path.join(ROOT, s["assert"]))
            r = wait_done(submit(j), tout)
            rc = 124 if r is None else (0 if r.get("ok") else 1)
            detail = "TIMEOUT" if r is None else str(r.get("result"))[:120]
            if rc == 0 and "assert" in s:
                ap = os.path.join(ROOT, s["assert"])
                first = open(ap, encoding="utf-8").readline().strip() if os.path.exists(ap) else "ASSERT: MANCANTE"
                rc = 0 if first.startswith("ASSERT: OK") else 3
                detail = first
        else:
            rc, detail = run_cmd(s, cwd, env, tout)
            if rc == 0 and s.get("ok_pattern"):         # legacy commands print their failure and still exit 0: never stamp those
                txt = open(detail, encoding="utf-8", errors="replace").read()
                if not re.search(s["ok_pattern"], txt):
                    rc, detail = 1, f"{detail}: no line matches ok_pattern {s['ok_pattern']!r}"
        sec = round(time.time() - t, 2)
        log(tlog, {"phase": sid, "sec": sec, "exit": rc, "editor": bool(is_editor), "detail": str(detail)[:200]})
        lines.append(f"{sid} {sec:.0f}s rc{rc}")
        if rc == 0 and st:
            open(os.path.join(stamp_dir, sid + ".stamp"), "w", encoding="utf-8").write(st + "\n")
        if rc != 0 and not s.get("may_fail"):
            code = rc
            if rc == 124:
                quit_session()
            lines.append(f"STOP at {sid}: {detail}")
            break

    keep = "keep" in opt and code == 0          # a failed round never leaves an editor behind (rule 15g)
    if sess["state"] != "none" and not keep:
        quit_session()
    total = round(time.time() - t_round, 1)
    log(tlog, {"phase": "round", "event": "end", "sec": total, "exit": code, "session": sess["state"], "kept": keep})
    esito = {0: "OK", 124: "TIMEOUT", 3: "ASSERT", 4: "NO EDITOR"}.get(code, "FAIL")
    print(f"ESITO: {esito} round {name} in {total:.0f} s, editor {sess['state']}{' (kept)' if keep else ''}")
    print(" | ".join(lines)[:400])
    print(f"timing: {tlog}")
    print(f"report: python import_timing_report.py {tlog} --ue-log={slog}")
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

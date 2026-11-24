"""One editor per round (cleanup plan fix 3, C5 2026-11-24): the Play capture runs as a "boot" job of the live editor_session.py
instead of a fresh editor per capture (pie_capture_timed.ps1 -> Run-Timed -> new UnrealEditor, ~60 s of boot + map load each time).

    import session_capture as SC
    sess = SC.ensure_session(max_minutes=60)          # reuses a live session (ping) or boots one on the owner's 2nd monitor
    runner = SC.session_runner()                      # drop-in `runner` for capture_move.anim_capture_move
    ... anim_capture_move(..., runner=runner) ...     # captures go to the session; videos/ffmpeg still run as subprocesses
    SC.quit_session()                                 # or keep it for the next round

The capture job is pie_capture_move.py itself (what pie_capture_boot_vd2.py execs), with the env pie_capture_timed.ps1 would set
(PIE_CAPTURE_KIT/OUT/BUTTON/ASSERT + the PIE_* keys of the call). editor_session runs it against a proxy `unreal`: QUIT_EDITOR ends the
job, its tick callbacks are removed, PIE is stopped, the env is restored. The verdict is the same file Run-Timed reads: <out>/assert.txt
(first line ASSERT: OK -> 0, other -> 3, missing -> 1), plus <out>/done.txt. Limit per capture: CAPTURE_TIMEOUT_S (15 min, as Run-Timed).
Same frames as a fresh editor? Checked once per chain with capdet `--verify` on a base captured both ways (handover c5-sessione.md)."""
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import round_runner as RR  # noqa: E402  (submit / wait_done / session_alive / quit_session: one job protocol)

CAPTURE_PY = "Tools/GASNPPTest/pie_capture_move.py"
CAPTURE_TIMEOUT_S = 15 * 60
TIMED_PS1 = "pie_capture_timed.ps1"


def _boot_script():
    """-ExecCmds "py <path>" breaks on the space in "Unreal Projects" (pie_capture_timed.ps1): a one-line boot in %TEMP% that puts the
    editor on the owner's second monitor (vd2_mover, WINDOW_PLACE=monitor2 default) and then becomes the job server."""
    boot = os.path.join(os.environ.get("TEMP", ROOT), "gstn_session_boot.py")
    vd2 = os.path.join(ROOT, "Tools", "GASNPPTest", "vd2_mover.py").replace("\\", "/")
    ses = os.path.join(ROOT, "Tools", "AnimPipeline", "editor_session.py").replace("\\", "/")
    with open(boot, "w", encoding="utf-8") as f:
        f.write("import runpy\n"
                f"try:\n    runpy.run_path(r'{vd2}', run_name='vd2_mover')\nexcept Exception as e:\n    print('vd2_mover failed:', e)\n"
                f"__file__ = r'{ses}'\nexec(open(__file__).read())\n")
    return boot


def boot_session(max_minutes, logf):
    for p in ("ready.txt", "quit.txt"):
        q = os.path.join(RR.SESSION, p)
        if os.path.exists(q):
            os.remove(q)
    os.makedirs(os.path.dirname(os.path.join(ROOT, logf)), exist_ok=True)
    args = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(ROOT, "Tools", "CI", "Run-Timed.ps1"),
            "-Preset", "Capture", "-Script", _boot_script(), "-Log", logf, "-MaxMinutes", str(max_minutes), "-SilentMinutes", "30"]
    # CREATE_NO_WINDOW, not DETACHED_PROCESS: a powershell started without a console does nothing and exits 0 (C5 2026-11-24, the
    # session never booted and the runner waited its 15 min for ready.txt)
    flags = (getattr(subprocess, "CREATE_NO_WINDOW", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)) if os.name == "nt" else 0
    return subprocess.Popen(args, cwd=ROOT, creationflags=flags, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def ensure_session(max_minutes=60, logf=r"Saved\Logs\ci\animpipe_session_capture.log", ready_timeout_s=300):
    """{'state': 'reused'|'ready'|'failed', 'sec': seconds to a usable editor}."""
    t0 = time.time()
    if RR.session_alive():
        return {"state": "reused", "sec": round(time.time() - t0, 1)}
    boot_session(max_minutes, logf)
    ok = RR.wait_ready(t0, ready_timeout_s)
    return {"state": "ready" if ok else "failed", "sec": round(time.time() - t0, 1), "log": logf}


def quit_session():
    RR.quit_session()


def _arg(cmd, name):
    return cmd[cmd.index(name) + 1] if name in cmd else None


def capture_job(kit, out, assert_file=None, env=None, button="Attack", timeout_s=CAPTURE_TIMEOUT_S):
    """One Play capture in the live session. Returns {'returncode', 'stdout', 'sec'} like the pwsh runner's result."""
    out_abs = out if os.path.isabs(out) else os.path.join(ROOT, out)
    os.makedirs(out_abs, exist_ok=True)
    for f in ("done.txt", "assert.txt"):
        if os.path.exists(os.path.join(out_abs, f)):
            os.remove(os.path.join(out_abs, f))
    jenv = {k: v for k, v in (env or {}).items() if k.startswith(("PIE_", "GSTN_"))}
    jenv.update({"PIE_CAPTURE_KIT": kit, "PIE_CAPTURE_OUT": out_abs.replace("\\", "/"), "PIE_CAPTURE_BUTTON": button})
    if assert_file:
        jenv["PIE_CAPTURE_ASSERT"] = os.path.abspath(os.path.join(ROOT, assert_file))
    t0 = time.time()
    r = RR.wait_done(RR.submit({"script": CAPTURE_PY, "mode": "boot", "env": jenv}), timeout_s)
    sec = round(time.time() - t0, 1)
    if r is None:
        return {"returncode": 124, "stdout": f"ESITO: TIMEOUT capture job after {sec} s (session busy or stuck)", "sec": sec}
    a = os.path.join(out_abs, "assert.txt")
    first = open(a, encoding="utf-8").readline().strip() if os.path.exists(a) else "assert.txt missing"
    d = os.path.join(out_abs, "done.txt")
    done = open(d, encoding="utf-8").readline().strip() if os.path.exists(d) else "done.txt missing"
    rc = 0 if (r.get("ok") and first.startswith("ASSERT: OK")) else (3 if first.startswith("ASSERT") else 1)
    return {"returncode": rc, "sec": sec, "stdout": f"{first} | done: {done} | job ok {r.get('ok')} {str(r.get('result'))[:300]}"}


def session_runner(fallback=None):
    """A capture_move runner: the pie_capture_timed.ps1 command becomes a session job, anything else (videos) runs as before."""
    def run(cmd, env, cwd):
        if any(str(c).endswith(TIMED_PS1) for c in cmd):
            return capture_job(_arg(cmd, "-Kit"), _arg(cmd, "-Out"), _arg(cmd, "-Assert"), env, _arg(cmd, "-Button") or "Attack")
        if fallback:
            return fallback(cmd, env, cwd)
        return subprocess.run(cmd, env=env, cwd=cwd, capture_output=True, text=True)
    return run

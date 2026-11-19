# PIE-MP-PERF (owner 2026-11-19: listen server + 3 players runs at 15 fps with warning spam). Boot for Run-Timed -Preset Capture:
# moves the editor to virtual desktop 2, starts PIE on the startup map (L_Test_Hub) as listen server with PIE_PERF_PLAYERS players
# (default 3, host included), Run Under One Process = PIE_PERF_ONE_PROCESS (1/0), warms up 10 s, then for PIE_PERF_SECONDS (30):
# Unreal Insights trace (cpu,frame,log,bookmark) + csvprofile, stat unit/game on screen. Writes <PIE_PERF_OUT>/result.txt (frame
# times from the engine's average delta, warning lines counted from the log), the .utrace and the csv paths, then quits the editor.
import os, sys, time, runpy, glob
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(_here, "vd2_mover.py"), run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)

OUT = os.environ.get("PIE_PERF_OUT") or os.path.join(unreal.Paths.project_saved_dir(), "PiePerf")
PLAYERS = int(os.environ.get("PIE_PERF_PLAYERS", "3"))
ONE = os.environ.get("PIE_PERF_ONE_PROCESS", "1") == "1"
SECS = float(os.environ.get("PIE_PERF_SECONDS", "30"))
os.makedirs(OUT, exist_ok=True)
log = []
def p(*a):
    log.append(" ".join(str(x) for x in a)); open(os.path.join(OUT, "result.txt"), "w").write("\n".join(log))

s = unreal.get_default_object(unreal.LevelEditorPlaySettings)
for k, v in (("play_net_mode", unreal.PlayNetMode.PIE_LISTEN_SERVER), ("play_number_of_clients", PLAYERS),
             ("play_net_dedicated", False), ("run_under_one_process", ONE)):
    try:
        s.set_editor_property(k, v); p("set", k, v)
    except Exception as e:
        p("FAILED set", k, e)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
st = {"t0": time.time(), "phase": "boot", "frames": 0, "dt": 0.0, "dts": []}
tag = "one" if ONE else "multi"

def cmd(c):
    w = None
    for wc in unreal.EditorLevelLibrary.get_pie_worlds(False) if hasattr(unreal.EditorLevelLibrary, "get_pie_worlds") else []:
        w = wc; break
    unreal.SystemLibrary.execute_console_command(w, c)

def tick(dt):
    t = time.time() - st["t0"]
    if st["phase"] == "boot" and t > 8:
        les.editor_request_begin_play(); st["phase"] = "warm"; st["tw"] = time.time(); p("PIE requested")
    elif st["phase"] == "warm" and time.time() - st["tw"] > 12:
        for c in ("stat unit", "stat game", f"trace.file {OUT}/pie_{tag}.utrace cpu,frame,log,bookmark", f"csvprofile start"):
            cmd(c)
        st["phase"] = "measure"; st["tm"] = time.time(); p("measure start, PIE worlds:",
            len(unreal.EditorLevelLibrary.get_pie_worlds(False)) if hasattr(unreal.EditorLevelLibrary, "get_pie_worlds") else "?")
    elif st["phase"] == "measure":
        st["dts"].append(dt)
        if time.time() - st["tm"] > SECS:
            for c in ("trace.stop", "csvprofile stop"):
                cmd(c)
            d = sorted(st["dts"]); n = len(d)
            p("frames", n, "avg ms %.1f" % (1000 * sum(d) / max(n, 1)), "p50 ms %.1f" % (1000 * d[n // 2]), "p95 ms %.1f" % (1000 * d[int(n * 0.95)]),
              "fps %.1f" % (n / max(sum(d), 1e-6)))
            st["phase"] = "stop"; st["ts"] = time.time()
    elif st["phase"] == "stop" and time.time() - st["ts"] > 4:
        les.editor_request_end_play()
        logf = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir())
        csvs = sorted(glob.glob(os.path.join(unreal.Paths.project_saved_dir(), "Profiling", "CSV", "*.csv")), key=os.path.getmtime)[-1:]
        p("csv", csvs)
        st["phase"] = "quit"; st["tq"] = time.time()
    elif st["phase"] == "quit" and time.time() - st["tq"] > 3:
        open(os.path.join(OUT, "done.txt"), "w").write("ok")
        unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")

h = unreal.register_slate_post_tick_callback(tick)
p("driver loaded", "players", PLAYERS, "one process", ONE)

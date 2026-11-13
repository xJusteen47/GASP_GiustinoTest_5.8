"""Editor side of `animpipe render <move> --standalone` (runs INSIDE a separate UnrealEditor process that animpipe/standalone.py
starts; never inside the owner's editor). Reuses capture_ue.py (setup / set_frame / capture) unchanged.

The separate process loads /Game/AnimPipeline/Pilot/L_PilotCapture FROM DISK, so whatever is unsaved in the owner's editor
(DefaultLevel or anything else) is not in it and nothing it spawns is ever saved. Job file (env ANIMPIPE_JOB, json):
{anim, move, azs: [..], size, el, dist, fov, out_dir, tools_dir}. Writes <out_dir>/<az>/<frame:03d>.rgb, then manifest.json
({n, azs, size}) and <out_dir>/standalone.done ("ok" or the error), then QUIT_EDITOR. One slate tick between set_frame and
capture (the scrub lands a tick later), a short warm-up first (shader compile makes the first captures black)."""
import json, os, sys, time, traceback
import unreal

JOB = json.load(open(os.environ["ANIMPIPE_JOB"], encoding="utf-8"))
sys.path.insert(0, JOB["tools_dir"])
import capture_ue as cap

SETTLE_TICKS = 3


def _log(msg):
    unreal.log("[animpipe-standalone] " + str(msg))


def _script():
    cap.OUT = JOB["out_dir"].replace("\\", "/").rstrip("/") + "/"
    st = cap.setup()
    if st.get("error"):
        raise RuntimeError(st["error"])
    anim = unreal.load_asset(JOB["anim"])
    if anim is None:
        raise RuntimeError("anim not found: " + JOB["anim"])
    n = int(round(anim.get_play_length() * 60))
    for _ in range(30):
        yield                                   # mesh + materials load
    for w in (0, 0):                            # warm-up captures, discarded
        cap.set_frame(JOB["anim"], w)
        yield
        yield
        cap.capture("lit", JOB["azs"][0], JOB["el"], "_warm", size=JOB["size"], dist=JOB["dist"], fov=JOB["fov"])
    for az in JOB["azs"]:
        sub = os.path.join(JOB["out_dir"], str(az)); os.makedirs(sub, exist_ok=True)
        for f in range(n + 1):
            cap.set_frame(JOB["anim"], f)
            for _ in range(SETTLE_TICKS):
                yield
            cap.OUT = sub.replace("\\", "/").rstrip("/") + "/"
            cap.capture("lit", az, JOB["el"], "%03d" % f, size=JOB["size"], dist=JOB["dist"], fov=JOB["fov"])
        _log(f"az {az}: {n + 1} frames")
    json.dump({"n": n, "azs": JOB["azs"], "size": JOB["size"], "move": JOB["move"]}, open(os.path.join(JOB["out_dir"], "manifest.json"), "w"))


class _Run:
    def __init__(self):
        self.gen, self.err = _script(), "ok"
        self.h = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, _dt):
        try:
            next(self.gen)
        except StopIteration:
            self.finish()
        except Exception:
            self.err = traceback.format_exc()
            _log("ERROR " + self.err)
            self.finish()

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.h)
        open(os.path.join(JOB["out_dir"], "standalone.done"), "w").write(self.err)
        unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")


_RUN = _Run()

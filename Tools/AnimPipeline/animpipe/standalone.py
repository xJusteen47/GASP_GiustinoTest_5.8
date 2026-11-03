"""`animpipe render <move> --standalone`: the in-game video WITHOUT the owner's editor.

Starts a second UnrealEditor process (rendering, off screen) on the SAVED map L_PilotCapture, runs standalone_capture.py in it
(capture_ue.py's setup/set_frame/capture, the same camera as the editor path: el 12, 450 cm, FOV 55), quits it, converts the
frames to PNG and runs ingame_judge.py --cached on them: same files, same format as `review` (hitstop | whiff | compare mp4 +
strip), one set per azimuth (name suffix _az<N>). The owner's running editor, his unsaved DefaultLevel and UnrealMCP are not touched.

Limits: needs the built clip saved on disk (build first) and L_PilotCapture saved (Content/AnimPipeline/Pilot/L_PilotCapture.umap);
a project that the owner has open in the editor can run a second editor on the same project on Windows (separate process, own
log), but not on a machine where the engine files are locked by a build; needs a GPU/RHI (not -nullrhi: scene capture renders);
the first run compiles shaders (minutes). Settings: engine_exe in animpipe.local.toml or env UE_EDITOR."""
import json, os, shutil, subprocess, sys, time

from animpipe import config as K

TOOLS = K.TOOLS
LEVEL_FILE = os.path.join("Content", "AnimPipeline", "Pilot", "L_PilotCapture.umap")
MAP = "/Game/AnimPipeline/Pilot/L_PilotCapture"
DEFAULT_ENGINE = r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
SIZE, EL, DIST, FOV = 384, 12, 450, 55            # = ingame_judge.py


class Unavailable(RuntimeError):
    """exit 4 (needs an app): engine, project or saved map not found here."""


def project_dir():
    # a git worktree of the tools has no content: ANIMPIPE_PROJECT names the project that holds the clips and the map
    if os.environ.get("ANIMPIPE_PROJECT"):
        return os.environ["ANIMPIPE_PROJECT"]
    d = TOOLS
    for _ in range(5):
        d = os.path.dirname(d)
        if any(f.endswith(".uproject") for f in os.listdir(d)):
            return d
    raise Unavailable("no .uproject above Tools/AnimPipeline")


def engine_exe():
    e = os.environ.get("UE_EDITOR") or K.CFG.get("engine_exe") or DEFAULT_ENGINE
    if not os.path.exists(e):
        raise Unavailable(f"UnrealEditor not found at {e} (set engine_exe in animpipe.local.toml or env UE_EDITOR)")
    return e


def build_args(exe, uproject, log):
    """The command line (a function so a test can read it). -ExecCmds reads the boot script path from the environment: the
    project path has a space and -ExecCmds stops at an inner quote (same trick as the test harness)."""
    return [exe, uproject, MAP, "-ExecCmds=py import os;exec(open(os.environ['ANIMPIPE_BOOT']).read())", "-unattended", "-nosplash",
            "-nosound", "-NoP4", "-RenderOffScreen", f"-ResX={SIZE}", f"-ResY={SIZE}", f"-abslog={log}"]


def convert(out_dir, azs, dest_for_az):
    """<out_dir>/<az>/<f>.rgb -> dest_for_az(az)/<f>.png. Returns frames per az."""
    from PIL import Image
    n = json.load(open(os.path.join(out_dir, "manifest.json")))["n"]
    for az in azs:
        d = dest_for_az(az); os.makedirs(d, exist_ok=True)
        for f in range(n + 1):
            raw = open(os.path.join(out_dir, str(az), "%03d.rgb" % f), "rb").read()
            Image.frombytes("RGB", (SIZE, SIZE), raw).save(os.path.join(d, "%03d.png" % f))
    return n + 1


def run(card, card_path, azs, hit, at=None, timeout=1500):
    """Capture + judge for each azimuth. Returns {frames, files: [...], log}. Raises Unavailable / RuntimeError."""
    pd = project_dir()
    if not os.path.exists(os.path.join(pd, LEVEL_FILE)):
        raise Unavailable(f"{LEVEL_FILE} is not saved on disk here (open it once in the editor and save: the capture level is created by capture_ue.setup)")
    exe = engine_exe()
    uproject = next(os.path.join(pd, f) for f in os.listdir(pd) if f.endswith(".uproject"))
    move = card["move"]
    out_dir = os.path.join(K.DATA, "standalone", move)
    shutil.rmtree(out_dir, ignore_errors=True); os.makedirs(out_dir)
    log = os.path.join(out_dir, "editor.log")
    job = {"anim": card["anim"], "move": move, "azs": azs, "size": SIZE, "el": EL, "dist": DIST, "fov": FOV,
           "out_dir": out_dir.replace("\\", "/"), "tools_dir": TOOLS.replace("\\", "/")}
    jp = os.path.join(out_dir, "job.json"); json.dump(job, open(jp, "w"))
    env = dict(os.environ, ANIMPIPE_JOB=jp.replace("\\", "/"), ANIMPIPE_BOOT=os.path.join(TOOLS, "standalone_capture.py").replace("\\", "/"))
    proc = subprocess.Popen(build_args(exe, uproject, log), env=env)
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        raise RuntimeError(f"standalone editor timed out after {timeout} s (log {log})")
    done = os.path.join(out_dir, "standalone.done")
    if not os.path.exists(done) or open(done).read().strip() != "ok":
        raise RuntimeError("standalone capture failed: " + (open(done).read()[-400:] if os.path.exists(done) else f"no .done file, see {log}"))
    ing = os.path.join(TOOLS, "sheets", "ingame")
    files = []
    for az in azs:
        raw = os.path.join(ing, f"_{move}_az{az}", "raw")
        shutil.rmtree(raw, ignore_errors=True)
        convert(out_dir, [az], lambda a: raw)
        cmd = [sys.executable, "ingame_judge.py", card_path, str(hit), str(az), "--cached", f"--raw={raw}", f"--tag=_az{az}"]
        if at is not None:
            cmd.append(f"--at={at}")
        p = subprocess.run(cmd, cwd=TOOLS, capture_output=True, text=True, encoding="utf-8", errors="replace",
                           env=dict(os.environ, PYTHONIOENCODING="utf-8"))
        if p.returncode:
            raise RuntimeError("ingame_judge failed: " + (p.stderr or p.stdout)[-300:])
        files += [l.strip() for l in p.stdout.splitlines() if l.strip().endswith((".mp4", ".png"))]
    return {"azs": azs, "files": files, "log": log}

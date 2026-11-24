"""K8 round, editor part, measured two ways (C5 2026-11-24, cleanup fix 3). Not a review round: it produces no review pack and promotes
nothing (the K8 r2 clip fails no_stop and has no animpipe check on the desktop, so capture_move's gates rightly refuse it); it times the
editor work of a round and checks that a capture in the live session gives the same frames as one in a fresh editor.

  python Tools/AnimPipeline/round_bench_k8.py before   # as today: session for the editor steps, quit, then pie_capture_timed (new editor), K7+K8
  python Tools/AnimPipeline/round_bench_k8.py after    # one session: editor steps + capture job; K7 base from the cache when its key is unchanged
  python Tools/AnimPipeline/round_bench_k8.py compare  # K8 frames.jsonl before vs after (pose and position per frame)

Editor steps = joint dumps of the K7 and K8 game clips (clip_joints_dump_ue.py), the step every round runs before its gates.
Capture = dropkick_sweet_whiff + dropkick_sweet_hit, views az90,az180,game, fast engine (PIE_FASTCAP, window press-20..+90), env of
capture_k8_fast.py. Cache key = capture_move._run_key (clip uasset sha, sequences, cvars, views, C++ source hash, chain, kit).
Results: Saved/C5Bench/<mode>.json; prints at most 5 lines."""
import json
import math
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "gstn_anim_mcp"))
import round_runner as RR  # noqa: E402
import session_capture as SC  # noqa: E402
import capture_move as CM  # noqa: E402
import manifest  # noqa: E402
from pathlib import Path  # noqa: E402

OUT = os.path.join(ROOT, "Saved", "C5Bench")
D = "/Game/GASNPPTest/Combat/Overlord"
SEQ = ["dropkick_sweet_whiff", "dropkick_sweet_hit"]
VIEWS = "az90,az180,game"
WIN = CM.FAST_WINDOW
ENV = {"PIE_CAPTURE_AIR_GIVE_UP": "200", "PIE_CAPTURE_DROPKICK_LOW_DIST": "190", "PIE_CAPTURE_DIST": "350",
       "PIE_CAPTURE_DROPKICK_SWEET_H": "480", "PIE_CAPTURE_VIEWS": VIEWS, "PIE_FASTCAP": "1", "PIE_FASTCAP_WINDOW": f"{WIN[0]},{WIN[1]}"}


def _run(name, kit, clip, am):
    return {"name": name, "kit": kit, "asset": f"{D}/{clip}", "clips": [f"{D}/{clip}"], "cvars": "gstn.Combat.Debug.Hitboxes 0",
            "sequences": SEQ, "assert": {s: [{"clip": am, "window": [143, 150], "min_weight": 0.8, "coverage": 0.9}] for s in SEQ}}


K7 = _run("k7", "KIT_Test_PlaytestTBK7", "AS_OverlordDropkick_StartupK7", "AM_OverlordDropkickK7")
K8 = _run("k8", "KIT_Test_PlaytestTBK8", "AS_OverlordDropkick_StartupK8", "AM_OverlordDropkickK8")
DUMP = {"script": "Tools/AnimPipeline/cards/overlord/dropkick/k8/ue_step.py", "entry": "run",
        "kwargs": {"script": "Tools/AnimPipeline/gstn_anim_mcp/clip_joints_dump_ue.py",
                   "env": {"CJD_ASSETS": f"{D}/AS_OverlordDropkick_StartupK7,{D}/AS_OverlordDropkick_StartupK8", "CJD_FACING": "y",
                           "CJD_RANGE": ""}}}


def key_of(run):
    root = Path(ROOT)
    return CM._run_key(root, run, SEQ, VIEWS, manifest.cpp_source_hash(root)[:16], False, f"{CM.CAPTURE_CHAIN}:{WIN[0]},{WIN[1]}")


def cached(run):
    return CM._cached_dir(Path(ROOT), run, key_of(run), SEQ)


def remember(run, raw_rel):
    root = Path(ROOT)
    m = manifest.load(root, run["asset"])
    m["captures"][key_of(run)] = {"dir": f"{raw_rel}/{run['name']}", "run": run["name"], "sequences": SEQ, "views": VIEWS,
                                  "t": time.strftime("%Y-%m-%dT%H:%M:%S"), "by": "round_bench_k8"}
    manifest.save(root, run["asset"], m)


def runs_file(raw_rel, runs):
    d = os.path.join(ROOT, raw_rel)
    os.makedirs(d, exist_ok=True)
    rows = [{k: r[k] for k in ("name", "kit", "cvars", "sequences", "assert")} for r in runs]
    json.dump(rows, open(os.path.join(d, "runs.json"), "w", encoding="utf-8"), indent=1)
    json.dump(runs[0]["assert"], open(os.path.join(d, "assert_spec.json"), "w", encoding="utf-8"), indent=1)
    return os.path.join(d, "runs.json"), os.path.join(raw_rel, "assert_spec.json")


def editor_steps(t):
    t0 = time.time()
    r = RR.wait_done(RR.submit(DUMP), 600)
    t["dump_s"] = round(time.time() - t0, 1)
    t["dump_ok"] = bool(r and r.get("ok"))


def before():
    t, t0 = {"mode": "before"}, time.time()
    s = SC.ensure_session(max_minutes=40, logf=r"Saved\Logs\ci\c5_bench_before_session.log")
    t["session"] = s
    if s["state"] != "failed":
        editor_steps(t)
    SC.quit_session()
    end = time.time() + 180                       # today's flow: the import editor is gone before the capture editor starts
    while time.time() < end and "UnrealEditor.exe" in subprocess.run(["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe"],
                                                                      capture_output=True, text=True).stdout:
        time.sleep(1)
    t["editor_steps_total_s"] = round(time.time() - t0, 1)
    raw = "Saved/C5Bench/before"
    rf, af = runs_file(raw, [K7, K8])
    env = dict(os.environ, **ENV, PIE_CAPTURE_RUNS=rf)
    c0 = time.time()
    p = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                        os.path.join(ROOT, "Tools", "GASNPPTest", "pie_capture_timed.ps1"), "-Kit", K8["kit"], "-Out", raw, "-Assert", af],
                       env=env, cwd=ROOT, capture_output=True, text=True)
    t["capture_s"] = round(time.time() - c0, 1)
    t["capture_rc"] = p.returncode
    t["capture_out"] = (p.stdout or "")[-400:]
    if p.returncode == 0:
        remember(K7, raw)        # the base enters the cache only from a run that passed (capdet-2 rule)
    t["total_s"] = round(time.time() - t0, 1)
    return t


def after():
    t, t0 = {"mode": "after"}, time.time()
    s = SC.ensure_session(max_minutes=40, logf=r"Saved\Logs\ci\c5_bench_after_session.log")
    t["session"] = s
    if s["state"] == "failed":
        t["error"] = "no session"
        return t
    editor_steps(t)
    hit = cached(K7)
    t["k7_cache"] = str(hit) if hit else "MISS"
    runs = [K8] if hit else [K7, K8]
    raw = "Saved/C5Bench/after"
    rf, af = runs_file(raw, runs)
    c0 = time.time()
    r = SC.capture_job(K8["kit"], raw, af, dict(ENV, PIE_CAPTURE_RUNS=rf))
    t["capture_s"] = round(time.time() - c0, 1)
    t["capture_rc"], t["capture_out"] = r["returncode"], r["stdout"][-400:]
    t["captured_runs"] = [x["name"] for x in runs]
    if "--keep" not in sys.argv:
        SC.quit_session()
    t["total_s"] = round(time.time() - t0, 1)
    return t


def compare():
    out = {}
    for seq in SEQ:
        a = [json.loads(x) for x in open(os.path.join(OUT, "before", "k8", seq, "frames.jsonl"), encoding="utf-8") if x.strip()]
        b = [json.loads(x) for x in open(os.path.join(OUT, "after", "k8", seq, "frames.jsonl"), encoding="utf-8") if x.strip()]
        bf = {r["f"]: r for r in b}
        worst, n, where = 0.0, 0, None
        for r in a:
            s = bf.get(r["f"])
            if not s or "body" not in r or "body" not in s:
                continue
            n += 1
            for k in ("actor", "pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r"):
                if k in r["body"] and k in s["body"]:
                    d = math.dist(r["body"][k], s["body"][k])
                    if d > worst:
                        worst, where = d, f"f{r['f']} {k}"
        out[seq] = {"frames_compared": n, "frames_before": len(a), "frames_after": len(b), "worst_cm": round(worst, 3), "where": where}
    return out


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "after"
    os.makedirs(OUT, exist_ok=True)
    res = {"before": before, "after": after, "compare": compare}[mode]()
    json.dump(res, open(os.path.join(OUT, mode + ".json"), "w", encoding="utf-8"), indent=1, default=str)
    print(json.dumps(res, default=str)[:900])

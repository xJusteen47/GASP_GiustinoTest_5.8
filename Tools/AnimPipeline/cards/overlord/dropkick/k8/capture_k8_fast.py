"""Dropkick K8 through the FAST capture (owner 2026-11-24, 1C + 2A): the template for every next round of a move.
  1. offline preview first (seconds, no editor; PROTO, only to scrap a wrong round):
     python Tools/AnimPipeline/quick_preview.py Tools/AnimPipeline/cards/overlord/dropkick/overlord_dropkick_startup_k8.json \
            --base=Tools/AnimPipeline/cards/overlord/dropkick/overlord_dropkick_startup_k7.json
  2. this script (default = dry run: prints the plan; --real captures; --final adds the owner's hitbox run):
     python Tools/AnimPipeline/cards/overlord/dropkick/k8/capture_k8_fast.py [--real] [--final]
K7 (approved, locked) is the base: captured once with this chain, then reused from the cache every round (only K8 is captured).
Fast engine (PIE_FASTCAP=1): async readback, only press-20 .. press+90 (+ hitstop) on disk, simple light on az90/az180, early end,
cameras follow the mesh. hitstop_pairs: whiff and hit side by side, the hit video extended by its hitstop (the snap is visible).
Same env as capture_k8.py (air give-up 200, sweet press height 480, low dist 190, camera distance 350). Prints at most 5 lines."""
import json, os, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), *[".."] * 6))
sys.path.insert(0, os.path.join(REPO, "Tools", "AnimPipeline", "gstn_anim_mcp"))
import capture_move as CM  # noqa: E402

os.environ.update({"PIE_CAPTURE_AIR_GIVE_UP": "200", "PIE_CAPTURE_DROPKICK_LOW_DIST": "190", "PIE_CAPTURE_DIST": "350",
                   "PIE_CAPTURE_DROPKICK_SWEET_H": "480"})
SEQ = ["dropkick_sweet_whiff", "dropkick_sweet_hit"]
ASSERT = lambda clip: {s: [{"clip": clip, "window": [143, 150], "min_weight": 0.8, "coverage": 0.9}] for s in SEQ}
D = "/Game/GASNPPTest/Combat/Overlord"
CARD = "Tools/AnimPipeline/cards/overlord/dropkick/overlord_dropkick_startup_k8.json"
runs = [
    {"name": "k7", "asset": f"{D}/AS_OverlordDropkick_StartupK7", "clips": [f"{D}/AS_OverlordDropkick_StartupK7"],
     "kit": "KIT_Test_PlaytestTBK7", "cvars": "gstn.Combat.Debug.Hitboxes 0", "assert": ASSERT("AM_OverlordDropkickK7")},
    {"name": "k8", "asset": f"{D}/AS_OverlordDropkick_StartupK8", "clips": [f"{D}/AS_OverlordDropkick_StartupK8"],
     "kit": "KIT_Test_PlaytestTBK8", "cvars": "gstn.Combat.Debug.Hitboxes 0", "assert": ASSERT("AM_OverlordDropkickK8"), "card": CARD},
    {"name": "k8_hitbox", "asset": f"{D}/AS_OverlordDropkick_StartupK8", "clips": [f"{D}/AS_OverlordDropkick_StartupK8"],
     "kit": "KIT_Test_PlaytestTBK8", "cvars": "gstn.Combat.Debug.Hitboxes 1", "assert": ASSERT("AM_OverlordDropkickK8"), "card": CARD,
     "hitbox_view": True},
]


def pwsh_runner(cmd, env, cwd):
    exe = cmd[0] if cmd[0].lower() not in ("pwsh", "pwsh.exe") else "powershell"
    full = dict(os.environ); full.update(env)
    return subprocess.run([exe] + cmd[1:], env=full, cwd=cwd)


real = "--real" in sys.argv
t0 = time.time()
res = CM.anim_capture_move("KIT_Test_PlaytestTBK8", SEQ, runs, views="az90,az180,game",
                           out="docs/notes/reviews/dropkick-kick-k6/k8fast", move="dropkick-kick", round="k8fast",
                           objective="dropkick-k8-fast", edit_window=[0, 12], window=[130, 200], anchor=153, faf=None,
                           dry_run=not real, root=REPO, runner=pwsh_runner if real else None,
                           raw_dir="Saved/AnimPipeline/captures/dropkick-kick-k6/k8fast", fast=True, fast_window=[20, 90],
                           final_round="--final" in sys.argv, hitstop_pairs=[SEQ])
out = os.path.join(REPO, "Saved", "AnimPipeline", "captures", "dropkick-kick-k6", "k8fast_result.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
json.dump(res, open(out, "w"), indent=1, default=str)
print(("REAL" if real else "DRY"), "ok", res.get("ok"), res.get("verdict"), res.get("stage", ""), str(res.get("error", ""))[:160])
print("captured:", (res.get("plan") or {}).get("capture_runs"), "reused:", sorted(((res.get("plan") or {}).get("reused") or {})))
print(f"TEMPO giro: {time.time() - t0:.0f} s (capture {res.get('minutes', 0)} min)")
for w in (res.get("warnings") or [])[:1]:
    print("WARN", w)
print(out)

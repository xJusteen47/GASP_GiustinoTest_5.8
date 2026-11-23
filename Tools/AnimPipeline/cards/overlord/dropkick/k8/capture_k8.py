"""Dropkick K8 capture through the anim skill: anim_capture_move, base = K7 (approved 2026-11-23), variant = K8 (card), plus a
hitbox-only run for the owner. One Play session, per-run kit, views az90 (side), az180 (behind), game.
Run from the repo root: python Tools/AnimPipeline/cards/overlord/dropkick/k8/capture_k8.py [--real]   (default = dry run)
Env as the K7 capture (k7/capture_k7.ps1): air give-up 200, sweet press height 480, low dist 190, camera distance 350."""
import json, os, subprocess, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), *[".."] * 6))
sys.path.insert(0, os.path.join(REPO, "Tools", "AnimPipeline", "gstn_anim_mcp"))
import capture_move as CM  # noqa: E402

os.environ.update({"PIE_CAPTURE_AIR_GIVE_UP": "200", "PIE_CAPTURE_DROPKICK_LOW_DIST": "190", "PIE_CAPTURE_DIST": "350",
                   "PIE_CAPTURE_DROPKICK_SWEET_H": "480"})
SEQ = ["dropkick_sweet_whiff", "dropkick_sweet_hit"]
ASSERT = lambda clip: {s: [{"clip": clip, "window": [143, 150], "min_weight": 0.8, "coverage": 0.9}] for s in SEQ}
D = "/Game/GASNPPTest/Combat/Overlord"
runs = [
    {"name": "k7", "asset": f"{D}/AS_OverlordDropkick_StartupK7", "clips": [f"{D}/AS_OverlordDropkick_StartupK7"],
     "kit": "KIT_Test_PlaytestTBK7", "cvars": "gstn.Combat.Debug.Hitboxes 0", "assert": ASSERT("AM_OverlordDropkickK7")},
    {"name": "k8", "asset": f"{D}/AS_OverlordDropkick_StartupK8", "clips": [f"{D}/AS_OverlordDropkick_StartupK8"],
     "kit": "KIT_Test_PlaytestTBK8", "cvars": "gstn.Combat.Debug.Hitboxes 0", "assert": ASSERT("AM_OverlordDropkickK8"),
     "card": "Tools/AnimPipeline/cards/overlord/dropkick/overlord_dropkick_startup_k8.json"},
    {"name": "k8_hitbox", "asset": f"{D}/AS_OverlordDropkick_StartupK8", "clips": [f"{D}/AS_OverlordDropkick_StartupK8"],
     "kit": "KIT_Test_PlaytestTBK8", "cvars": "gstn.Combat.Debug.Hitboxes 1", "assert": ASSERT("AM_OverlordDropkickK8"),
     "card": "Tools/AnimPipeline/cards/overlord/dropkick/overlord_dropkick_startup_k8.json", "hitbox_view": True},
]


def pwsh_runner(cmd, env, cwd):
    exe = cmd[0] if cmd[0].lower() not in ("pwsh", "pwsh.exe") else "powershell"
    full = dict(os.environ); full.update(env)
    return subprocess.run([exe] + cmd[1:], env=full, cwd=cwd)


real = "--real" in sys.argv
res = CM.anim_capture_move("KIT_Test_PlaytestTBK8", SEQ, runs, views="az90,az180,game",
                           out="docs/notes/reviews/dropkick-kick-k6/k8", move="dropkick-kick", round="k8",
                           objective="dropkick-k8-doom", edit_window=[0, 12], window=[130, 200], anchor=153, faf=None,
                           dry_run=not real, root=REPO, runner=pwsh_runner if real else None,
                           raw_dir="Saved/AnimPipeline/captures/dropkick-kick-k6/k8")
out = os.path.join(REPO, "Saved", "AnimPipeline", "captures", "dropkick-kick-k6", "k8_capture_result.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
json.dump(res, open(out, "w"), indent=1, default=str)
print(("REAL" if real else "DRY"), "ok", res.get("ok"), res.get("verdict"), res.get("stage", ""), str(res.get("error", ""))[:200])
print(out)

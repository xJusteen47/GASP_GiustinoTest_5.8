# Review run-stop-2 (2026-11-19, desktop, Opus pre-review) - verdict: FAIL (gioco), not for the owner. Fable review still owed.
Clip: AS_RunStop_Ganon_L_V1 / _R_V1 (/Game/Blueprints/Animation/RunStop/), pose-based (Tools/AnimPipeline/cards/runstop/run_stop_gen.py),
61 frames at 60 fps, root motion = the sim's brake (470 cm/s -> 0 in 29 frames, 125 cm incl. 3 lead frames).
Capture: KIT_Test_Overlord, gstn.RunStop.Mode 1, sequences mv_run_stop_base (run, entry 470, right foot = R clip) and mv_run_stop
(sprint, entry 756, left foot = L clip). base = run-stop-1's GASP capture (Saved/RunStop/base), same frames.
Files: base_vs_clip_az90.jpg / _az0.jpg (rows base/clip, f58-118 step 4), clip_R_az90.jpg, clip_R_az0.jpg, clip_L_sprint_az90.jpg,
run_stop2_base_vs_clip_az90.mp4 (f50-139, 60 fps).

Clip asset = authored take: sampled in Unreal (Saved/RunStop2/dump_clip.json, run_stop2_dump.py) vs run_stop_<side>_bones.json, hands,
feet and head identical to 0.1 cm at frames 2/20/31/40/60. The clip time tracks real time (cap_v2.log.ue.log `grep "RunStop mode=1 active=1"`:
t 0.083 -> 0.502 over 25 frames, one 4-frame jump to the stop at speed 10, settle to 1.017).

Identity traits (brief section 2):
1. Front foot planted, facing forward, no turn - authored YES (lunge, front ankle 42 cm ahead, skids to 31). In game it varies (below).
2. Torso rises AFTER the feet, arms open low - authored YES (trunk -32..-37 deg until frame 31, rise to +4 by 40). Capture clip_v1c
   shows it (lean f74-94, rise f98-114, arms open low in az0 f74-86); capture clip_v2 (same clip but the head) shows an upright body. NO (unstable).
3. Soft parts last - head +3 frames, arms +3/4 frames on the rise: authored YES; not readable in game yet.
Smash/AAA parity (owner's two questions): not yet - the game does not show the authored pose reliably. FAIL items:
- gioco 1: the in-game pose is not the clip. Same clip, two runs: v1c deep lean + very wide lunge (back knee near the floor), v2 upright;
  sprint (L clip, entry 756) sits into a deep crouch (pelvis far below the authored -10 cm). Suspects after the state machine:
  OffsetRootBone / StrideWarping / Procedural_Feet pinning (on while moving) acting on a clip with root motion, and the 0.06 s entry blend.
  Needs a pose dump in PIE (component pose after the graph vs the clip at the same clip time) before any more posing.
- gioco 2 (FIXED this thread): the Run Stop state still evaluated the GASP clips (ABP evaluators hard-wired); the locomotion lean leaned
  the body BACK while braking (now off while the clip plays, GSTN_RunPresentationComponent::TickLean).
- posa (minor, after gioco 1): R clip's after-stop step back of the right foot (staggered idle stance) crosses the legs around f42.

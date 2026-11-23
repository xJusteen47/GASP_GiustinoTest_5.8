# MONTAGE-EXIT-1 handover: jab squat and down-air cut (laptop, UE 5.9, 2026-11-15)
Branch `claude/project-thread-n216qs` (from anim4x `f5377ba2`, a5idak already inside), laptop folder `GASP_Giustino2_n216qs`. Built, tests below.
- **Cause, measured in Play** (`Tools/GASNPPTest/pie_probe_pose.py`: bone heights, Mover mode, montage per frame + `obj dump` of the GASP anim
  instance; data in the laptop's `Saved/MontageExit/probe{1,2,3}`, not committed). Our fork's synced montage player stopped every montage with
  `FAnimMontageInstance::GetBlend()` = the instance's blend-IN. Jab (blend-in 0): cut in one frame at its blend-out point (a25). Down-air
  (blend-in 2 f): 2 frames instead of the authored 8. Blend-in 0 vs 2 f changed nothing else: same squat to the cm.
- **The squat itself**: on the frame the jab montage starts to leave, the GASP state machine goes Idle Loop -> Idle Transition and its
  pose-match chooser picks `M_Relaxed_Transition_Crouch_to_Stand` (start 0.133 s) from the jab's exit pose. Pelvis -10 -> -43 cm, head
  -49 cm, back to idle in about 35 frames. Same thing seen after the get-ups. Base r0 clip did not trigger it, so it is the r1 clip's exit pose
  (knees 12/20, wide stance), confirmed: blend-out trigger 0 (montage to a28) still picks Crouch_to_Stand.
- **Fix 1 (fork, `PATCHES.md` row)**: `UNetMontageSimulator::MakeMontageBlendOut` = the montage's own BlendOut/BlendModeOut/BlendProfileOut,
  used by the auto blend-out and both stop paths. After it: jab leaves over 4 frames, down-air over 8 (a33 -> a40), legs move smoothly into
  the fall loop, no jump at the ability end (the fall loop is the game pose all along; the review's leg jump was the 2-frame cut).
- **Fix 2 (owner rule, all moves)**: a timeline that ends by itself RELEASES its montage (`UGSTN_GA_BaseCombat::ShouldReleaseMontageOnEnd`,
  sync var `bMontageReleased`): it plays to its own end and blend-out. A new action stops it: any `UGSTN_GA_Base` activation calls
  `StopReleasedMontages` first; any synced montage replaces it. `EndBlendOutFrames >= 0` still cuts (owner 11-13 rule). Cancels unchanged.
  Today clips are as long as their timelines, so this shows only for longer montages (dropkick land).
- **NOT fixed: the jab squat still happens**, now blended over 4 frames instead of cut. Two ways out, owner/anim choice: (a) jab 1 round 2
  ends nearer the idle stance in its last ~5 frames (feet under hips, knees straighter), recommended; (b) take the Crouch transitions out of
  the standing choice in `CHT_MoverCharacterAnimations_PoseMatch` (sample content, NOT in git: both machines by hand).
- **Not covered by the release**: a hit reaction, landing or walking during a released tail does not stop it (no sim hook outside abilities).
- New test `MontageSync.BlendOutIsMontages`; fixed the `Weakpoint.SetFacingEffect` crash (Mover's uninitialized `UpdatedComponent`). Headless `Automation RunTests GSTN`: **1608 success / 0 fail**, then the run hung in
  `GSTNSurface.Core.TypeTableChecksum` (no output for 55 min; process 21752 left running, I could not stop it): check that test.
- **Dropkick asks (coordinator), not done, for the next code thread**: (1) hold a montage section until Mover lands, then jump to the land
  section: generic = a timeline track "HoldUntilLanded" on a frame range that stalls the timeline through the track-hook stall (like the
  hold) while airborne and loops `MontageSection`, then jumps timeline and montage to the land frame; the movement track's `bCancelOnLand`
  already reads landing. (2) pop to standing after the land clip: same GASP pick-from-exit-pose as the jab, plus the old cut (fix 1 helps).
  (3) 53 cm pelvis drop loop->land: capsule not shrunk, needs a capsule track.
- **Montage start jitter (desktop finding)**: captures render at 60 fps over a 62.5 Hz sim, so a render frame holds 0, 1 or 2 ticks; the
  first rendered frames vary with that phase. Not looked at; check `t.OverrideFPS` vs sim rate first. **Cost**: one Opus thread, about 170k tokens of context, no subagents; 3 Play probes, 3 builds, 2 test runs; estimate $15-20.

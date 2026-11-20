# Camera jitter 3: capsule changes must not move the camera (cloud, Opus high, 2026-11-20 23:15-23:45 local) — NOT COMPILED
Branch `claude/project-thread-hiq1zx` = `claude/playtest-cam-render-skid` (bf8cc00f) + this. Owner 22:14: the bounce in his video was ZL (crouch) spam;
same for FAir and landings. Rule (owner): no capsule change, from any source, moves the camera vertically (outside ADS).
## Why it could still move (code reading; camera-jitter-2's scripted runs never pressed crouch)
1. Our side: the anchor's height came from the drawn mesh read back to a standing root. That is capsule-invariant only once capsule,
   mesh offset (ApplyShape) and Mover's smoothing of the actor shift (PresentShift correction, cleared every substep) all agree; any
   frame where they do not shows as a dip (measured 2026-11-18: 8 cm for ~5 frames per crouch, playtest-bugs-1118.md). Spam = repeated dips.
2. GASP's side (suspect, not proven): CameraDirector_SandboxCharacter reads E_Stance and activates CameraRig_CrouchOffset on Crouch
   (strings in the asset: Stance, Crouch, CameraRig_CrouchOffset, bForceNewInstance). Its offset is 0 since 11-18, but switching the rig set
   on every crouch may restart/blend the rig (damping state). If so, in_z moves while cam_comp_z stays still in the trace.
## Change
- `FGSTN_CameraAnchorSettings::VerticalSource` (new, default **CapsuleBottom**): height = capsule bottom + standing half height
  (`FGSTN_CameraAnchorDamper::StandingHeightFromCapsule`). On the ground the bottom is kept on the floor by every capsule change
  (KeepBottomOnFloor), so crouch/knockdown/landing capsules give 0 cm by construction. Horizontal stays the smoothed mesh. Steps at 62.5 Hz on
  slopes/stairs: the vertical spring (0.07 s) smooths it; in the air the air hold keeps the height. `gstn.Camera.Anchor.Vertical 1/2` A/B.
- Get Camera Pivot (custom camera API) uses the same height. Doc: docs/architecture/camera-anchor.md.
- Trace: + mesh_stand_z, half_height, crouch. camera_trace_check.py: new line "grounded capsule changes N: max height move in 0.5 s" per stage.
- camera_jitter_run.py: + crouch spam (toggle every 0.15 s, 3 s) and a jump with FAir + landing. Dair/dropkick need their own input: not added.
- Test GSTN.Camera.Anchor.CapsuleChangeKeepsHeight (crouch, knockdown, tilted up).
## Desktop (Sonnet, ~2 $)
1. Build (Run-Timed). Tests `GSTN.Camera` (expect all pass).
2. `camera_jitter_timed.ps1` 3 players, CVars `gstn.Camera.Anchor.Vertical 1` then `2` (A/B). Checker capsule line: with 1, stand_z,
   cam_comp_z must be 0.0; in_z/out_z 0 too unless GASP's director moves it (item 2): then the fix is in the director asset (ask the
   Unreal Assistant how to drop the Stance=Crouch rig or stop it restarting), outside this branch.
3. Owner plays: ZL spam, FAir, landings.
COSTO: ~5 $ | 0 minuti desktop | 0 token review

# DROPKICK-CAPSULE-2 handover: desktop build + verify of the Body Capsule track, hold window 2A (desktop, 2026-11-15) — PARTIAL: 2 code bugs left
Branch `claude/integ-dropkick-capsule` = `claude/project-thread-0pka8k` 82f2a007 + merge of `claude/desktop-jab-dropkick-r1`, folder `GASP_Giustino2_ue59`.
Opus, no subagents. Cloud design notes stay in `dropkick-capsule-1.md`. Not pushed. `AM_OverlordDAirV9` still dirty, not ours; jab `director/` untracked, not ours.
## Done
- Build fixes (2 attempts): `GSTN_BodyCapsuleModifier.cpp` const `MoverComp` in the effect params (const_cast); `QueueInstantMovementEffect_Internal`
  exported with `MOVER_API` in `Plugins/EngineOverrides/Mover/.../MoverComponent.h` (link error). Build OK: `Saved/Logs/ci/capsule_build3.log.esito`.
- Tests: `Run-Timed.ps1 -Preset Test -Tests "BodyCapsule+HoldUntilLanded+MontageSync+Timeline"` -> 88 GSTN tests, 0 failed (BodyCapsule 2,
  HoldUntilLanded 4, MontageSync 5); count = `Result=\{..\} Path=\{GSTN` lines in `Saved/Logs/ci/capsule_tests.log.ue.log`.
- Decision 2A: `setup_dropkick_hold_until_landed.py` hold = [startup + 6, startup + loop) (env `DROPKICK_HOLD_FOLLOW_THROUGH`, 0 = old). Log
  `Saved/AnimR2/setup_dropkick_hold_until_landed.txt`: hold [21, 26), land 59, total 91. In game the montage wraps 25.98 -> 21.98 (frames.jsonl `montage`).
- Kit + hold + capsule scripts run (saved only the 6 dropkick assets: KIT/WPN/AM/GA/TL_..Dropkick). Capsule log: air [9, 59) hh 35 Body, land [59, 91)
  hh 35 Capsule, script estimate drop ~21 cm.
- Capture: `pie_capture_move.py` gained `PIE_CAPTURE_COLLISION=1` (Collision show flag on every view) and a `body` field per row (pelvis z, capsule
  hh/bottom, mesh rel z). Files `docs/notes/reviews/dropkick-capsule/` (mp4 normal + quarter, side sheet v100-v158, `body_rows.txt`, `frames.jsonl`).
## Measured (`body_rows.txt`, from frames.jsonl)
- Air: capsule hh 86 -> 35 at v52 on the torso (side sheet: capsule circle on the torso, v100-v104). OK.
- Touchdown: pelvis z 36.6 (v105, last air) -> 11.7 (v107): 25 cm over 2 frames, ~13 of it is the fall speed (13-14 cm/frame) => ~12 cm extra
  (was ~53). BUT v106 is a one-frame glitch: pelvis -34.6, mesh not yet on the capsule (mesh rel z -84.4, becomes -37.0 on v107); in the side view
  the body disappears out of the frame on v106. Bug A (code): the land frame's mesh offset is applied one frame after the montage jumps to the land clip.
- End: timeline ends v137, capsule regrows hh 35 -> 86 WITHOUT moving the actor up (actor 37.2, bottom -48.8 = under the floor), then depenetration
  pops it to 118.3 (v138) and 169.3 (v154); it stays 69 cm above its start height (h=69.3). Visible pops v134-v138. Bug B (code): the Body-anchor
  growth back to standing (shift +51 cm up, swept) does not move the actor; likely the sweep starts penetrating (land capsule bottom 2.2 vs standing
  bottom 14) and blocks. Look at `FGSTN_BodyCapsuleShiftEffect::ApplyMovementEffect` Move() and the land track's Capsule anchor bottom.
## Next (code thread, Opus high, ~$10): fix A and B, add a test for "growth back to standing moves the actor up", recapture with the same boot.
Boot scripts used (scratchpad, not kept): editor `-ExecCmds="py boot.py"` with vd2_mover, then the three setup scripts / capture with env
PIE_CAPTURE_KIT=KIT_Test_OverlordDropkick, PIE_CAPTURE_COLLISION=1. Map arg `/Game/Levels/Test/L_Test_Hub` needed for the capture (generic Run-Timed Kind Capture).
Other moves needing Body Capsule data later (rule 12, not changed): dair (horizontal phases), ground/knockdown reactions (lying victim in a 180 cm
capsule), crouch attacks. Get-up = separate tech-start anim later; bounce = gameplay (momentum + input), not animated (owner, 2026-11-15).
**Cost**: estimated $12-14 (one Opus thread ~110k context, 3 builds, 1 test run, 1 setup run, 2 captures).

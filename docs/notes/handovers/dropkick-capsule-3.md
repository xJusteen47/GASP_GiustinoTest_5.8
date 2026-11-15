# DROPKICK-CAPSULE-3 handover: the two Body Capsule code bugs fixed (desktop, 2026-11-15). DONE, 1 attempt
Branch `claude/integ-dropkick-capsule` (from 6df6404f), folder `GASP_Giustino2_ue59`. Opus, no subagents. Not pushed (owner asked first).
`AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. No asset saved, no project setting changed, sim rate untouched.
## Root causes
- B (end pop): the modifier queued its shift effect with `QueueInstantMovementEffect_Internal`, which schedules through
  `GetScheduledNetworkTime` = +0.3 s (Mover's network event delay). The capsule size came from `Applied` at once, the actor move 0.3 s
  later: grown 35 -> 86 in place at v137 (bottom -48.8), depenetrated to +18 cm, then the late +51 cm landed at v154 (+69 cm). The air
  shrink (v52) was late the same way.
- A (landing blink v106): the mesh offset reached Mover's base visual transform after Mover's finalize had placed the mesh
  (combat tick is TG_PostUpdateWork), so the land pose was drawn one frame with the air offset.
## Fix (owner tip: Mover's stance modifier, `FStanceModifier::AdjustCapsule`/`CanExpand`)
- Mover fork: `UE::Mover::MakeSchedulingInfoForThisStep` / `MakeScheduledEffectForThisStep` and
  `UMoverComponent::QueueInstantMovementEffectThisStep_Internal` (execute on the issuing frame; the state machine applies it later in the
  same substep, before the mode moves). The modifier uses it. Rollback: queued and applied inside the same simulated substep.
- `ApplyShape`: when the base visual transform changes, the drawn mesh moves by the same delta now (as stance does), smoothing offset kept.
- Shift effect, growth: stance-style `OverlapTest` of the grown capsule at the destination; fits -> set directly, else the old sweep.
- Test `GSTNCombat.NP.BodyCapsule.ThisStepSchedule` (fixed and variable dt).
## Proof
- Build: `Saved/Logs/ci/capsule3_build.log.esito` ESITO OK (364 s). Tests: `Run-Timed.ps1 -Preset Test -Tests "BodyCapsule+HoldUntilLanded+MontageSync+Timeline"`
  -> 89 passed, 0 failed (count `Result=\{Success\} .*Path=\{GSTN` / `Result=\{Fail` in `Saved/Logs/ci/capsule3_tests.log.ue.log`).
- Capture: same boot as -2 (vd2_mover, KIT_Test_OverlordDropkick, PIE_CAPTURE_COLLISION=1, map L_Test_Hub), `capsule3_capture.log.esito` OK.
  Files `docs/notes/reviews/dropkick-capsule-3/` (mp4 normal + quarter, side sheet v102-v141, `body_rows.txt`, `frames.jsonl`).
- `body_rows.txt`: landing v105 -> v106 pelvis 35.3 -> 12.0 with mesh rel -37.0 on the same frame (no -34.6 frame). End v137: actor
  37.2 -> 88.2 (+51) and hh 86 on the same frame, bottom stays 2.2, no pop, stays 88.2 to v169 (h -11.8: the floor at the landing spot,
  the falling capsule stopped at bottom 2.2 there). Air: actor moves onto the torso from v51 (20 cm/frame), no longer 0.3 s late.
## Open (for the coordinator)
- v137 pelvis 49.5 -> 33.2, then rising: same drop as the old run (51.2 -> 29.8), the montage end blending into the stand; one frame
  carries a 14.7 cm smoothing offset (mesh rel -102.7) from the +51 cm actor move. Animation/blend item, not the capsule bug; check in review.
- Interpolated simulated proxies call `OnPreMovement` too (`TickInterpolatedSimProxy`): the modifier queues there; harmless today, worth a guard.
**Cost**: estimated $7-8 (one Opus thread ~95k context, 1 build, 1 test run, 1 capture).

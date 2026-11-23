# DROPKICK-LAND-1 handover: dropkick held until landing, first-frame jitter (cloud, NOT BUILT, 2026-11-15)
Branch `claude/project-thread-qxjy3w` from n216qs `8ba977e1`. Cloud thread: nothing compiled or run. Desktop thread builds and verifies.
- **Hold Until Landed track** (`GSTNCombatCore/.../Timeline/GSTN_HoldUntilLanded.{h,cpp}`, generic, data only): frames [Start, End), `LandFrame`
  (-1 = End), `LandFromFrame`, optional `LoopSection`/`LandSection`. Clock (`GSTN_CombatTimelineLogic::AdvanceClock`, sim, rollback-safe): in the
  air the timeline stands on End-1 (hitbox open to the land frame stays open); first grounded frame (Mover `IsOnGround` via the sim-state tracker,
  lent through the track-hook environment) jumps to LandFrame, skipped keys do not fire; no Mover = plays straight through. The jump leaves a
  NEGATIVE `TimelinePausedFrames`: serialized zigzag now (wire change). Montage (`UGSTN_GA_BaseCombat::UpdateHoldUntilLandedMontage`): in the air
  it loops the first loop copy, on landing it jumps to the land part via new `SetSyncedMontageTime`. End/release rules use the evaluated frame.
- **Get-up**: generic `FollowUpTimeline` on the timeline data: a combat ability started (forced) when the timeline ends by itself. No get-up
  ability/clip exists yet (anim work), so the dropkick still ends with its land clip tail. `DROPKICK_FOLLOW_UP=<BP class>` sets it later.
- **Capsule 53 cm drop: NOT done.** Needs a design choice (owner): (a) capsule track = Mover movement modifier like `FStanceModifier`, rollback-safe,
  recommended, ties into the queued ECB capsule work; (b) anim: land clip starts at the loop's pelvis height. ~$10-15 code for (a).
- **Start jitter**: the local montage was posed from the smoothed state (60 fps render over 62.5 Hz). Fork patch (PATCHES.md row): presentation
  clock, first rendered frame = the move's start time, then render-delta steps, never ahead of sim, at most 2 ticks behind. Presentation only.
  Behaviour change: `PlayMontage` of the SAME montage with `StartTime >= 0` is now a restart (holds its start tick).
- **Tests added**: `GSTNCombat.Core.HoldUntilLanded.{Clock,Montage}`, `GSTNCombat.NP.HoldUntilLanded.{Fall,EarlyLand}`, `GSTNCombat.NP.MontageSync.PresentationClock`.
- **Desktop verify, in order**: 1) merge `claude/project-thread-anim-r2` + this branch into a work branch, build _ue59 (editor closed); fix compile
  errors (likely spots: `FName = NAME_None` UFUNCTION defaults, `AllowedClasses` meta on `TSoftClassPtr<UObject>` FollowUpTimeline, zigzag casts).
  2) headless `Automation RunTests GSTN`: the 5 new tests + all `MontageSync.*` and `Timeline*` green. 3) editor: `setup_overlord_dropkick_kit.py`
  then `Tools/GAS/setup_dropkick_hold_until_landed.py` (log in `Saved/AnimR2/`, expect hold [16,27) land 60). 4) PIE: dropkick from a high jump
  holds the pose and loops until touchdown, then land clip at once; from a low jump lands out of the startup; whiff/hit both. 5) captures per
  `Tools/GASNPPTest/animr2/runbook.md`: jab1 and dropkick, first rendered frame montage label 0.00 every run, then steady ~1.0 steps. 6) PIE
  harness `abilities`, `forced_rollback`, `combat_spike` all `CMP OK` (negative paused frames, montage jumps under rollback).
- **Known limits**: follow-up classes are collected when the table set is built (a registry entry added later by a tick extension is not scanned);
  the follow-up may end the old ability as Interrupted, check the OnMoveEnded reason; landing during a hitstop frame is taken the frame after.
- **Cost**: one Opus thread, one context compaction, no subagents, no builds: about $20-30.

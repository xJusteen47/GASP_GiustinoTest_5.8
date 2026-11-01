# Spin and drill — doc deltas

Differences between the code and [spin-drill.md](../architecture/spin-drill.md) /
[spin-drill-plan.md](../architecture/plans/spin-drill-plan.md), to fold into the spec at the end of the work (D13).

## Process

- **`check_modules.py` does not exist.** The plan (D1) asks for "`check_modules.py` clean"; there is no such script in the
  repository (searched the whole tree). The D1 and D2 module checks were done by hand with grep: Core lists no GAS, NP or Mover
  module; no module lists GSTNParts, GSTNLevelObjects, GSTNLink, an Editor module or the project module; nothing depends on
  `GSTNSpinDrillTests`; no string tags outside the tag definitions and tests. Either write the script or drop it from the plan.

## D1

- No plugin had a separate test module before. `GSTNSpinDrillTests` exists (Runtime, out of Shipping) for the harness content of
  later checkpoints; unit tests stay in each module's `Private/Tests`, as in the other plugins.
- The tag ini (`Config/Tags/GSTNSpinDrill.ini`) has no leaves yet; the `GameplayCue.SpinDrill.*` leaves come with D11.

## D2

- `FGSTN_DrillTargetResponse` (spec §6) follows §13b: no `bDrillable` (drillable = penetration vs hardness), new `bNeverDrillable`.
  `LaunchOnDestroyed` is dropped: what happens when a target breaks is the bore tuning's `bContinueThroughDestroyed` (SD.7).
- `FGSTN_BoreTuning`: no `MinBoreFramesForLaunch` (§13b); new `JumpBoostWindowFrames` (§13b, read from the input history).
- `FGSTN_DrillWobbleTuning`: new `MaxWobbleDegrees`; the curve is normalised on both axes (time over the seat, amplitude 0..1).
- End conditions (§7.4 with §13b): the maximum time without a jump input gives the hop out (`DetachPop`, the §13b proposal); a
  target lost or no longer drillable gives the final jump whatever the jump input (SD.7). Crouch release counts during the seat too.
- Air-spin refresh: the default `RefreshOn` is Land, Wall, Trampoline, Launch; Hit is off by default (not decided in the spec). A
  hop out and a bounce off a non-drillable target never refresh (§7.6); a launch refreshes only when `bRefreshAirSpin`.
- Quantised anchor and axis are plain C++ structs (`FGSTN_DrillAnchorQ`, `FGSTN_DrillAxisQ`), not USTRUCTs: int16 is not a
  Blueprint type and the fragment (D3) serialises them by hand. Their functions are C++ only.
- `UGSTN_SpinDrillTuning` lives in the GAS module as the spec says; the kit extension that references it is not built yet.
- All default numbers are placeholders.

## D3

- Feature id: `GSTNFeatureIds::SpinDrill = 19` added to `GSTN_SyncFragment.h` (owner OK SD.11, seam S5): the constant, its one-line
  comment and a blank line, as its neighbours. No other GSTNCombat edit.
- `FGSTN_DrillFragment` drops `Target`, `AnchorQ` and `AxisQ` of spec §7.1 (plan §2b: the latch is a GSTNLink link). It adds
  `PhaseStartFrame` (phase timers as frame stamps), an `AirSpin` flag beside the sense bit, `LowGravityUntilFrame` (for seam S6,
  nothing reads it yet), and `SeatFrames` / `MaxBoreFrames` resolved at the latch (the target's data may change while latched).
  Sizes: 7 B ground spin, 12 B air spin, 16 / 21 B latched (spec: about 20 B active).
- The fragment is never removed once added (as the defence and link fragments); every transition returns it to its defaults
  when nothing is live, so an idle entity pays the 25 bits of framing and 0 B of payload; only an entity that never spun pays 1 bit.
- `LatchFrame = F + 1` (spec §7.4) and the seat phase starts there; the end conditions run from the latch frame on.
- A cancel returns to `None`; the cancel's recovery frames are its own timeline's job (D5).
- Tag projection: with no fragment, the tags are cleared only when the ability system holds one of them (a rollback to before the
  first spin), instead of the defence's early return; same "set, never added" rule otherwise.
- The NP module also depends on GameplayAbilities and AbilitySystemSimulation (tag projection), as GSTNLinkNP does.
- The extension's inputs (`FGSTN_DrillFrameInputs`) and tuning are defaults in D3: nothing in game drives the phase machine until D4.

## D3b (input history, GSTNInput)

- Not a spin-drill plan checkpoint: the D0 finding (16-event stick ring too short for a spin under camera/facing churn) fixed in
  GSTNInput before D4, owner OK 2026-11-01. Fix 2 of the spike report, compacted: `MaxDirectionEvents` 32, `MaxButtonEvents` 16,
  packed events (348 to 232 bytes for the history). D4 can rely on a full default 360 Spin slot in any camera/facing setup.
- Spec impact: none for spin-drill.md; input.md gets the "History rings and storage" bullet, combat.md §6.4 and §15 the new counts.
- Trace scopes added (`GSTN_InputHistory_*`, `GSTN_InputMatcher_Match`, `GSTN_SlotResolver_Resolve`, `GSTN_InputSimulator_Tick`,
  the two `SimulationTick`s); measurements and the copy audit in docs/notes/input-memory-audit.md.

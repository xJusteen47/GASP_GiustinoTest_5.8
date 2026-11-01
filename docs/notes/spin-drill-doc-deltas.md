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

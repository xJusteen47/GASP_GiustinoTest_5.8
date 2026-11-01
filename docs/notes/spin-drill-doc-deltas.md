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

## D4 (ground and air spin)

- **One class per sense.** Spec §3 has one `UGSTN_GA_Spin` with two bindings. The drill fragment is written inside the combat tick,
  before the ability system knows which command fired, so the sense is data of the timeline: a plugin track `FGSTN_SpinTrack`
  (`Sense`, `bAirSpin`, range = the spin) and four default classes `UGSTN_GA_SpinClockwise`, `...CounterClockwise`,
  `UGSTN_GA_AirSpinClockwise`, `...CounterClockwise` (abstract base `UGSTN_GA_Spin`). The track is unknown to the timeline player and
  has no hook (it never stalls or stamps).
- **How the extension sees a spin start.** At AfterExecute, when the combat state shows a timeline that started on this frame, the
  extension reads that timeline's data from the table set of the state's revision (`UGSTN_CombatSimComponent::FindTableSet`, class
  default of the registry entry: content, the same on every machine). The context's `RunningTimeline` cannot serve: it is taken
  before step 8 starts a command's timeline. Only the start frame looks further than one comparison.
- **End of a spin.** At AfterClocks a Spinning fragment ends when the combat state no longer runs the timeline it started with
  (`TimelineStartFrame` differs or no timeline): cancel, jump cancel, replacement or natural end, all one rule.
- **Low gravity without seam S6.** `UGSTN_GE_AirSpinLowGravity` (infinite) multiplies the combat stats the jump logic turns into gravity
  every frame (`JumpRiseGravityScale`, `JumpCutGravityScale`, `FallGravityScale` by the gravity SetByCaller, `MaxFallSpeed` by the fall
  speed SetByCaller), so the existing stats modifier applies it. It grants `Combat.State.SpinLowGravity`; the extension compares that
  tag with the fragment's window at BeforeTagProjection every frame and applies or removes the effect (level-triggered: a corrected or
  rolled back spin cannot leave it behind; one tag test per frame). `LowGravityExtraFrames` therefore already works (the window is in
  the fragment): **S6 is no longer needed** for SD.3. A pawn without the combat attribute set gets no effect.
- **Hop.** `UGSTN_GE_AirSpinHop` (instant velocity effect, not a launch move, so the jump state does not take it for a trampoline).
  New tuning `FGSTN_AirSpinTuning::bHopCancelsFall` (default true, Sunshine): the speed along gravity-up becomes max(current, HopSpeed);
  false adds HopSpeed. Speed across gravity is kept. Any gravity direction (the tracker's up).
- **Refresh rules.** Moved from AfterClocks to the start of AfterExecute, after the frame's jump step: Land while the combat jump
  phase is None (grounded), Trampoline on the frame the phase becomes Launched. Getting hit is not a refresh (owner 2026-11-01, already
  the default `RefreshOn`). **Wall contact is not detected yet** (no wall fact in the combat state); the Launch refresh comes with D9.
  A pawn without the combat stats has no jump state, so it always reads as grounded and its air spin is never spent (documented limit).
- **Input.** `FGSTN_SpinSlots` (Core): two 360 Spin slots in Stick space, plus the keyboard shortcut as two Motion slots
  (right-down-left = clockwise, left-down-right = counter-clockwise). A command is unique within a slot set and a step has no "any of",
  so the shortcut uses child commands `Command.Movement.Spin.Clockwise.Keys` / `.CounterClockwise.Keys` bound to the same classes. Not
  gated to digital input yet (`bDigitalDirection` not built): a pad half circle from left through down to right also matches.
- **Bindings.** `UGSTN_SpinDrillLibrary::MakeDefaultSpinBindings`: per spin command an air binding (requires `Mover.IsInAir`) and a ground
  binding (ignores it), slots inline in the binding's `SlotOverride`, so no command set or input config changes. The air classes are
  blocked by `Combat.State.AirSpinSpent`. No kit asset was changed: the harness adds the bindings in memory.
- **Kit extension and attach.** `FGSTN_SpinDrillKitExtension` (GAS, soft `UGSTN_SpinDrillTuning`) and a loadout extension that copies the
  values into `FGSTN_SpinDrillAttach` (NP, per actor, values only) wherever the loadout is compiled. `FGSTN_DrillSimTuning` gained the air
  spin's hop and gravity values. Without the entry: the defaults.
- **Tags (26, was 21).** `Command.Movement.Spin.Clockwise.Keys`, `.CounterClockwise.Keys`, `Combat.State.SpinLowGravity` (the one state
  tag granted by an effect, not projected) and `Combat.SetByCaller.SpinDrill.GravityScale`, `.FallSpeedScale`.
- **Default timelines** (`FGSTN_SpinTimelines`, Core): ground 30 frames, actionable 18, ring frames 2-16 radius 110 cm at 40 cm height,
  3 damage; air 36 frames, actionable 20, ring 2-20. Jump cancel window over the whole spin; no movement lock. Placeholders.
- **Module edges.** GSTNSpinDrillNP adds GSTNInputNP (tracker) and GSTNCombatGAS (combat stats); GSTNSpinDrillGAS adds the NP fork modules,
  GSTNInputCore/NP and GSTNCombatNP (the abilities are `UGSTN_GA_BaseCombat` children).

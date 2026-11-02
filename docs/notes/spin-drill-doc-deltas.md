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

## D5 (opposite-spin cancel)

- As spec §7.3 and SD.2 (the opposite rotation stops the spin, it does not start one), with the D4 shape: a plugin track
  `FGSTN_SpinCancelTrack` (sense of the cancelling rotation) on the cancel's timeline, and two classes `UGSTN_GA_SpinCancelClockwise`,
  `...CounterClockwise` (abstract `UGSTN_GA_SpinCancel`). The extension turns the track into the Cancel request on the frame the timeline
  starts; the phase machine keeps its D3 rule (opposite sense, from `CancelFromFrame`).
- The spin timelines carry a cancel window into the opposite spin command (the parent tag, so the keyboard child is in it) from
  `FGSTN_SpinTimelineParams::CancelFromFrame` (6, the tuning's default). The window is the gate and the tuning value is what the phase
  machine checks: keep them equal (noted on the field).
- Routing is binding data: the cancel binding requires the sense tag of the spin it stops (`Combat.State.Spinning.CounterClockwise` for the
  clockwise cancel). Bindings resolve to the most required tags, then the first: in the air the cancel and the air spin both require one
  tag, so `MakeDefaultSpinBindings` puts the cancels first (12 bindings with the keyboard shortcut).
- The cancel timeline is the recovery (`CancelRecoveryFrames`, 8): no hitbox, no movement (momentum kept), a jump window, actionable at its
  end. Spec §2's "recovery 4 frames" and the tuning's 8 disagreed: the tuning's 8 is used.
- New rule: a spin that ends early (cancel, replacement, jump cancel) also ends the air spin's low gravity, keeping only
  `LowGravityExtraFrames` (SD.3 "only during the air spin"). The air spin stays spent for the airtime.
- No new tags. Harness: `spin_opposite_cancel` in `spin_drill_scenarios.py` (not run yet, as the D4 ones).

## D6/D7 (combat core seams)

- Coordinated with the M7 thread (its G3 had not started; it rebases on these). Additive only: every existing timeline behaves as before.
- **S1 re-hit interval**: `FGSTN_HitboxSpec::RehitIntervalFrames` (0 = once per group and attack, today's rule). The interval counts
  simulation frames since the timeline started, hitstop and held frames included, so a held move keeps grinding. Instead of a frame field
  per target, the target ring stores a key: `FGSTN_HitboxSpec::MakeTargetKey` = the group (bits 0-2) plus the interval's index mod 32
  (bits 3-7), so `FGSTN_HitTarget`, its byte on the wire and the sync state size are unchanged. Needs a group below 8 (groups are already
  a byte mask); a larger group ignores the interval. `MaxTargetsPerGroup` counts per key (per interval). `FGSTN_AttackerHit::TargetKey`
  carries the key; the attacker's hit presentation matches on it, so each re-hit flashes once.
- **S2 loop section: dropped.** The bore holds its loop with a track hook stall owned by GSTNSpinDrill (the defence's hold window
  mechanism, `FGSTN_TrackHookResult::bStallThisFrame`), and S1 counts run frames, so the hitbox stays open and re-hits while held. No
  timeline player change, no new track kind. The synced montage loop becomes a presentation matter (D11).
- **S3: dropped** earlier (plan §2b, the latch's parked transform).
- **S4 attacker hit notify: already there.** M6 F10 added `FGSTN_CombatTickContext::AttackerHits` (victim id, group, inbox entry, hit
  location) from the AfterExecute point on; the drill reads it in D9. A contact normal is not in it; D9 derives it from the snapshot or
  asks for it then.
- **S6 gravity contributor: not needed** (D4: the low gravity is an effect on the combat gravity stats).
- **D-1 authoritative event key**: `FGSTN_TimelineTrack_Event::OutboxEventTag` (+ `OutboxTags`, `OutboxMagnitude`). The combat tick posts
  one sim event per crossed key with a tag to the entity's outbox (`FGSTN_CombatTimelineLogic::AppendOutboxEvents`), after the weapon
  switch step, while a timeline runs; published by the authority only. It adds events; the HitLanded and KO event structs are untouched.
- Tests: `GSTNCombat.NP.Seams.S1.TargetKey`, `.S1.RehitEveryInterval`, `.D1.EventKeyPostsToOutbox` (GSTN_CombatNPTests_Seams.cpp).

## D8 (dive)

- **Entry**: `Command.Movement.Drill` = Press(Crouch) (`FGSTN_SpinSlots::MakeDrillSlot`), bound to `UGSTN_GA_DrillDive` requiring
  `Combat.State.Spinning`; the spin timelines open a window into it from `DiveFromFrame` (3). Spec §5 says Hold(Crouch): a hold is not an
  anchor condition, so the press starts the dive and the ability ends on the release (`bEndOnTriggerRelease`). Crouch held before the spin
  does not dive (press it again).
- **Movement without a movement track**: the core does not play timeline movement tracks for ordinary timelines, so the plunge is an instant
  velocity effect at the start (`UGSTN_GE_DrillPlunge`: PlungeSpeed along the tracker's down, any gravity) plus a dive gravity effect
  (`UGSTN_GE_DrillDiveGravity`: the four jump and fall gravity scales times 0, granting `Combat.State.DiveGravity`), synced from the fragment
  like the low gravity. The speed stays constant. **Steering is the character's air control**, not a separate 15 %: tune AirControl for
  the feel (spec §13b). The air spin's low gravity ends when the dive starts.
- **Outcomes** (`ReadDiveOutcome`, AfterExecute): a hit of the dive's timeline this frame (`Context.AttackerHits`) is a bounce (D9 turns
  drillable targets into a latch first); the ground after the dive's first frame is the lag (`GroundLagFrames`, from the attach). The
  bounce sets `GSTNDrillFlags::Bounced` for that frame; the ability applies `UGSTN_GE_DrillBounce` (BounceSpeed against gravity, a hop
  that cancels the fall) and ends. The lag lasts until the ability ends it GroundLagFrames after landing; the fragment's Lag phase ends with
  the dive's timeline.
- **Timeline end rules** (`HasPhaseTimelineEnded`): Spinning and Diving end when the combat state no longer runs the timeline that started
  them; Lag when the dive's timeline ends or another starts. A timeline that started on the frame itself (an adoption) is left to
  AfterExecute, so a dive or cancel adopted on that frame is read as such.
- **Keyboard shortcut**: D4 put it under two child commands because "a step has no any of"; wrong, `FGSTN_InputCondition_AnyOf` exists.
  The two spin slots are now AnyOf(Spin 360, Motion), and the `.Keys` tags are gone. Bindings: 7 (2 cancels, 2 air, 2 ground, the dive).
- **Tags (25)**: `.Keys` x2 removed, `Combat.State.DiveGravity` added.
- **Default dive timeline** (`FGSTN_SpinTimelines::MakeDive`): 120 frames at most, the dive track, a swept sphere tip (45 cm, 20 cm below the
  root, `Combat.HitType.Drill`, 4 damage). Placeholders.
- Harness: `drill_ground_lag`, `drill_nondrillable_bounce` (not run; they need `Input.Button.Crouch` in IC_GASNPP_Test). `drill_tilted_gravity`
  not written: no harness level with a gravity volume; the vectors under any gravity are unit tested.

## D11, minimal part (presentation)

- Owner rule of 2026-10-24 (no aesthetics before the mechanics): only the procedural layer that makes the spin readable in playtests.
  `UGSTN_DrillPresentationComponent` turns the mesh around the capsule's up while Spinning (SpinRps, 2.5) or Diving/Boring
  (`VisualSpinRps`) and wobbles it while Seating (`UGSTN_DrillMath::ComputeWobble`), from the shown drill fragment and the frame, asserted
  every tick in TG_PostPhysics on top of Mover's base visual transform. Added by the Presentation module's own loadout extension on every
  machine but a dedicated server. Idle cost: one fragment lookup per tick.
- Not done (later D11, after the mechanics): placeholder montages per phase, cues, skin override hook, the `Anim.Set.*` leaves.

## D9, part 1 (the drill latch as a GSTNLink type)

- **New plugin edge: GSTNSpinDrill depends on GSTNLink** (uplugin; Core lists GSTNLinkCore, GAS adds GSTNLinkCore/GAS, NP adds
  GSTNLinkCore/GAS/NP). Plan 2b ships the drill type from this plugin, so the D1 rule "no module lists GSTNLink" ends here. GSTNLink is
  unchanged: everything goes through its public API (registry `AddDefinition`, `UGSTN_LinkRule`, the link fragment, `MarkPendingBreak`).
- **Tags (29)**: `Combat.Link.Type.Drill`, `Combat.Link.Role.Driller`, `.Drilled`, `Combat.Immunity.Drill`, declared in GSTNSpinDrillCore.
- **Type made by code**, the mount's pattern (owner did not answer asset vs code): `FGSTN_DrillLinkType::Build` (GAS), registered at
  PostEngineInit by the GAS module. Driller = role 1, parked, acts, no collision with the target, `YawLimitDeg` 0; Drilled = role 2, acts;
  `bInitiatorPredicts`, PendingFrames 2; hit break on the driller (any knockback, the target's own hits do not), own and partner death,
  leash 300 cm; no timeout, no interact offer. **The driller sits at a fixed offset (0, 0, 100) over the target's root**: GSTNLink places a
  parked role per type, not per latch, so the tip does not sit where it hit (needs a per-link anchor seam in GSTNLink: OPEN).
- **Rule** `UGSTN_LinkRule_Drill` (Core): GSTNLink's shared checks, then allies refuse (Team), `Combat.Immunity.Drill` refuses, the
  latch immunity after the last latcher refuses. No weight limit. The immunity tag is also in the definition's `RequestRequirements`,
  which is what puts it in the snapshot's request tag mask (the rule alone would never see it). It stands for "hittable but not
  drillable" until D12 decides by penetration; `FGSTN_DrillTargetResponse::bNeverDrillable` is not read by the rule (the target side
  sees no drill snapshot extension).
- **The tip asks, the link decides**: the dive tip's hit data carries `LinkType = Combat.Link.Type.Drill`. GSTNLink makes it a request:
  no damage on the tip hit (the bore does the damage), the attacker's link goes Pending on the hit frame, Active on the response (or
  PendingFrames later on the driller's own machine), idle on a refusal (the hit does not land, no hitstop).
- **Phase machine** (`FGSTN_DrillLinkView` from the link fragment, read at AfterExecute after the link tick): Diving + Active -> Latch
  (asked every frame, seats from `MinDiveFramesBeforeLatch`); Pending -> new request `AwaitLatch` (flag `GSTNDrillFlags::AwaitingLatch`);
  idle with the flag -> Bounce (refused or timed out); a tip hit with no link asked -> Bounce as D8. Latched: the link no longer Active ->
  broken by a hit = DetachHitstun, any other break (death, distance, forced) = target gone = Launch; the core state's hitstun, tumble and
  KO also detach. **Crouch held = the dive's timeline still runs** (the dive ability ends on the release) until the bore ability holds its
  own loop; so the dive's 120 frames also cap the bore for now. When the drill ends its own latch (pop, launch) while the link is Active
  it marks GSTNLink's Release break (`ReleaseLinkOnUnlatch`); the watchdog places the exit as for any link.
- Not in part 1 (next): `UGSTN_GA_DrillBore` with the track hook stall and the tip's re-hit interval (hits, `HitsLanded`, the target
  response's scales), the launch and pop velocities (`ComputeLaunchVelocity` via an instant effect), the Latched/Detached/Launched
  outbox events, the Launch air spin refresh, harness scenarios.
- Tests: `GSTNSpinDrill.NP.Latch.{LinkView, DiveOutcomeFromLink, EndAndRelease}`, `GSTNSpinDrill.GAS.Latch.LinkTypeAndRule`; the tag
  test counts 29 and checks the link type is declared.

## Triage: SUITE_LINK latch_big_enemy_host_latches_client (M7-01/02 OPEN 7)

- Not the D6/D7 seams: for a hitbox with `RehitIntervalFrames` 0 `MakeTargetKey` returns the group, so the ring key, the max-targets
  count and the hit presentation key are the values of before; the outbox event key posts only for keys with a tag. No other spin commit
  touches a file outside GSTNSpinDrill (GSTNSpinDrill is not enabled in those runs).
- The check (`hits <= 4` counter steps over the two hits of the attack window) was already at its limit before the rebase: 3, 4, 4 in the
  m7-g2/g5 editor logs; after it 5, 5, 3, 5. One odd step over the limit (half a reconcile on the counter). No code fix; options for the
  lead: count per hit instead of per window, or measure the distribution over ~10 runs at 0 % before changing the bound.

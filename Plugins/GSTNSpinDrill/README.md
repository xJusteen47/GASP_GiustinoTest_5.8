# GSTNSpinDrill

Spin and drill for the GSTN combat on Unreal Engine 5.8: a **spin** started by a stick rotation (ground spin, and one air spin per
airtime with a small hop), the **opposite-spin cancel**, and the **drill**: hold crouch during a spin to dive along gravity, latch
onto what the tip hits when it is drillable, bore with repeated hits, then hop out on release or jump out high with a buffered jump.
The design is [spin-drill.md](../../docs/architecture/spin-drill.md); the checkpoints are
[spin-drill-plan.md](../../docs/architecture/plans/spin-drill-plan.md).

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoints D1 (the plugin, its five modules, the native tags of spec
§10), D2 (tuning structs, target response, the pure drill math and the tuning asset) and D3 (the drill sync fragment and the tick
extension's phase machine) and D4 (the ground and air spin: abilities, input slots, hop, low gravity, one air spin per airtime) are
built, D5 (the opposite-spin cancel), D6/D7 (combat core seams: re-hit interval, outbox event keys) D8 (the dive) and the minimal part of D11 (the procedural spin and wobble), and D9 part 1 (the drill latch as a GSTNLink type made by code: the tip asks for it, the phase machine seats, bounces and ends from the link). Next: D9 part 2 (bore ability, launch), D10. The D0 input findings (Spin condition, keyboard shortcut) are in [spin-drill-spike-report.md](../../docs/notes/spin-drill-spike-report.md).

## Modules

| Module | Type | State | What it will hold |
|---|---|---|---|
| `GSTNSpinDrillCore` | Runtime | native tags, tuning structs, target response, `UGSTN_DrillMath`, `FGSTN_SpinTrack`, `FGSTN_SpinTimelines`, `FGSTN_SpinSlots` | Data structs, pure math, drill target response, native tags. No GAS, NP or Mover. Tests `GSTNSpinDrill.Core.*`. |
| `GSTNSpinDrillNP` | Runtime | `FGSTN_DrillFragment` (feature id 19), `FGSTN_DrillTickExtension` (priority 130), `FGSTN_SpinDrillAttach`, hop and low gravity effects | Drill sync fragment, combat tick extension, later the snapshot extension and the latched movement mode. Needs the GAS-NPP fork. Tests `GSTNSpinDrill.NP.*`. |
| `GSTNSpinDrillGAS` | Runtime | `UGSTN_SpinDrillTuning`, `FGSTN_SpinDrillKitExtension` + loadout extension, the spin abilities, `UGSTN_SpinDrillLibrary` | The five abilities (spin, spin cancel, dive, bore, launch), tuning data asset, kit extension. |
| `GSTNSpinDrillPresentation` | Runtime | `UGSTN_DrillPresentationComponent` (D11, minimal) | Procedural spin and wobble component; clients and listen hosts only. |
| `GSTNSpinDrillTests` | Runtime, not in Shipping | empty | Harness abilities and test content names. Unit tests stay in each module's `Private/Tests`. |

The Core module class registers the plugin's `Config/Tags` folder with the tag manager: the engine reads `Config/Tags` only for the
project and for Game Feature plugins, so the `GameplayCue.SpinDrill.*` leaves (ini only, none yet) would otherwise be missing.

## Dependencies

- Plugins: `GSTNInput`, `GSTNCombat`, `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNLink` (D9), `GameplayAbilities`, `NetworkPrediction`, `Mover`.
- `GSTNSpinDrillCore`: Core, CoreUObject, Engine, GameplayTags, GSTNInputCore, GSTNCombatCore, GSTNLinkCore (the drill rule); private Projects (to find the plugin's
  own folder). No GAS, NP or Mover module.
- `GSTNSpinDrillNP`: Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, AbilitySystemSimulation (tag projection and the
  effects, as GSTNLinkNP), NetworkPrediction, Mover, GSTNInputCore, GSTNInputNP (the Mover sim state tracker), GSTNCombatCore,
  GSTNCombatGAS (the combat stats the low gravity scales), GSTNCombatNP, GSTNSpinDrillCore, GSTNLinkCore, GSTNLinkGAS (the registry),
  GSTNLinkNP (the link fragment the drill reads).
- `GSTNSpinDrillGAS`: Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover,
  GSTNInputCore, GSTNInputNP, GSTNCombatCore, GSTNCombatGAS, GSTNCombatNP (the abilities are `UGSTN_GA_BaseCombat` children),
  GSTNSpinDrillCore, GSTNSpinDrillNP, GSTNLinkCore, GSTNLinkGAS (the drill link type, `FGSTN_DrillLinkType`).
- `GSTNSpinDrillPresentation`: Core, CoreUObject, Engine, GameplayTags, GSTNSpinDrillCore, GSTNSpinDrillNP.
- `GSTNSpinDrillTests`: the four modules above.
- Never: `GSTNParts`, `GSTNLevelObjects` (their bridges are separate small plugins, spec §4), an Editor module or the project module.
  `GSTNLink` is used through its public API only (D9), never edited from here.

## Data and math (D2)

Tuning (`Data/GSTN_SpinDrillTypes.h`, Core), grouped by the `UGSTN_SpinDrillTuning` data asset (GAS module, one per kit, no logic):

| Struct | Fields |
|---|---|
| `FGSTN_AirSpinTuning` | `HopSpeed`, `SpinGravityScale`, `SpinMaxFallSpeedScale`, `LowGravityExtraFrames`, `UsesPerAirtime`, `RefreshOn` (`EGSTN_AirSpinRefresh` flags: Land, Wall, Trampoline, Launch, Hit; default all but Hit) |
| `FGSTN_SpinCancelTuning` | `CancelFromFrame`, `CancelRecoveryFrames` |
| `FGSTN_DiveTuning` | `DiveDirection` (Gravity; Facing and Stick later), `PlungeSpeed`, `SteerFraction` (lateral only, 15 %), `MinDiveFramesBeforeLatch`, `GroundLagFrames`, `BounceSpeed` |
| `FGSTN_BoreTuning` | `SeatFrames`, `RehitIntervalFrames`, `MaxBoreFrames`, `DetachPopSpeed`, `JumpBoostWindowFrames`, `LatchDepthCm`, `bContinueThroughDestroyed` (no `MinBoreFramesForLaunch`: spec §13b) |
| `FGSTN_DrillLaunchTuning` | `LaunchSpeed`, `LaunchDirection` (SurfaceNormal or GravityUp), `bRefreshAirSpin` |
| `FGSTN_DrillWobbleTuning` | `WobbleCurve` (soft, amplitude over the seat, both axes 0..1), `MaxWobbleDegrees`, `PrecessionHz`, `VisualSpinRps` |

Target side: `FGSTN_DrillTargetResponse` (`Data/GSTN_DrillTargetResponse.h`): overrides only (`SeatFramesScale`, `MaxBoreFramesOverride`,
`DamagePerHitScale`), `bLatchMoves` and `bNeverDrillable`. Whether a part is drillable is penetration against hardness, not a flag.
A `static_assert` keeps it within the 20-byte snapshot extension slot.

`UGSTN_DrillMath` (`Math/GSTN_DrillMath.h`), pure, no world, no UObject read on the simulation path:

- `QuantizeAnchor` / `DequantizeAnchor`: 3 x int16, 1 cm, at most 0.5 cm error, false when clamped (beyond 327.67 m). C++ only.
- `EncodeAxis` / `DecodeAxis`: octahedral 2 x int16, below 0.01 degree, poles exact, zero encodes +Z. C++ only.
- `ResolveEnd` (`FGSTN_DrillEndInputs` -> `EGSTN_DrillEndResult`), first match: hitstun or KO -> `DetachHitstun`; target gone or
  not drillable -> `Launch` (`KeepPlunging` with `bContinueThroughDestroyed`); crouch released or maximum reached -> `Launch` with a
  jump boost, `DetachPop` without; else `Continue`. Helpers `ResolveMaxBoreFrames`, `ResolveSeatFrames`, `IsJumpBoostInWindow`.
- `ShouldRefreshAirSpin`, `AirSpinsUsedAfterEvents`, `CanAirSpin`, `RefreshEventsForEnd` (only a launch that the tuning lets refresh).
- `ComputeLaunchVelocity` (any gravity, fallbacks for a zero normal or zero gravity), `ComputeDiveVelocity` (lateral steering).
- `ComputeWobble` (amplitude and phase from frames since latch and sampled curve values; exactly 0 from `SeatFrames`) and
  `SampleWobbleCurve` (reads the curve once, outside the simulation).

Default numbers are placeholders to tune in play, not design values.

## Sync fragment and tick extension (D3)

`FGSTN_DrillFragment` (`Sim/GSTN_DrillFragment.h`), registered under `GSTNFeatureIds::SpinDrill` = 19 at module startup: phase
(None, Spinning, Diving, Seating, Boring, Launching, Lag), flags (spin sense, air spin), phase start frame, air spins used, low
gravity until frame, latch frame, seat and maximum bore frames resolved at the latch, hits landed. The latch target, anchor and axis
are not here: plan §2b moves them to the GSTNLink latch. Wire (measured by `GSTNSpinDrill.NP.Fragment.RoundTripAndBits`): no
fragment 1 bit; idle fragment 0 B payload, 25 bits with the list framing; ground spin 7 B; air spin 12 B; latched 16 B (21 B with
the air group live), plus 25 bits of framing.

`FGSTN_DrillTickExtension` (`Sim/GSTN_DrillTickExtension.h`), priority 130 (after parts 100, defence 110, links 120): the refresh
events, the low-gravity stamp, seat to bore and the end of a spin, launch or lag timeline at AfterClocks; nothing at
BeforeCandidates yet; the request of the frame and the bore end rules (`UGSTN_DrillMath::ResolveEnd`) at AfterExecute; the
`Combat.State.*` tags set (never added) at BeforeTagProjection. The pure step functions (`StepAfterClocks`, `StepAfterExecute`,
`GatherStateTags`) are public for tests.

## Spin (D4)

- **Abilities** (`Abilities/GSTN_GA_Spin.h`, GAS): `UGSTN_GA_SpinClockwise`, `UGSTN_GA_SpinCounterClockwise` (ground, 3D World) and
  `UGSTN_GA_AirSpinClockwise`, `UGSTN_GA_AirSpinCounterClockwise` (air, Sunshine), children of the abstract `UGSTN_GA_Spin`. Each is a
  timeline ability whose inline timeline (`FGSTN_SpinTimelines`) holds a `FGSTN_SpinTrack` (sense, air flag), the hitbox ring and a jump
  cancel window; no movement lock. The air classes apply `HopEffect` (`UGSTN_GE_AirSpinHop`) when their timeline starts and are blocked
  by `Combat.State.AirSpinSpent`. One class per sense because the fragment is written in the combat tick before the ability system knows
  the command.
- **Simulation**: at AfterExecute the tick extension sees a timeline that started this frame with a spin track and writes the Spinning
  phase (sense, air spin used, low gravity window); at AfterClocks the phase ends when the combat state no longer runs that timeline.
  Refresh at AfterExecute from the combat jump state: Land while grounded, Trampoline on the launch edge; a hit never refreshes.
- **Low gravity** without a core seam: `UGSTN_GE_AirSpinLowGravity` multiplies the jump and fall gravity scales and the maximum fall speed
  (SetByCaller from the tuning) and grants `Combat.State.SpinLowGravity`; the extension applies or removes it every frame from the
  fragment's window (one tag test). `LowGravityExtraFrames` extends the window.
- **Hop**: `UGSTN_DrillMath::ComputeHopVelocity` on the tracker's velocity and up; `bHopCancelsFall` (default) makes the speed along up
  max(current, `HopSpeed`), so the spin always lifts; any gravity.
- **Input** (`FGSTN_SpinSlots`, Core): `Command.Movement.Spin.Clockwise` / `.CounterClockwise` = AnyOf(Spin 360 in Stick space, the
  Motion D-S-A / A-S-D); `Command.Movement.Drill` = Press(Crouch). The keyboard Motion is not gated to digital input yet.
- **Kit**: `UGSTN_SpinDrillLibrary::MakeDefaultSpinBindings` (air binding requires `Mover.IsInAir`, ground binding ignores it, slots inline),
  `AddSpinBindingsToKit` for setup scripts and the harness. Tuning per kit: a `FGSTN_SpinDrillKitExtension` entry naming a
  `UGSTN_SpinDrillTuning`; its loadout extension copies the values into `FGSTN_SpinDrillAttach` on every machine. No entry = defaults.
- **Opposite-spin cancel (D5)**: the spin timelines open a window into the opposite spin command from frame 6; the bindings route
  that command to `UGSTN_GA_SpinCancelClockwise` / `...CounterClockwise` while `Combat.State.Spinning.<other sense>` holds (cancel
  bindings first). The cancel's timeline carries a `FGSTN_SpinCancelTrack` and is the 8-frame recovery (no hitbox, momentum kept); the
  extension stops the spin and ends the air spin's low gravity. Outside a spin the same rotation is an ordinary spin.
- **Dive (D8)**: crouch pressed during a spin (`Command.Movement.Drill`, window from frame 3) starts `UGSTN_GA_DrillDive`: a plunge at
  PlungeSpeed along gravity with no gravity while diving (`UGSTN_GE_DrillDiveGravity`), steering by air control, the drill tip
  (`Combat.HitType.Drill`). A tip hit asks for the drill latch (D9); a refused one bounces, the ground gives the landing lag, releasing crouch ends it.
- **Harness**: `Tools/GASNPPTest/spin_drill_scenarios.py` (`spin_ground_hits_ring`, `spin_air_hop_low_gravity`,
  `air_spin_once_per_airtime`, `spin_opposite_cancel`, `drill_ground_lag`, `drill_nondrillable_bounce`), bindings added in memory.
- Limits: wall contact does not refresh the air spin yet (no wall fact in the combat state); a pawn without the combat stats never
  spends its air spin; deviations in [spin-drill-doc-deltas.md](../../docs/notes/spin-drill-doc-deltas.md) "D4".

## Tags

Native, declared in `GSTN_SpinDrillTags.h` (`GSTNSpinDrillTags`), listed by `GetDeclaredTags` (25 tags):

| Tags | Kind |
|---|---|
| `Command.Movement.Spin.Clockwise`, `.CounterClockwise`, `.Button`, `Command.Movement.Drill` | command leaves (the `Command.Movement` family is the combat plugin's) |
| `Input.Button.Spin` | optional button leaf, not in the default kit |
| `Combat.State.Spinning`, `.Spinning.Clockwise`, `.Spinning.CounterClockwise`, `.AirSpinSpent`, `.Drilling`, `.DrillLatched` | projected from the drill fragment, never granted |
| `Combat.State.SpinLowGravity`, `Combat.State.DiveGravity` | granted only by the low gravity and dive gravity effects (the exceptions to "projected") |
| `Combat.SetByCaller.SpinDrill.GravityScale`, `.FallSpeedScale` | magnitudes of the low gravity effect |
| `Combat.HitType.Drill` | hit type leaf |
| `Combat.Event.Drill.Latched`, `.Detached`, `.Launched`, `.GroundLag` | events, published once on the authority |
| `Ability.Movement.Spin`, `.SpinCancel`, `Ability.Combat.Drill.Dive`, `.Bore`, `.Launch` | ability identity |

Not declared yet: the `Anim.Set.*` leaves (presentation checkpoint, GSTNAnimation's rules), `Combat.Link.Type.Drill` and its roles
(latch checkpoint), the `GameplayCue.SpinDrill.*` ini leaves.

## How to enable

Off by default. To try it: enable `GSTNSpinDrill` in the `.uproject` (or Edit > Plugins), rebuild, and run the tests
`Automation RunTests GSTNSpinDrill`. Enabling it changes nothing in game until a kit binds the spin commands
(`UGSTN_SpinDrillLibrary::AddSpinBindingsToKit`, or the bindings by hand).

## Extending

Make a `UGSTN_SpinDrillTuning` asset per kit and name it in the kit's `FGSTN_SpinDrillKitExtension`. Another spin is a timeline asset
with a `FGSTN_SpinTrack` (or a Blueprint child of a spin class with its own timeline); another input is a different slot on the binding.
The target response is filled by a kit or a drill-target interface in a later checkpoint; see spec §13.

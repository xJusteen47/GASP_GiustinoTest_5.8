# GSTNSpinDrill

Spin and drill for the GSTN combat on Unreal Engine 5.8: a **spin** started by a stick rotation (ground spin, and one air spin per
airtime with a small hop), the **opposite-spin cancel**, and the **drill**: hold crouch during a spin to dive along gravity, latch
onto what the tip hits when it is drillable, bore with repeated hits, then hop out on release or jump out high with a buffered jump.
The design is [spin-drill.md](../../docs/architecture/spin-drill.md); the checkpoints are
[spin-drill-plan.md](../../docs/architecture/plans/spin-drill-plan.md).

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoints D1 (the plugin, its five modules, the native tags of spec
§10), D2 (tuning structs, target response, the pure drill math and the tuning asset) and D3 (the drill sync fragment and the tick
extension's phase machine) are built. No ability yet, so nothing drives the phase machine in game (D4 onward). The D0 input findings (Spin condition,
keyboard shortcut) are in [spin-drill-spike-report.md](../../docs/notes/spin-drill-spike-report.md).

## Modules

| Module | Type | State | What it will hold |
|---|---|---|---|
| `GSTNSpinDrillCore` | Runtime | native tags, tuning structs, target response, `UGSTN_DrillMath` | Data structs, pure math, drill target response, native tags. No GAS, NP or Mover. Tests `GSTNSpinDrill.Core.*`. |
| `GSTNSpinDrillNP` | Runtime | `FGSTN_DrillFragment` (feature id 19), `FGSTN_DrillTickExtension` (priority 130) | Drill sync fragment, combat tick extension, later the snapshot extension and the latched movement mode. Needs the GAS-NPP fork. Tests `GSTNSpinDrill.NP.*`. |
| `GSTNSpinDrillGAS` | Runtime | `UGSTN_SpinDrillTuning` (data asset, no logic) | The five abilities (spin, spin cancel, dive, bore, launch), tuning data asset, kit extension. |
| `GSTNSpinDrillPresentation` | Runtime | empty | Procedural spin and wobble component; clients and listen hosts only. |
| `GSTNSpinDrillTests` | Runtime, not in Shipping | empty | Harness abilities and test content names. Unit tests stay in each module's `Private/Tests`. |

The Core module class registers the plugin's `Config/Tags` folder with the tag manager: the engine reads `Config/Tags` only for the
project and for Game Feature plugins, so the `GameplayCue.SpinDrill.*` leaves (ini only, none yet) would otherwise be missing.

## Dependencies

- Plugins: `GSTNInput`, `GSTNCombat`, `GSTNCombatGAS`, `GSTNCombatNP`, `GameplayAbilities`, `NetworkPrediction`, `Mover`.
- `GSTNSpinDrillCore`: Core, CoreUObject, Engine, GameplayTags, GSTNInputCore, GSTNCombatCore; private Projects (to find the plugin's
  own folder). No GAS, NP or Mover module.
- `GSTNSpinDrillNP`: Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, AbilitySystemSimulation (tag projection, as
  GSTNLinkNP), NetworkPrediction, Mover, GSTNInputCore, GSTNCombatCore, GSTNCombatNP, GSTNSpinDrillCore.
- `GSTNSpinDrillGAS`: Core, CoreUObject, Engine, GameplayTags, GSTNCombatCore, GSTNCombatGAS, GSTNSpinDrillCore, GSTNSpinDrillNP.
- `GSTNSpinDrillPresentation`: Core, CoreUObject, Engine, GameplayTags, GSTNSpinDrillCore, GSTNSpinDrillNP.
- `GSTNSpinDrillTests`: the four modules above.
- Never: `GSTNParts`, `GSTNLevelObjects` (their bridges are separate small plugins, spec §4), an Editor module or the project module.
  `GSTNLink` comes in with the latch (D9), through a new link type.

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
`GatherStateTags`) are public for tests. `GatherInputs` and `GetTuning` return defaults until the abilities (D4) and the kit
extension exist, so in game an entity never gets a fragment yet.

## Tags

Native, declared in `GSTN_SpinDrillTags.h` (`GSTNSpinDrillTags`), listed by `GetDeclaredTags` (21 tags):

| Tags | Kind |
|---|---|
| `Command.Movement.Spin.Clockwise`, `.CounterClockwise`, `.Button`, `Command.Movement.Drill` | command leaves (the `Command.Movement` family is the combat plugin's) |
| `Input.Button.Spin` | optional button leaf, not in the default kit |
| `Combat.State.Spinning`, `.Spinning.Clockwise`, `.Spinning.CounterClockwise`, `.AirSpinSpent`, `.Drilling`, `.DrillLatched` | projected from the drill fragment, never granted |
| `Combat.HitType.Drill` | hit type leaf |
| `Combat.Event.Drill.Latched`, `.Detached`, `.Launched`, `.GroundLag` | events, published once on the authority |
| `Ability.Movement.Spin`, `.SpinCancel`, `Ability.Combat.Drill.Dive`, `.Bore`, `.Launch` | ability identity |

Not declared yet: the `Anim.Set.*` leaves (presentation checkpoint, GSTNAnimation's rules), `Combat.Link.Type.Drill` and its roles
(latch checkpoint), the `GameplayCue.SpinDrill.*` ini leaves.

## How to enable

Off by default. To try it: enable `GSTNSpinDrill` in the `.uproject` (or Edit > Plugins), rebuild, and run the tests
`Automation RunTests GSTNSpinDrill`. Enabling it changes nothing in game until a kit binds its commands (D4 onward).

## Extending

Make a `UGSTN_SpinDrillTuning` asset per kit (the kit extension that references it comes later). The target response is filled by a
kit or a drill-target interface in a later checkpoint. Blueprint hooks on the abilities arrive with D4 and D5; see spec §13.

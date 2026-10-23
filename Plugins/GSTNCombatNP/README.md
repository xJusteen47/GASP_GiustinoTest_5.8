# GSTNCombatNP

The Network Prediction adapter of [GSTNCombat](../GSTNCombat/README.md). It was the `GSTNCombatNP` module of the
`GSTNCombat` plugin and is now its own plugin so that `GSTNCombat` (`GSTNCombatCore`) works in a project without the GAS-NPP
fork. The module keeps its name, so class paths (`/Script/GSTNCombatNP.*`) and include paths are unchanged and no class
redirect is needed. Design: [docs/architecture/combat.md](../../docs/architecture/combat.md).

Status: **experimental (0.1)**.

## What it contains

The combat NP model: `UGSTN_CombatSimComponent` (`Simulation/GSTN_CombatSimComponent.h`, model `GSTN_Combat`, sort priority
`PreKinematicMovers - 3`) and its sync/aux/input types (`Simulation/GSTN_CombatSimTypes.h`): the sync state is `FGSTN_CombatSyncCore` (the embedded input state
and the hitstop window) plus a list of sync fragments (below). It embeds the input command and
the input sim state of [GSTNInputNP](../GSTNInputNP/README.md) and is the host of its simulator (`IGSTN_InputSimHost`), so the
component only adds the debug hitstop. It also implements `IGSTN_CommandSourceInterface` (the activation context and
`IsCommandButtonHeld` for abilities), honours the command locks (`Input.Lock.Command[.<root>]`) and publishes
`Input.Event.CommandExecuted` through `UGSTN_SimEventSubsystem` on the authority only (the listen-server host's own pawn
included). Everything that touches the ability system goes through here. `UGSTN_CombatSimComponent` is `Blueprintable` so a
project can subclass it for its own defaults; its `InputConfig` and `SlotBindings` are soft references resolved once in
`BeginPlay`, and its simulation hooks stay in C++.

## Sync fragments and tick extensions

**Action part (combat-m3 7.1).** `FGSTN_CombatSyncCore` also carries what the combatant is doing: `ActionState` (`Actionable`, `InTimeline`),
`ActiveTimeline` (registry index or `INDEX_NONE`), `ActiveSource` (`EGSTN_ActivationSource`, from GSTNInputNP), `TimelineStartFrame` and
`TimelinePausedFrames` (timeline frame = `F - TimelineStartFrame - TimelinePausedFrames`), `PausedFramesTotal` (combat clock = `F - PausedFramesTotal`),
`ActiveWeaponIndex`, `LoadoutRevision` and `Requests`, a fixed array of four `{ Type, Frame, Arg }` (`Simulation/GSTN_CombatActionTypes.h`). An idle
combatant holds the defaults and pays **one bit** for the whole part, so a game that applies no loadout sends what it sent before plus that bit. A request is
only valid in the frame it was made: the combat tick must call `Requests.PruneBefore(Frame)` at the top of every frame (C10 wires it; no request is made before then), so the wire never carries a dead one.
`LoadoutRevision` is written only by the server, out of band, with `UGSTN_CombatSimComponent::ServerWriteLoadoutRevision`; the tick makes requests and adopts
abilities only while `Core.IsLoadoutCurrent(TableRevision)` (the loadout revision gate, combat-m3 7.8). All of it takes part in `ShouldReconcile`, `ToString` and
`Interpolate` (discrete: the newer frame). The tick reads and writes them (the clock, the requests, adoption, the weapon index); the types are
`FGSTN_CombatRequest` and `FGSTN_CombatRequests` (`Find` and `Remove` by type, frame and argument: one frame can hold the commits of a replaced
timeline and of its replacement), `FGSTN_CombatAuxState` (the model's aux state, empty) and `FGSTN_CombatTickContext` (what a tick extension sees).
The `Combat.State.*` tags exist in `GSTNCombatCore` but the tick does not project them yet.

**Clock, aim and command marker (for abilities).** The component answers three queries about the frame being simulated, for an
ability's activation or tick (not presentation code): `GetCombatClock()` is the Network Prediction frame minus every hitstop frame so
far (`Core.PausedFramesTotal`; the pure helper is `FGSTN_CombatClock`), so it stands still in hitstop (the tick adds one to it for every frame inside the window of the previous state, and to `TimelinePausedFrames`
when a timeline runs, so a timeline also stands still and ends later by the hitstop length); `IsInTimeline()` is
`Core.ActionState == InTimeline`; `GetAimRotation()` is the camera aim of the frame's command (`HasAim()` says whether the command carried
one, see `bSendCameraAim` in [GSTNInputNP](../GSTNInputNP/README.md)). The aim is copied at the top of each combat tick into three scalars,
so a re-simulation reads the command it replays. `FGSTN_InputHostUtils::IsCommandActivation(Handle)` is true for the spec's own handle inside `CanExecuteBinding` and
`ExecuteRequest` (an `FGSTN_CommandActivationScope(Handle)` on the stack): an ability uses it to tell a command from a trigger or an effect,
and an ability that a command's ability triggers synchronously (another handle) is not a command.

**`UGSTN_GA_Base` (combat-m3 6.1; policy enum `EGSTN_InterruptPolicy`: `SkipIfBusy`, `InterruptCurrent`).** The base of the abilities that run under the combat model (`Abilities/GSTN_GA_Base.h`, `Abstract, Blueprintable`,
derives from the fork's `UNpGameplayAbility`). It gives an ability: `GetCombatComponent()` (cached when the avatar is set);
`GetActivationContext()` (the command, press frame, trigger button and source of the activation, captured in `ActivateAbility` into sync
variables and rebuilt on demand; the command is stored as its table index, never as a tag); `bEndOnTriggerRelease` (ends the ability on the
frame the button that started it is no longer held in the input history; no effect on a start without a button); `GetCombatClock()` and
`WaitCombatFrames(N)` (counts N combat frames, hitstop excluded, from a start clock in a sync variable, then calls `OnCombatFramesElapsed` /
the Blueprint event; one wait at a time, never called while a frame is restored); `GetAimRotation()`; and `InterruptPolicy`
(`SkipIfBusy` refuses a non-command activation while a timeline runs; `InterruptCurrent` cancels the running abilities that carry any tag of
`InterruptTags`; a command is never subject to it). Call the parent's `ActivateAbility` before reading the context. The decisions are
the pure `FGSTN_GARules` (`Abilities/GSTN_GATypes.h`). Costs, cooldowns, block/cancel tags and triggers are the ability system's own.

**Timeline abilities (combat-m3 6.2, 7.2).** `UGSTN_GA_BaseCombat` derives from `UGSTN_GA_Base` and carries frame data (`Timeline`, or a
`TimelineAsset` that wins): total and first actionable frames, a startup override window, cancel windows, a cancellable tail, a montage.
The component builds a **timeline registry** from the bindings of its command table (the timeline ability classes, each once, in binding order;
the index is what `ActiveTimeline` stores). Each combat tick: requests older than the frame are pruned; the running timeline is evaluated
(`FGSTN_CombatTimelineLogic::AdvanceClock`: it ends at its length, or when its ability is gone; crossing the commit frame requests
`CommitTimeline`); the input simulation asks the component `IsCommandAllowed` for every candidate (`FGSTN_CombatTimelineLogic::IsCommandAllowed`:
free when idle, in the tail or from the first actionable frame; inside the override window a command named by `OverrideInto`, or of a higher tier
when it is empty; inside an open cancel window a command its query names); a timeline command that wins starts the timeline
(`Start`: a `CancelTimeline` request for the replaced entry and the new state). The ability is activated by the input request of the same frame,
finds itself in the combat state, plays its montage and ends when the state stops naming it. The ability system's pre-tick runs
`CancelTimeline`, the input request, then `CommitTimeline` (`CommitAbility`), so a timeline replaced inside its override window never pays.
**Adoption (combat-m3 7.4).** A timeline ability that something other than a command activates (a gameplay event or tag trigger, an effect, another
ability, AI) finds no start in the combat state, so it marks itself as waiting (sync var `bAwaitingAdoption` plus the loose tag
`Combat.Ability.AwaitingAdoption`) and the combat tick of the next frame adopts it (`FGSTN_CombatTimelineLogic::Adopt`: the first waiting entry in registry
order wins, the others get a `CancelAbility` request, the timeline state is written with source `Adopted`); the ability starts in its own tick of that frame.
The class has to be in the registry: list it in `UGSTN_CombatSimComponent::AdoptableTimelines` (same list on every machine) or bind it to a command.
The tick publishes `Combat.Event.TimelineStarted` (source tag `Combat.Source.{Command,Forced,Adopted}`) when a timeline reaches its commit.
Not done yet: tag projection, facing, movement track, the other events (C12), hits (M4).
All of the logic is pure and covered by `GSTNCombat.NP.Timeline.*`.

**Sync state.** `FGSTN_CombatSyncState` is `Core` (`FGSTN_CombatSyncCore`: the input state, the hitstop window and the action part) followed by
`Fragments`, a list of `FGSTN_SyncFragment` from `GSTNCombatCore` that features add per entity. The core's bytes are the same with
or without fragments, and an entity with none pays one bit for the list (the format and the reason it is length-prefixed are in the
[GSTNCombat README](../GSTNCombat/README.md)). A fragment mismatch against the authority asks for a reconcile like the hitstop window
does, and so does a mismatch of the input state (it is authoritative; the trace names the field). Interpolated proxies take the newer core, and each fragment
decides how it blends (default: the newer one). No production fragment type exists yet (the tests define their own).

**Tick extensions.** `IGSTN_CombatTickExtension` (`Simulation/GSTN_CombatTickExtension.h`) is the C++ per-frame hook an optional plugin registers in
`FGSTN_CombatTickExtensionRegistry::Get()`. The combat tick calls the registry at four fixed points (`EGSTN_CombatHookPoint`:
`AfterClocks`, `BeforeCandidates`, `AfterExecute`, `BeforeTagProjection`, the steps of combat-m3 7.2); with nothing registered each call is an empty loop.
Extensions run ordered by `GetPriority()` (lower first), ties by `GetTypeId()` (a `GSTNFeatureIds` value), so the order is the same on every
machine whatever the plugin load order; two extensions with one type id are refused. An extension is simulation code: deterministic, its state only in
its own sync fragment, no component, world or ability system reads, no net-mode branches. Until the timeline clock and tag projection exist
in the model the four points sit around the input simulator call (which does candidates, resolution and the request in one function); their meaning
is fixed and the call sites move with the model.

*Register an extension:* derive from `IGSTN_CombatTickExtension`, give it a `GetTypeId()` (a `GSTNFeatureIds` value) and a `GetPriority()`, implement
the hooks it needs and keep its state in its own `FGSTN_SyncFragment`; then call `FGSTN_CombatTickExtensionRegistry::Get().Register(&Extension)` once
from the module's startup (and `Unregister` at shutdown). The hook receives an `FGSTN_CombatTickContext` (`In` read-only, `Out` for its fragment).

*Model order:* the combat model (`GSTN_Combat`) sorts at `PreKinematicMovers - 3`, the embedded input model `GSTN_Input` (sort priority 46)
is part of its state and has no tick of its own; the ability system is `- 2`, Mover after it.

## Dependencies

- Plugins (all non-optional, see `GSTNCombatNP.uplugin`): `GSTNCombat`, `GSTNCombatGAS`, `GSTNInputNP`, `GSTNInput`, GameplayAbilities,
  AbilitySystemSimulation, NetworkPrediction (the GAS-NPP fork in `Plugins/GASNPP`), EnhancedInput, Mover.
- Module `GSTNCombatNP` depends on `GSTNCombatCore`, `GSTNCombatGAS` (the timeline ability tags, the combat attribute set the stats modifier reads),
  `GSTNInputCore`, `GSTNInputNP` and the fork modules. Nothing in `GSTNCombat`
  depends on this plugin.
- Dependency rule: an `*NP` module may depend on another plugin's `*NP` module of the same layer, and on nothing above it.
  `GSTNCombatNP` depends on `GSTNInputNP` by design (one NP model per character embeds the input simulation) and may use only its
  Public headers and interfaces (for example `IGSTN_InputSimHost`), never its internals. Core modules keep no NP, GAS or Mover
  dependencies.

## Tags and settings

This plugin declares no gameplay tags and owns no settings class and no config file: the combat tags, lock tags and
`UGSTN_CombatSettings` live in `GSTNCombatCore` (`Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`), the input tags and
settings in `GSTNInputCore`.

## How to wire it

1. Enable the plugin `GSTNCombatNP` (the project's `.uproject` lists it; it pulls in `GSTNInputNP` and the fork).
2. Put `UGSTN_CombatSimComponent` on the pawn next to a `UNpAbilitySystemComponent` and a Mover component, set its
   `InputConfig` and `SlotBindings` (a `UGSTN_CommandBindingSet`), and use `UGSTN_StandardSlots` from `GSTNCombatCore` for the
   default command set. A pawn uses either this component or `UGSTN_InputSimComponent` of `GSTNInputNP`, never both.
3. A project module that uses the component lists `GSTNCombatNP` (and the Core modules it includes from) in its `Build.cs`.

## How a frame flows

On a pawn that has the combat component, an ability system component and a Mover component, every fixed Network
Prediction tick (62.5 Hz) runs the three models in this order, on the server, on the predicting client and again on
every re-simulation after a rollback:

```
combat model (priority 47)  ->  ability system (48)  ->  Mover (50)
```

1. **Combat model.** Reads the frame's input command (buttons, raw stick, camera yaw), classifies the stick against the
   facing the Mover tracker knows for the end of the previous frame, and appends the result to the input history kept in
   its sync state. It then decides what it wants from the ability system (activate a move, pause the montage for
   hitstop) and writes that as a *frame-stamped request* in its own sync state. It never calls the ability system.
2. **Ability system.** At the start of its tick it broadcasts `OnSimulationPreTick` (a hook added to the fork, see
   [PATCHES.md](../GASNPP/PATCHES.md)). The combat component executes the pending requests there, so activations run
   with the correct frame, time and rollback bookkeeping.
3. **Mover.** Moves the character. Abilities reach Mover only through queued moves, never by writing character state.

Consequences worth remembering: tags and effects granted in a frame are visible to Mover in the same frame, but
Mover's result is visible to the abilities one frame later; all code in this chain must be deterministic and keep its
state in the sync/aux state (rules in [gas-npp.md](../../docs/architecture/gas-npp.md)).

## Tests

Automation tests `GSTNCombat.NP.*` in `Source/GSTNCombatNP/Private/Tests/` run without a world, headless, with the rest:
`Automation RunTests GSTNInput+GSTNCombat`.

- `Fragments.{RoundTrip0, RoundTrip1, RoundTrip2, UnknownTypeId}`: the wire format of the combat sync state with zero, one and two fragments
  (the log prints the bits of each; zero fragments are exactly one bit over the core), the array order not affecting the bytes,
  `ShouldReconcile`, `Interpolate` and `ToString` of the state, a fragment of an unknown type id being skipped without moving the reader, and a
  malformed count being an archive error. The fragments used are test-only types with ids from the test range.
- `State.{RoundTrip, RequestsStaleDropped, Interpolate, ShouldReconcile, RevisionGate}`: the wire format of the action part, requests of older frames
  dropped by `PruneBefore`, interpolation, the reconcile rule and the loadout revision gate. `Fragments.UnsendableDoesNotReconcile`: a fragment that
  cannot be sent never asks for a reconcile.
- `Timeline.{Adopt, AdoptSameClass, SameFrameCommit, Hitstop}`: adoption (first waiting wins, losers cancelled, the running class adopted again is not
  cancelled), the requests of one frame when a timeline is replaced at the end of its window, and the clock standing still in hitstop.
- `Timeline.{CommitDeferred, Clock, OverrideInto, CancelWindow, Replace, TailCancel}`: the timeline part of the tick on plain sync state: the deferred
  commit (once, on the frame the override window ends, or on the start frame without a window), the clock and the end of a timeline, the override rule
  (higher tier, explicit query), cancel windows (half open), replacement (the old entry is cancelled, nothing was committed) and the cancellable tail.
- `GABase.{WaitFramesRollback, InterruptPolicy, ReleaseEnds}`: the combat clock and a wait counted on it (through hitstop and through a
  rollback and replay), the interrupt truth table and the command marker scope, the end on trigger release. Pure rules; the ability
  itself is checked by the PIE scenarios.
- `TickExtension.{OrderByPriority, EmptyHooksNoEffect}`: run order by priority then type id independent of registration order, duplicate
  ids refused, and the hook points changing nothing when no extension or only an observer is registered.

The pure logic is covered by `GSTNCombat.Core.*` (in `GSTNCombatCore`) and `GSTNInput.NP.*` (in `GSTNInputNP`). The network behaviour of the
combat model is checked with the PIE harness `Tools/GASNPPTest/gasnpp_test.py` (dedicated server + 2 clients), scenario
`combat_spike`; any change to the combat sync state needs it (see [testing.md](../../docs/architecture/testing.md)).

## Conventions

Types are prefixed `GSTN_`; export macro `GSTNCOMBATNP_API`. Public headers in `Public/<Area>/`, implementations in
`Private/<Area>/`. Simulation code must be deterministic and rollback-safe (rules in
[gas-npp.md](../../docs/architecture/gas-npp.md)).

## Further reading

[combat.md](../../docs/architecture/combat.md) · [combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[input.md](../../docs/architecture/input.md) · [GSTNInputNP README](../GSTNInputNP/README.md) ·
[gas-npp.md](../../docs/architecture/gas-npp.md) · [PATCHES.md](../GASNPP/PATCHES.md)

## Stats movement modifier (combat-m3 5)

`FGSTN_StatsMovementModifier` (`Movement/GSTN_StatsMovementModifier.h`) is a permanent Mover modifier that makes a character's speeds follow
its `WalkSpeed` stat. `UGSTN_CombatSimComponent` queues it once (`bQueueStatsModifier`, default on) on the authority and the owning client, on the first
tick where Mover's simulation exists. On every simulated substep (`OnPreMovement`) it assigns **archetype value x factor**, never the current value
times a factor, to the speeds the modes read, so the result is a pure function of the restored stats and a rollback cannot leave a stale value. Factor =
`WalkSpeed / UGSTN_CombatSettings::StatsReferenceWalkSpeed` (0 while the owner has `Combat.Lock.Movement`: the character brakes with its own
deceleration). What it writes, in all modes of the component: `UCommonLegacyMovementSettings::MaxSpeed`; `MaxSpeedOverride` of a mode whose archetype sets
it (>= 0); and the float properties named in `UGSTN_CombatSettings::StatsSpeedProperties` (the gait variables of a Blueprint walking mode, which feed
`MaxSpeedOverride` and so win over the shared settings: the project lists `WalkSpeed`, `RunSpeed`, `SprintSpeed`, `CrouchSpeed` in its
`Config/DefaultGSTNCombat.ini`). A character whose ability system has no `UGSTN_CombatAttributeSet` is left alone. The arithmetic is `FGSTN_StatsScaling`
(GSTNCombatCore). Not built yet: acceleration/braking/air control, gravity phases (C13).
Tests: `GSTNCombat.Core.Stats.{Factor,ScalingIsBaseRelative}`, `GSTNCombat.NP.Stats.{MaxSpeedFollowsAttribute,LockMovementBrakes}`.

## Weapon switch (combat-m3 7.7)
A timeline ability with a `FGSTN_TimelineTrack_WeaponSwitch` track swaps the active weapon: when the track's frame is crossed the combat tick sets `ActiveWeaponIndex` (`FGSTN_WeaponSwitchRules::Resolve`, GSTNCombatCore), makes a `SwapWeaponStats` request carrying the old index and publishes `Combat.Event.WeaponSwitched` (authority only). The ability system's pre-tick runs `FGSTN_WeaponStatsSwap::Execute`: the old weapon's stats effect (`UGSTN_GE_WeaponStats`) is removed and the new one's applied, both found by class, so replaying the frame gives the same stats. The effects come from `UGSTN_CombatSimComponent::WeaponStats` (index 0 = primary). The primary weapon does not change during a match, so a switch is primary <-> support; a support weapon inherits the kit's stats and by default has no effect (an entry with an empty `Effect`). Tests: `GSTNCombat.Core.Stats.WeaponSwitchResolve`, `GSTNCombat.NP.Stats.SwapDeterministic`; PIE: `scenario_weapon_switch` (Tools/GASNPPTest). Not built yet: the loadout filling `WeaponStats`, the per-weapon anim layer, `UGSTN_GA_WeaponSwitch` as a plugin ability.

## Jump (combat-m3 7.9, C13)

The combat tick steps `FGSTN_CombatSyncCore::Jump` from the Mover sim-state tracker (grounded tag, speed along the local up, `WasExternallyLaunchedThisFrame`) and the stats of the ability system, before the input tick; it gates `Command.Movement.Jump` / `.FastFall` requests (`CanExecuteBinding`) and turns an accepted one into the take-off / fast fall state. `UGSTN_GA_PlatformJump` (Role Jump) only applies the impulse effect of that frame (`UGSTN_GE_PlatformJump` / `UGSTN_GE_PlatformAirJump`, upward speed `JumpVelocity` / `AirJumpVelocity`); `UGSTN_GA_PlatformFastFall` is the same class with Role FastFall. A kit binds them to its jump commands as data. The stats modifier applies the gravity phase (`UGSTN_CombatSimComponent::ApplyJumpGravity`). Not simulated yet: squat frames, `MaxFallSpeed` / `FastFallSpeed` caps. Tests: `GSTNCombat.NP.State.RoundTrip` (jump state on the wire); the behaviour is covered by the harness scenarios `variable_jump`, `air_jump_fastfall`, `tilted_gravity_jump`, `external_launch_jump`.

## Dash and dodge abilities (combat-m3 section 3, C14)

`UGSTN_GA_MoveTimeline` (abstract) is a timeline ability whose movement is one instant velocity effect applied when the timeline begins (`UGSTN_GA_BaseCombat::OnTimelineBegan`). Concrete defaults: `UGSTN_GA_Dash`, `UGSTN_GA_GroundDodge` (low long-jump arc, lands standing), `UGSTN_GA_AirDodge` (dive roll, ends on landing after the landing lag of its movement track). The speeds are the data of the `MoveEffect` (`UGSTN_GE_Move*`, creator `UGSTN_MoveImpulseCreator`), the timeline is the ability's timeline data, and the dodges carry `Combat.State.Hitstun` / `Hitstop` in `ActivationBlockedTags` (no hard-coded check: a kit's child class clears them). The creator reads the up direction, facing and velocity from the Mover sim-state tracker, so the impulse is the same on every machine under any gravity. The ability never sets character state; it ends itself on landing from the tracker's ground tag. No Mover mode and no `Combat.Mover.*` tag is added.
Bind them as data: dash to `Command.Movement.Dash`; the ground dodge (require `Input.Context.Grounded`) and the air dodge (require `Input.Context.Airborne`) to `Command.Defence.Dodge`. Tests: `GSTNCombat.NP.Dodge.*`; PIE scenarios dash, ground_dodge, air_dodge, dodge_blocked_in_hitstun, dash_tilted_gravity, kit_without_dodge, interact_tap_hold.

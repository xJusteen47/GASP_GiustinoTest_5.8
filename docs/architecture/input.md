# Input plugin (`GSTNInput`)

Fighting-game style input for any Unreal project: designer-defined commands (button, direction, motion, chord, charge,
hold, release, mash, spin), time and hold buffering, reset rules, priority between commands, and command → GameplayAbility
bindings with context requirements. Simulated and rolled back by the Network Prediction fork when used with GAS-NPP.
Source: `Plugins/GSTNInput/`.

The combat plugin (`GSTNCombat`, [combat.md](combat.md)) is built on top of it. This file describes what the input
plugin owns, what it leaves to its host, and how the code moves out of `GSTNCombat` (section 8). The input behaviour
itself (classifier, history, grammar, buffer, priority, bindings) is specified in combat.md §6–§9 and in the M1/M2
specifications; their rules do not change, only where the code lives and some names.

Status (2026-10-22): step S1 is **done**: `GSTNInputCore` exists with the generic M1 code (section 8 lists what differs from the plan). Priority criteria (S1b), `GSTNInputNP` and the rest of M2 (S2) are still design; the NP code is in `GSTNCombatNP`.

## 1. Goals

- Usable without the combat plugin: a game that only wants buffered commands driving GameplayAbilities enables
  `GSTNInput` alone.
- Usable without GAS-NPP for the pure part: `GSTNInputCore` depends on neither GAS nor Network Prediction nor Mover.
- No game vocabulary inside: no "attack", "special", "crouch", hitstun or combat tags. Buttons, commands and their
  tiers are data the game provides.
- One NP model per entity: a host simulation (the combat model) **embeds** the input simulation instead of running a
  second model next to it.

## 2. Modules

| Module | Depends on | Contents |
|---|---|---|
| `GSTNInputCore` (Runtime) | Core, CoreUObject, Engine, GameplayTags, DeveloperSettings; private EnhancedInput (only `UInputAction` references) | Today's `GSTNCombatCore/Input/` minus the combat content: core types, samples, direction classifier, history, conditions, patterns, matcher, buffer reset rules, slots, slot sets, resolver, validation, binding requirements and selection, input script, settings, input config, generic tags, the authoritative event stream. Automation tests `GSTNInput.Core.*`. |
| `GSTNInputNP` (Runtime) | GSTNInputCore, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, NetCore, Mover; private EnhancedInput | Input command, embeddable input sim state and simulator, command binding set, input sources (player, AI), context providers, the Mover sim-state tracker, the standalone input component, activation context and the interface abilities use. |

`GSTNInput.uplugin` plugin dependencies: GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, EnhancedInput,
Mover. Prefix stays `GSTN_`; export macros `GSTNINPUTCORE_API`, `GSTNINPUTNP_API`; log category `LogGSTNInput`.

`GSTNCombat` declares a plugin dependency on `GSTNInput`; `GSTNCombatCore` depends on `GSTNInputCore`, `GSTNCombatNP`
on `GSTNInputNP`.

## 3. Tags (*Decision*)

The input plugin declares only roots and the tags its own code produces or reads. Games declare the rest (natively, or
in the tag ini of section 3.1).

| Tag | Declared by | Meaning |
|---|---|---|
| `Input.Button` (root) | GSTNInputCore | Root of logical button tags; tag pickers filter on it. The buttons themselves are game data. |
| `Command` (root) | GSTNInputCore | Root of command tags; `CommandTiers` and lock mapping work on its children. |
| `Input.Lock.Command` | GSTNInputCore | Locks every command; `Input.Lock.Command.<root>` locks `Command.<root>.*` (combat.md §11.4 rule). Per-root lock tags are declared by whoever declares the root. |
| `Input.Context.Grounded`, `.Airborne`, `.OnWall` | GSTNInputCore | Context tags written by the Mover context provider (5.2); binding requirements use them. |
| `Input.BufferEvent` (root), `.AbilityActivated`, `.Landed` | GSTNInputCore | Frame events for reset rules; other systems declare theirs under the same root (combat: `.HitReceived`, `.WeaponSwitched`). |
| `Input.Event.CommandExecuted` | GSTNInputCore | Authoritative event (section 6). |

Renamed from the combat names: `Input.Combat.Button.*` → `Input.Button.*`, `Combat.Context.*` → `Input.Context.*`,
`Combat.Lock.Command*` → `Input.Lock.Command*`, `Combat.BufferEvent.*` → `Input.BufferEvent.*`,
`Combat.Event.CommandExecuted` → `Input.Event.CommandExecuted`. `Combat.Lock.Movement` stays a combat tag (it is read by
the combat stats modifier).

### 3.1 Designer tags

Game-specific buttons, commands and their per-root lock tags live in **`Config/Tags/GSTNCommands.ini`** (replaces the
planned `GSTNCombatCommands.ini`, which was never created): the one exception to the project's native-tags rule.

## 4. `GSTNInputCore`

Every type keeps its behaviour, tests and header comments. Renames:

| Today (`GSTNCombatCore`) | In `GSTNInputCore` |
|---|---|
| `GSTN_CombatCoreTypes.h` | `GSTN_InputCoreTypes.h` (same enums and masks) |
| `GSTN_CombatCoreLog.h`, `LogGSTNCombatCore` | `GSTN_InputLog.h`, `LogGSTNInput` |
| `UGSTN_CombatInputSettings` ("GSTN Combat Input") | `UGSTN_InputSettings` ("GSTN Input"), same fields, `config = Game` |
| `UGSTN_CombatInputConfig`, `FGSTN_CombatButtonDef` | `UGSTN_InputConfig`, `FGSTN_InputButtonDef`; gains `MoveAction` (2D stick action) and `bSendCameraAim` (combat.md §11.6) |
| `GSTN_CombatInputTags.h` (`GSTNInputTags`) | `GSTN_InputTags.h` (`GSTNInputTags`): the generic tags of section 3 only |
| `UGSTN_CombatEventSubsystem`, `FGSTN_CombatEvent` (planned in M2 §7b) | `UGSTN_SimEventSubsystem`, `FGSTN_SimEvent` (same API) |

Stay in `GSTNCombatCore` (combat content built on the input core): the six standard button tags (now
`Input.Button.Attack/Strong/Special/Crouch/Shield/Jump`, in `GSTN_CombatButtonTags.h`, namespace `GSTNCombatButtonTags` so
it does not clash with the generic `GSTNInputTags`), `GSTN_CombatCommandTags.h` without the context tags (`GSTNCommandTags`),
`UGSTN_StandardSlots` (the default fighting-game command set, combat.md §9.1) and its tests.

As implemented in S1 (differences from the plan above): `FGSTN_InputButtonDef` has not yet gained `MoveAction` and
`bSendCameraAim` (they land with the NP sources in S2); the generic native tags are in `GSTN_InputTags.h` as declared in
section 3, except that the per-root lock tags are not declared yet (S2c).

### 4.1 Priority criteria (*Decision*, 2026-10-19)

The order "tier → specificity → manual offset" of combat.md §8.2 is no longer hard-coded: it is the **default list of
priority criteria**, and both the order and the criteria are configurable.

- **`FGSTN_PriorityCriterion`** (USTRUCT base, used as `TInstancedStruct`, like conditions): `virtual int32 Score(const
  FGSTN_PriorityContext&) const`, where the context is the slot spec, its pattern and the settings. Higher score wins.
  Built-in criteria: `_Tier` (from `CommandTiers`), `_Specificity` (from the pattern), `_ManualOffset` (the slot's
  `PriorityOffset`).
- **`UGSTN_InputSettings::PriorityCriteria`** (`TArray<TInstancedStruct<FGSTN_PriorityCriterion>>`, max 8): the
  ordered list. Default `[Tier, Specificity, ManualOffset]` = today's behaviour. Reordering or removing is a Project
  Settings change; a **new criterion is one small C++ struct** with one function (e.g. "chords first", "a given button
  first").
- **`FGSTN_CommandPriority`** becomes the vector of the criteria's scores (`TArray<int32, TInlineAllocator<8>>`),
  compared lexicographically; ties still go to slot order. It is computed once per slot when a slot set or table is
  built (criteria must not depend on the frame), so the resolver's hot path does not change, and the loser consumption
  and the conflict validator keep comparing priorities as today.
- **Frame-dependent ordering** (which press is newer) stays in the press-group stage, configured by
  `BufferConflictPolicy` and `SimultaneousFrames`; it runs before the criteria, as now.

Generic code that names combat content today, to be cleaned in the move:

- **Settings defaults**: `CommandTiers` and `ExclusiveButtons` start empty in `UGSTN_InputSettings`. The combat tiers
  (Attack 100, Strong 200, Special 300, Combo 350, Super 400) and exclusive buttons (Attack, Strong, Special) go into the
  project's `Config/DefaultGame.ini` section `[/Script/GSTNInputCore.GSTN_InputSettings]`; combat.md §8.2 lists them
  as the recommended values.
- **`FGSTN_ButtonMap`** builds only from an input config (no hard-coded six-button order): `MakeStandard()` is gone.
- **`FGSTN_InputScript`**: the button letters are a parameter (no default to the combat buttons): the default constructor is
  gone, the constructor and the static `Build` take the button tags. Tests pass their own.
- **`UGSTN_InputSlotSet`** no longer has the `FillWithStandardSlots` editor button (it needed the combat set):
  `UGSTN_StandardSlots::FillSlotSet(Set)` does the same from code, in `GSTNCombatCore`.
- **Tag pickers**: `meta = (Categories = "Input.Button")` and `"Input.BufferEvent"` instead of the combat roots.
- **Tests**: generic tests use test-only native tags (`Input.Button.Test.Attack`… and `Command.Test.Attack.Neutral`…,
  declared in `Private/Tests/GSTN_TestTags.h` under `WITH_DEV_AUTOMATION_TESTS`), renamed `GSTNInput.Core.Input.*`.
  `StandardSlots`, the slot-set asset test (it builds the standard set), the native-tag test of the combat tags and the
  settings test of the combat defaults move to `GSTNCombatCore` as `GSTNCombat.Core.*`, with their own helpers
  (`GSTN_CombatTestUtils.h`). The combat defaults test reads `Config/DefaultGame.ini`, so it also checks that the values
  parse.
- Comments that cite combat.md sections keep citing them (the rules live there) but say "combat.md" explicitly.

## 5. `GSTNInputNP`

### 5.1 Simulation pieces (embeddable)

- **`FGSTN_InputCmd`** (was `FGSTN_CombatInputCmd`): `HeldButtons`, `PressedButtons`, `StickX/Y`, `CameraYaw`,
  `ForcedCommand`, later `AimYaw/AimPitch`; `NetSerialize`, `ToString`, `PrepareRepeatedInput`. A host model uses it
  directly as its NP input type.
- **`FGSTN_InputSimState`**: `History`, `Classifier`, `WatchedTagMask`, the frame-stamped request (`RequestedBinding`,
  `RequestButton`, `RequestFrame`), `LastActivatedFrame`. Not a sync state by itself: helpers `NetSerialize(Ar)`,
  `ShouldReconcile(Other)`, `ToString(Builder)` that the host's sync state calls for its embedded member.
- **`FGSTN_InputSimulator`** (stateless): `Tick(const FGSTN_InputTickArgs&, IGSTN_InputSimHost&)` runs M2 §5 steps
  1–7 for one frame (sample, history, watched tags, reset rules, locks, resolve, request, consumption, outbox entry) on
  the embedded state.
- **`IGSTN_InputSimHost`** (plain C++ interface, not a UInterface; the host implements it):
  `AppendFrameEvents(FGSTN_BufferFrameEvents&)` (extra reset events such as hits),
  `IsCommandAllowed(int32 CommandIndex)` (the host's own gating, e.g. combat timeline and cancel windows),
  `CanExecuteBinding(int32 BindingIndex)` (ASC `CanActivateAbility` by default), `AppendResetRules(...)`
  (host-level rules, e.g. the running timeline's).
- **`UGSTN_CommandBindingSet`** (DataAsset, was `UGSTN_CombatSlotBindingSet`): slots, bindings
  `{ Command; Requirements; Ability }`, reset rules. The combat model replaces it with its compiled loadout table (M3)
  through the same simulator calls.

### 5.2 Context and input sources

- **`UGSTN_InputContextProvider`** (abstract, `EditInlineNew`, held by the component): `GetFacingYaw()`,
  `AppendContextTags(FGameplayTagContainer&)`, `AppendFrameEvents(...)`, all from end-of-F-1 state.
  **`UGSTN_MoverInputContextProvider`** (default): facing and Mover tags from the tracker, `Input.Context.Grounded`
  / `.Airborne` / `.OnWall` from configurable mode-name lists, `Input.BufferEvent.Landed`.
- **`UGSTN_MoverSimStateTracker`** moves here (`GSTNInputNP/Mover/`, same class name and API).
- **`IGSTN_InputSource`** (was `IGSTN_CombatInputSource`), **`UGSTN_PlayerInputSource`**, **`UGSTN_AIInputSource`**:
  as M2 §4, reading buttons and the stick action from the `UGSTN_InputConfig`.

### 5.3 Abilities' view

- **`IGSTN_CommandSourceInterface`** (UInterface, implemented by every component that runs the input simulation):
  `GetActivationContext()`, `IsCommandButtonHeld(int32 Button)`, `GetAimRotation()`.
- **`FGSTN_ActivationContext`**, **`EGSTN_ActivationSource`** (`Command`, `Forced`, `Adopted`, `Other`) live here;
  the combat plugin's `UGSTN_GA_Base` finds the interface on the avatar, so it works with either component.

### 5.4 Standalone component

**`UGSTN_InputSimComponent`** (`UNetworkPredictionComponent`) for projects without the combat plugin: registers its
own NP model (`FGSTN_InputModelDef`, priority `PreKinematicMovers - 3`, before the ASC like the combat model), with
`FGSTN_InputCmd` and a sync state holding only `FGSTN_InputSimState`. Properties: input config, binding set, input
source (instanced), context provider (instanced), global reset rules. Executes requests in the ASC pre-tick
(`OnSimulationPreTick`), projects the context tags on the ASC, drains its outbox on the authority, implements
`IGSTN_CommandSourceInterface`.

A pawn uses **either** this component **or** the combat component (which embeds the same simulation); having both is a
validation error at BeginPlay (two models would read the same buttons).

## 6. Authoritative events

`UGSTN_SimEventSubsystem` (world subsystem, `GSTNInputCore`) is the stream of combat.md §5.6, generalised: same API
(`Broadcast`, `Listen(FGameplayTagQuery)`), same rule (outbox written in simulation, drained only on the authority
after `FinalizeFrame`). The input simulation publishes `Input.Event.CommandExecuted`; the combat plugin publishes its
`Combat.Event.*` through the same subsystem.

## 7. What stays in the combat plugin

Default command set and its tags, combat button tags, timelines, adoption, `Combat.Lock.Movement`, combat states,
hits, kits, loadouts. The combat component embeds `FGSTN_InputSimState` in its sync state, implements
`IGSTN_InputSimHost` (timeline windows, hit events, compiled table) and `IGSTN_CommandSourceInterface`, and uses
`FGSTN_InputCmd` as its input type.

## 8. Migration (do it now, before M2 continues)

The split is mostly moving files and renaming; M2's NP code is half written, so the sooner the better.

**Step S1 — `GSTNInputCore`** (*done* 2026-10-22; one commit, builds and tests green):

1. Create `Plugins/GSTNInput/GSTNInput.uplugin` and module `GSTNInputCore` (Build.cs as section 2, module class, log).
   Enable the plugin in `GASP_Giustino2.uproject`.
2. `git mv` the generic files of `GSTNCombatCore/Public|Private/Input/`, `GSTN_CombatCoreTypes.h` and the generic tests
   into the new module; apply the renames of section 4; `GSTNCOMBATCORE_API` → `GSTNINPUTCORE_API` in moved headers.
3. `GSTNCombatCore` keeps `GSTN_CombatButtonTags`, `GSTN_CombatCommandTags`, `UGSTN_StandardSlots` and their tests; it
   depends on `GSTNInputCore`; `GSTNCombat.uplugin` declares the `GSTNInput` plugin dependency.
4. Add the combat values to `Config/DefaultGame.ini` (section 4).
5. Build; run `Automation RunTests GSTNInput` and `GSTNCombat`: all the existing tests pass, split between the two
   prefixes. The plan said 58; the suite had 63 tests before the move, and after it 56 are `GSTNInput.Core.*` and 7 are
   `GSTNCombat.Core.*`. Run both prefixes with one command, `Automation RunTests GSTNInput+GSTNCombat`: two
   `RunTests` in one `-ExecCmds` can drop the second.

**Step S1b — priority criteria** (one commit, after S1): section 4.1. With the default list every existing resolver,
validation and standard-set test passes unchanged; new tests: the list reordered (specificity before tier) changes the
winner of a chord-versus-higher-tier case, a test-only criterion struct changes a result, an empty list falls back to slot
order.

**Step S2 — `GSTNInputNP`**: the rest of M2 lands in `GSTNInputNP` instead of `GSTNCombatNP` (updated M2 spec):
`FGSTN_InputCmd` (move the working-copy fields of `FGSTN_CombatInputCmd`), `FGSTN_InputSimState` (move `History`,
`Classifier`, `WatchedTagMask`, `LastActivatedFrame` and the request out of `FGSTN_CombatSyncState`), simulator, host
interface, sources, context provider, binding set, tracker move, standalone component. `UGSTN_CombatSimComponent`
embeds them. The combat component's `MoveAction`/`ButtonActions` move to the input config: **tell the user before
re-setting them on `BP_GASNPP_TestCharacter`**.

Redirects in `Config/DefaultEngine.ini`:

- Tracker: both old names → `/Script/GSTNInputNP.GSTN_MoverSimStateTracker` (update the existing entry, add one from
  `/Script/GSTNCombatNP.GSTN_MoverSimStateTracker`).
- No asset of `UGSTN_CombatInputConfig` exists yet (checked), so the input classes need no redirects.

Tools: `setup_gas_assets.py` and `gasnpp_test.py` keep `GSTN_CombatSimComponent` (the class stays); the input config
asset is created by the setup script; testing.md lists the new test prefixes.

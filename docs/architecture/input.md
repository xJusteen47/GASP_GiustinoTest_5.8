# Input plugin (`GSTNInput`)

Fighting-game style input for any Unreal project: designer-defined commands (button, direction, motion, chord, charge,
hold, release, mash, spin), time and hold buffering, reset rules, priority between commands, and command → GameplayAbility
bindings with context requirements. Simulated and rolled back by the Network Prediction fork when used with GAS-NPP.
Source: `Plugins/GSTNInput/`.

The combat plugin (`GSTNCombat`, [combat.md](combat.md)) is built on top of it. This file describes what the input
plugin owns, what it leaves to its host, and how the code moves out of `GSTNCombat` (section 8). The input behaviour
itself (classifier, history, grammar, buffer, priority, bindings) is specified in combat.md §6–§9 and in the M1/M2
specifications; their rules do not change, only where the code lives and some names.

Status (2026-10-22): step S1 is **done**: `GSTNInputCore` exists with the generic M1 code (section 8 lists what differs from the plan). Step S1b (priority criteria, section 4.1) is **done**. Step S2a is **done**: the module `GSTNInputNP` exists with `FGSTN_InputCmd`, `FGSTN_InputSimState` and the Mover tracker. Step S2b is **written, not yet built or tested in the editor**: `FGSTN_InputSimulator` + `IGSTN_InputSimHost`, `UGSTN_CommandBindingSet` (with the runtime `FGSTN_CommandTable`), the player and AI input sources, the context provider base and the Mover provider; `UGSTN_CombatSimComponent` is the first host (`SlotBindings` replaces `ButtonAbilities`). S2c-S2e (locks, activation-context API, event subsystem, standalone component) are still design.

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

**Dependency rule.** An `*NP` module may depend on another plugin's `*NP` module of the same layer, and on nothing above it.
`GSTNCombatNP` depends on `GSTNInputNP` by design: the combat NP model embeds the input simulation, one NP model per
character. Core modules keep no NP, GAS or Mover dependencies, so shared NP types are not moved into `GSTNInputCore`.
`GSTNCombatNP` may use only the Public headers and interfaces of `GSTNInputNP` (for example `IGSTN_InputSimHost`), never
its internals.

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

Game-specific buttons, commands and their per-root lock tags live in **`Config/Tags/GSTNCommands.ini`** of the *game
project* (replaces the planned `GSTNCombatCommands.ini`, which was never created): the one exception to the project's
native-tags rule. The file belongs to the project, not to a plugin: the plugins ship no designer tags. The sample project's
file exists and holds the test buttons and commands of the PIE harness (`Input.Button.Dash/Buff`, `Command.Dash/Buff`).

**Ownership exception.** `Input.Button` is a root of the input plugin, and the combat plugin declares the standard button
leaves (`Input.Button.Attack`...) natively under it (`GSTN_CombatButtonTags.h`). Declaring leaves under another plugin's
root is the one place where the "one owner per root" rule is bent: the leaves are combat vocabulary, the root is the filter
every button tag picker uses, and a rename to `Combat.Button.*` would break that filter and every asset that names the tag.
Open owner decision (independence audit point 4): keep the exception, or rename to a combat root.

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
section 3, except that the per-root lock tags are declared with their roots (the combat ones in `GSTNCombatCore`, S2c).

### 4.1 Priority criteria (*Decision*, 2026-10-19; implemented in S1b, 2026-10-22)

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
- **As implemented** (`GSTN_PriorityCriteria.h`): `FGSTN_PriorityContext` holds the slot spec, its pattern and the settings (references, valid for
  the call). The three built-ins are `FGSTN_PriorityCriterion_Tier`, `_Specificity` and `_ManualOffset`.
  `UGSTN_InputSettings::MakeDefaultPriorityCriteria()` builds the default list and the constructor uses it, so a project
  with no ini entry behaves as before. `FGSTN_InputSlotSpec::GetPriority` scores the first `GSTNPriority::MaxCriteria` (8)
  entries; an entry left empty in the editor scores 0 so every slot has one score per listed criterion. `FGSTN_CommandPriority::Compare`
  reads a shorter vector as zero padded, which keeps the order total. The `Tier`, `Specificity` and `Offset` fields are gone:
  code that read them reads `Scores[i]` instead. Scoring is not cached yet: the resolver still calls `GetPriority` per
  candidate per frame (cheap, no allocation); a per-slot-set cache is a later optimisation, not a behaviour change.
- **Frame-dependent ordering** (which press is newer) stays in the press-group stage, configured by
  `BufferConflictPolicy` and `SimultaneousFrames`; it runs before the criteria, as now.

Generic code that names combat content today, to be cleaned in the move:

- **Settings defaults**: `CommandTiers` and `ExclusiveButtons` start empty in `UGSTN_InputSettings`. The combat tiers
  (Attack 100, Strong 200, Special 300, Combo 350, Super 400) and exclusive buttons (Attack, Strong, Special) were first put in
  the project's `Config/DefaultGame.ini`; they now live in the combat plugin's own settings, see §4.2. combat.md §8.2 lists them
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
  (`GSTN_CombatTestUtils.h`). The combat defaults test reads the settings of the running project, so it also checks that the
  combat plugin's config file (`Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`) loads and is contributed to the input settings.
- Comments that cite combat.md sections keep citing them (the rules live there) but say "combat.md" explicitly.

### 4.2 Independence and extensibility (done 2026-10-22, from the independence audit)

Each plugin must work in a project that knows nothing about this one:

- **Own config files.** `UGSTN_InputSettings` is `config = GSTNInput, defaultconfig`: defaults in
  `Plugins/GSTNInput/Config/DefaultGSTNInput.ini` (the class defaults written out, plus the `[CoreRedirects]` for the
  Mover tracker's old class names), project overrides in the project's `Config/DefaultGSTNInput.ini`. Nothing is read from
  `DefaultGame.ini`. The combat plugin has `UGSTN_CombatSettings` (`GSTNCombatCore`, `config = GSTNCombat`) with `CommandTiers`
  and `ExclusiveButtons` and `Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`.
- **Contributions.** The combat module's startup (and a change of its settings in the editor) calls
  `UGSTN_InputSettings::SetContribution("GSTNCombat", tiers, buttons)`; `GetCommandTier` and `IsExclusiveButton` (used by the
  resolver) look at the settings' own lists and at every contribution, combined independently of order (own list wins on the
  same tag, otherwise the highest tier; any source makes a button exclusive). A contribution is not config: it lives on the
  settings default object only, so a transient settings object in a test starts without any, and nothing is read from it
  per frame beyond the same lookups as before.
- **Optional engine/fork plugins.** The `.uplugin` files mark GameplayAbilities, AbilitySystemSimulation, NetworkPrediction
  and Mover as `Optional`; EnhancedInput stays required (the Core module links it privately). The NP modules still need the
  fork to build, a full split into a sub-plugin is not done.
- **Soft references.** `Action`/`MoveAction` (`TSoftObjectPtr<UInputAction>`), `SlotSets` (`TSoftObjectPtr`), `Ability`
  (`TSoftClassPtr<UGameplayAbility>`), and the hosts' `InputConfig`/`SlotBindings`. Resolved once at setup: the host in
  `BeginPlay` (`CompileInput`), `UGSTN_InputConfig::ResolveActions` (the input source reads `GetButtonAction`/`GetMoveAction`,
  never a soft pointer), and `UGSTN_CommandBindingSet::BuildTable` (slot sets kept alive in a transient array, abilities into
  `FGSTN_CompiledBinding::Ability`, which stays a plain `TSubclassOf`). Nothing is loaded in a tick.
- **Blueprint wrappers (opt-in, the slower path).** `Public/Input/GSTN_BlueprintRules.h`: `FGSTN_InputCondition_Blueprint`,
  `FGSTN_PriorityCriterion_Blueprint` and `FGSTN_BufferResetRule_Blueprint` each hold a `TSubclassOf` of an abstract
  Blueprintable `UObject` (`UGSTN_InputConditionBlueprint::Matches`, `UGSTN_PriorityCriterionBlueprint::Score`,
  `UGSTN_BufferResetRuleBlueprint::ShouldFlush`) with a const `BlueprintNativeEvent` that takes a plain-value view struct
  (`FGSTN_BlueprintConditionView` / `PriorityView` / `ResetView`) and returns the result. The wrapper calls the class
  default object, so the rule can keep no state: it must be a function of the view (no world queries, no state outside its
  inputs), like every C++ rule. The C++ structs stay the default. A Blueprint condition is never an anchor and offers no
  canonical input (slot validation cannot compare it); its specificity and referenced buttons are properties of the
  wrapper. Tests: `GSTNInput.Core.Input.Blueprint.*`.
- **Other Blueprint exposure.** `UGSTN_BlueprintInputContextProvider` (§5.2); `UGSTN_MoverSimStateTracker` is `BlueprintType`
  with `BlueprintPure` queries; `UGSTN_CombatSimComponent` is `Blueprintable`; `UGSTN_CommandBindingUtils::AreRequirementsSatisfied`,
  `UGSTN_InputSettings::GetCommandTier`/`IsExclusiveButton` and `UGSTN_InputConfig::FindButtonIndex` are `BlueprintPure`. Tag
  fields that hold state tags of any root (`FGSTN_TagRequirements`, `OnTagAdded`, the provider's `MoverTags`) carry no
  `Categories` filter on purpose and say so in their comment; the others have one. Not done: BlueprintNativeEvent on the combat
  component itself (its simulation hooks run every tick and stay C++; the provider, the sources and the rule families are the
  Blueprint points).

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
- **`UGSTN_MoverSimStateTracker`** lives here (`GSTNInputNP/Mover/`, same class name and API; moved in S2a).
- **`IGSTN_InputSource`** (was `IGSTN_CombatInputSource`), **`UGSTN_PlayerInputSource`**, **`UGSTN_AIInputSource`**:
  as M2 §4, reading buttons and the stick action from the `UGSTN_InputConfig`.

**Blueprint provider.** `UGSTN_BlueprintInputContextProvider` (`Context/`) is a Blueprintable provider whose C++ virtuals
route to const `BlueprintNativeEvent`s (`GetBlueprintFacingYaw`, `GetBlueprintContextTags`, `GetBlueprintContextFingerprint`,
`GetBlueprintFrameEvents`) plus a `BlueprintImplementableEvent` `Initialize` for BeginPlay. It is the slower path (the
Blueprint VM runs every simulated tick) with the same determinism rules as the rule wrappers (§4.2); the base class and
`UGSTN_MoverInputContextProvider` stay plain C++.

### 5.3 Abilities' view

- **`IGSTN_CommandSourceInterface`** (UInterface, implemented by every component that runs the input simulation):
  `GetActivationContext()`, `IsCommandButtonHeld(int32 Button)`, `GetAimRotation()`.
- **`FGSTN_ActivationContext`**, **`EGSTN_ActivationSource`** (`Command`, `Forced`, `Adopted`, `Other`) live here;
  the combat plugin's `UGSTN_GA_Base` finds the interface on the avatar, so it works with either component.

### 5.4 Standalone component

**`UGSTN_InputSimComponent`** (`UNetworkPredictionComponent`) for projects without the combat plugin: registers its
own NP model (`FGSTN_InputModelDef`, priority `PreKinematicMovers - 4`, before the ASC like the combat model, see §5.7), with
`FGSTN_InputCmd` and a sync state holding only `FGSTN_InputSimState`. Properties: input config, binding set, input
source (instanced), context provider (instanced), global reset rules. Executes requests in the ASC pre-tick
(`OnSimulationPreTick`), projects the context tags on the ASC, drains its outbox on the authority, implements
`IGSTN_CommandSourceInterface`.

A pawn uses **either** this component **or** the combat component (which embeds the same simulation); having both is a
validation error at BeginPlay (two models would read the same buttons).

### 5.5 As implemented in S2b (differences from the plan above)

- `FGSTN_InputSimulator::Tick(FGSTN_InputTickArgs, const IGSTN_InputSimHost&)` runs steps 1-4, 6, 7 (no locks, no outbox yet). Args carry the table, button map, settings, the host's global reset rules and the hitstop window (`PausedStart/End`); the result gives the pressed mask and the requested command/binding.
- `IGSTN_InputSimHost` has `GetFacingYaw`, `GetOwnerTags`, `AppendContextTags`, `GetContextFingerprint`, `AppendFrameEvents(prev, cur, events)`, `IsCommandAllowed`, `CanExecuteBinding`. The provider's `GetContextFingerprint` (a small context class, not a mode-name hash) replaces the "previous mode name hash": `FGSTN_InputSimState::PrevContext`. `AppendResetRules` was dropped: the host's global rules are an argument, the binding set's rules live in the table.
- `FGSTN_InputSimState` renamed `RequestedAction` to `RequestedBinding` and gained `RequestButton` and `PrevContext`.
- `FGSTN_CommandTable` (built by `UGSTN_CommandBindingSet::BuildTable`): commands sorted by tag name, bindings by (command, declaration order), candidate slots (commands with a binding), sorted watched tags. The binding set also has `SlotSets` (slot sets included before `Slots`).
- `IGSTN_InputSource::ProduceInput(FGSTN_InputSourceContext, DeltaMs, Out)`; the stick action is `UGSTN_InputConfig::MoveAction` (added), not a property of the source. The AI stick is sent with camera yaw 0.
- The Mover provider lists mode names (`GroundedModes`, `AirborneModes`, `WallModes`) and the Mover tags to copy (`MoverTags`; Mover cannot enumerate tags). Projecting the context tags onto the ASC (combat.md section 5.4) is not done yet.
- Combat component properties: `InputConfig`, `SlotBindings`, `ResetRules`, `InputSource`, `ContextProvider`; `ButtonActions`, `MoveAction` and `ButtonAbilities` are gone (the Blueprint must be re-set; `setup_gas_assets.py` still sets `MoveAction` and needs updating).

### 5.7 Network Prediction sort priorities

Network Prediction checks sort priorities across every registered model and warns "ModelDefs X and Y have same sort priority"
when two share one (it falls back to a lexical sort, which is deterministic but accidental). The ability system is
`PreKinematicMovers - 2` (48), Mover 50. The combat model `GSTN_Combat` is `- 3` (47) and the standalone input model
`GSTN_Input` is `- 4` (46): they are never on the same pawn, but the check is global, so each model of this plugin family has its own
number. Both stay before the ability system, which is what matters (a request of a frame is ready when the ASC ticks). A new
model of the family takes the next free number below.

### 5.6 As implemented in S2c-S2e (differences from the plan above)

- **Locks.** `UGSTN_CommandLocks` (`GSTNInputCore`, `Input/GSTN_CommandLocks.h`): `Input.Lock.Command` (checked as an **exact** tag: a container holding `Input.Lock.Command.Attack` also answers to its parent `Input.Lock.Command`, which would make every per-root lock global) stops everything; `Input.Lock.Command.<path>` locks `Command.<path>` and everything below. The mapping is worked out once per command when the table is built (`FGSTN_CommandTable::FCommand::LockTags`: the lock tag of the command and of each ancestor under `Command`, only those that are registered tags), so the per-frame check is a tag intersection (`FGSTN_CommandTable::IsCommandLocked`). The simulator skips matching on the global lock (presses stay buffered, a forced command is refused) and treats a locked command as unavailable (it reserves its press). The lock tags are read from the owner's end-of-previous-frame tags. The combat plugin declares the native lock tags of its roots (`GSTNCombatLockTags`: Attack, Strong, Special, Combo, Super); Movement, Weapon and Link get theirs when those roots exist.
- **Unbound commands** were already excluded from `CandidateSlots` in S2b; S2c adds the test that proves an unbound command sorting first does not steal a press.
- **Activation context.** `Simulation/GSTN_CommandSourceInterface.h`: `EGSTN_ActivationSource`, `FGSTN_ActivationContext` (`Button` is an `int32`, -1 for none, so Blueprint can hold it), `IGSTN_CommandSourceInterface` (`GetActivationContext`, `IsCommandButtonHeld`, static `Find(Actor)`). `GetAimRotation` is not there: the command has no aim fields yet. `RequestButton` was already in the state (S2b); S2c gives it a third value, `FGSTN_InputSimState::CommandWithoutButton` (255), for a command whose match has no terminal press, because 0 means a forced command. The context is built by the pure `FGSTN_ActivationContext::FromRequest(State, Table, Frame)`; `PressFrame` is the newest press of the trigger button at or before the request in the history (the terminal press, unless the same button was pressed again within the buffer), so no extra sync state is needed. Hosts read the sync state with the **simulation** read, so the answer is the state of the frame the ability system is ticking.
- **Events.** `GSTNInputCore/Events/`: `FGSTN_SimEvent`, `FGSTN_SimEventOutbox` (capped at 64, oldest dropped), `FGSTN_SimEventDispatcher` (plain C++, the matching and listener bookkeeping, tested without a world) and `UGSTN_SimEventSubsystem` (`Broadcast`, `Listen` with a dynamic delegate, `ListenNative`, `StopListening`). A listener's query is matched against the event's tags plus its type; an empty query takes everything. The simulator appends `Input.Event.CommandExecuted` to `FGSTN_InputTickArgs::Outbox` (command tag, owner and context tags, the ability CDO's asset tags; no role branch), and the host calls `FGSTN_InputHostUtils::PublishOutbox` from `FinalizeFrame`: published only when `Owner->HasAuthority()`, emptied either way. Authority is never inferred from the absence of a local player, so a listen-server host's own pawn publishes.
- **Host helpers.** `FGSTN_InputHostUtils` (`Simulation/GSTN_InputHostUtils.h`) holds what both hosts do the same way: `CanExecuteBinding`, `ExecuteRequest` (from the ability system's pre-tick), `IsButtonHeld`, `PublishOutbox`. The combat component now uses it too.
- **Standalone component.** `UGSTN_InputSimComponent` (`Simulation/GSTN_InputSimComponent.h`): model `GSTN_Input` (sort priority `PreKinematicMovers - 4`, see §5.7), sync state `FGSTN_InputModelSyncState` = `FGSTN_InputSimState`, no aux state. Properties like the combat component's (`InputConfig`, `SlotBindings`, `ResetRules`, `InputSource`, `ContextProvider`). With another component that implements `IGSTN_CommandSourceInterface` on the same pawn it logs an error at BeginPlay and stays inert (it still registers its model, because the base class registers it when the component initialises, but it produces no input, ticks nothing and executes nothing); the check depends only on the owner's components, so it is the same on every machine. Not done: projecting `Input.Context.*` onto the ability system (also missing in the combat component), and the Blueprint wrappers, which are now done (§4.2, §5.7).

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
4. Add the combat values to the config (done first in `Config/DefaultGame.ini`, then moved into the combat plugin's own settings, §4.2).
5. Build; run `Automation RunTests GSTNInput` and `GSTNCombat`: all the existing tests pass, split between the two
   prefixes. The plan said 58; the suite had 63 tests before the move, and after it 56 are `GSTNInput.Core.*` and 7 are
   `GSTNCombat.Core.*`. Run both prefixes with one command, `Automation RunTests GSTNInput+GSTNCombat`: two
   `RunTests` in one `-ExecCmds` can drop the second.

**Step S1b — priority criteria** (done; one commit, after S1): section 4.1. With the default list every existing resolver,
validation and standard-set test passes unchanged; new tests: the list reordered (specificity before tier) changes the
winner of a chord-versus-higher-tier case, a test-only criterion struct changes a result, an empty list falls back to slot
order.

**Step S2a — the module `GSTNInputNP`** (done, no behaviour change): `FGSTN_CombatInputCmd` became `FGSTN_InputCmd` and the history, classifier, watched-tag mask, request and `LastActivatedFrame` of `FGSTN_CombatSyncState` became `FGSTN_InputSimState` (`Public/Simulation/GSTN_InputSimTypes.h`); the combat sync state embeds it as `Input` and keeps the hitstop window. The wire format and the reconcile checks are unchanged. The request fields keep their M0 names (`RequestedAction`; `RequestedBinding` and `RequestButton` come with S2b/S2c). The tracker moved with `git mv` and exports `GSTNINPUTNP_API`. `GSTNCombatNP` and the project module depend on `GSTNInputNP`.

**Step S2 — `GSTNInputNP`**: the rest of M2 lands in `GSTNInputNP` instead of `GSTNCombatNP` (updated M2 spec):
`FGSTN_InputCmd` (move the working-copy fields of `FGSTN_CombatInputCmd`), `FGSTN_InputSimState` (move `History`,
`Classifier`, `WatchedTagMask`, `LastActivatedFrame` and the request out of `FGSTN_CombatSyncState`), simulator, host
interface, sources, context provider, binding set, tracker move, standalone component. `UGSTN_CombatSimComponent`
embeds them. The combat component's `MoveAction`/`ButtonActions` move to the input config: **tell the user before
re-setting them on `BP_GASNPP_TestCharacter`**.

Redirects (added in S2a in `Config/DefaultEngine.ini`; now in the input plugin's `[CoreRedirects]`):

- Tracker (the entries moved to `Plugins/GSTNInput/Config/DefaultGSTNInput.ini`, §4.2): both old names → `/Script/GSTNInputNP.GSTN_MoverSimStateTracker` (update the existing entry, add one from
  `/Script/GSTNCombatNP.GSTN_MoverSimStateTracker`).
- No asset of `UGSTN_CombatInputConfig` exists yet (checked), so the input classes need no redirects.

Tools: `setup_gas_assets.py` and `gasnpp_test.py` keep `GSTN_CombatSimComponent` (the class stays); the input config
asset is created by the setup script; testing.md lists the new test prefixes.

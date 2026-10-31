# Input plugin (`GSTNInput`)

Fighting-game style input for any Unreal project: designer-defined commands (button, direction, motion, chord, charge,
hold, release, mash, spin), time and hold buffering, reset rules, priority between commands, and command → GameplayAbility
bindings with context requirements. Simulated and rolled back by the Network Prediction fork when used with GAS-NPP.
Source: `Plugins/GSTNInput/` (`GSTNInputCore`) and `Plugins/GSTNInputNP/` (`GSTNInputNP`, the Network Prediction adapter).

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

**Two plugins.** `GSTNInput` holds `GSTNInputCore` only; its descriptor lists just EnhancedInput, so it works in a project without the GAS-NPP fork. `GSTNInputNP` is a separate plugin (`Plugins/GSTNInputNP/`, module name unchanged, so `/Script/GSTNInputNP.*` paths stay valid) that depends on `GSTNInput`, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, EnhancedInput and Mover (all non-optional there). Prefix stays `GSTN_`; export macros `GSTNINPUTCORE_API`, `GSTNINPUTNP_API`; log category `LogGSTNInput`.

`GSTNCombat` declares a plugin dependency on `GSTNInput` only; `GSTNCombatNP` is its own plugin (`Plugins/GSTNCombatNP/`) that depends on `GSTNCombat`, `GSTNInput` and `GSTNInputNP`. `GSTNCombatCore` depends on `GSTNInputCore`, `GSTNCombatNP` on `GSTNInputNP`.

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
  `Plugins/GSTNInput/Config/DefaultGSTNInput.ini` (the class defaults written out; the `[CoreRedirects]` for the
  Mover tracker's old class names are in `Plugins/GSTNInputNP/Config/DefaultGSTNInputNP.ini`), project overrides in the project's `Config/DefaultGSTNInput.ini`. Nothing is read from
  `DefaultGame.ini`. The combat plugin has `UGSTN_CombatSettings` (`GSTNCombatCore`, `config = GSTNCombat`) with `CommandTiers`
  and `ExclusiveButtons` and `Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`.
- **Contributions.** The combat module's startup (and a change of its settings in the editor) calls
  `UGSTN_InputSettings::SetContribution("GSTNCombat", tiers, buttons)`; `GetCommandTier` and `IsExclusiveButton` (used by the
  resolver) look at the settings' own lists and at every contribution, combined independently of order (own list wins on the
  same tag, otherwise the highest tier; any source makes a button exclusive). A contribution is not config: it lives on the
  settings default object only, so a transient settings object in a test starts without any, and nothing is read from it
  per frame beyond the same lookups as before.
- **Exclusive context groups (binding validation).** `UGSTN_InputSettings::ExclusiveContextGroups` is a list of
  `FGSTN_ExclusiveTagGroup` (`Tags`: a context holds at most one of them). The plugin ships `Input.Context.Grounded` +
  `Input.Context.Airborne` as one group in `Config/DefaultGSTNInput.ini`; a game or plugin adds the groups of its own movement
  states, directly in the settings or through the optional trailing argument of `SetContribution`.
  `GetAllExclusiveContextGroups()` returns the settings' groups plus every contribution's (a set: order is irrelevant).
  Validation only (editor time, the simulation never reads them): `FGSTN_TagRequirements::CanHoldTogether(Other, Groups)` is also
  false when a group holds a tag this requires and a different one Other requires, so two bindings of a command that need
  Grounded and Airborne are not reported as an arbitrary choice; `UGSTN_CommandBindingUtils::FindBindingConflicts(Bindings, Groups)`
  takes the groups (default empty = the old behaviour) and `FindImpossibleBindings` reports a binding requiring two tags of one
  group. `UGSTN_CommandBindingSet::IsDataValid` passes the combined groups of the settings. Members match exactly, not through
  the tag hierarchy. The validator never names a project tag.
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
  `ForcedCommand` and the optional camera aim `bHasAim`/`AimYaw`/`AimPitch` (uint16, 65536 steps per turn, `FGSTN_DirectionClassifier::QuantizeAim`; sent by the player source when `UGSTN_InputConfig::bSendCameraAim` is on and by the AI source after `SetAim`; one wire bit when absent, two words in full when present, no delta against the previous command because the NP input buffer does not offer it; a repeated input keeps it); `NetSerialize`, `ToString`, `PrepareRepeatedInput`. A host model uses it
  directly as its NP input type.
- **`FGSTN_InputSimState`**: `History`, `Classifier`, `WatchedTagMask`, the frame-stamped request (`RequestedBinding`,
  `RequestButton`, `RequestFrame`), `LastActivatedFrame`. Not a sync state by itself: helpers `NetSerialize(Ar)`,
  `ShouldReconcile(Other)`, `ToString(Builder)` that the host's sync state calls for its embedded member.
  The input state is **authoritative**: `ShouldReconcile` is true on the first difference (the trace names the field in
  `np.PrintReconciles`) and the host must return it from its own `ShouldReconcile`; discarding it would let the
  rolled back history drift from the server's without a correction.
- **`FGSTN_InputSimulator`** (stateless): `Tick(const FGSTN_InputTickArgs&, IGSTN_InputSimHost&)` runs M2 §5 steps
  1–7 for one frame (sample, history, watched tags, reset rules, locks, resolve, request, consumption, outbox entry) on
  the embedded state.
- **`IGSTN_InputSimHost`** (plain C++ interface, not a UInterface; the host implements it):
  `AppendFrameEvents(FGSTN_BufferFrameEvents&)` (extra reset events such as hits),
  `IsCommandAllowed(int32 CommandIndex)` (the host's own gating, e.g. combat timeline and cancel windows),
  `CanExecuteBinding(int32 BindingIndex)` (ASC `CanActivateAbility` by default), `AppendResetRules(...)`
  (host-level rules, e.g. the running timeline's).
  **The answer of `CanExecuteBinding` decides whether a press is consumed**, which is rolled back state, so it must be the
  same on the client and the server: `CanActivateAbility` overrides of bound abilities must not read the role, local control
  or non rolled back state, and `ServerOnly` / `ServerOnlyExecution` abilities cannot be bound (the engine's
  `ShouldActivateAbility(GetLocalRole())` refuses them on a client). `UGSTN_CommandBindingSet::IsBindableAbility` is the check:
  `BuildTable` drops and logs such bindings, `IsDataValid` reports them. TODO (fork): a role free activation check would lift
  the restriction.
- **Stick-only commands consume their anchor.** A terminal motion or spin has no press, so `FGSTN_InputHistory` keeps the
  newest consumed stick anchor frame (`ConsumeSticksUpTo`, set by the simulator through `FGSTN_SlotResolution::ConsumeFrom`);
  the matcher ignores terminal stick anchors at or before it. Wire: one optional byte in the history (flag 0x20 of the first
  header byte, the frame's age), compared in `operator==`, aged out like an event. Tests `GSTNInput.Core.Input.Resolver.StickOnly*`,
  `...History.ConsumedStickAnchor`.
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

- Tracker (the entries live in `Plugins/GSTNInputNP/Config/DefaultGSTNInputNP.ini`, §4.2): both old names → `/Script/GSTNInputNP.GSTN_MoverSimStateTracker` (update the existing entry, add one from
  `/Script/GSTNCombatNP.GSTN_MoverSimStateTracker`).
- No asset of `UGSTN_CombatInputConfig` exists yet (checked), so the input classes need no redirects.

Tools: `setup_gas_assets.py` and `gasnpp_test.py` keep `GSTN_CombatSimComponent` (the class stays); the input config
asset is created by the setup script; testing.md lists the new test prefixes.

## 9. Planned: keyboard shortcuts for stick gestures (owner, 2026-10-25)

Owner idea: on keyboard the spin, a stick rotation, gets a **shortcut such as A-S-D without W**, a partial rotation that is easy to
roll with three fingers. *Proposal*, not built:

- The `Spin` condition already counts rotation in degrees with a sense. A slot gains `DigitalMinDegrees` (default: its normal
  threshold), used when the direction samples in the window came from **digital** directions. A-S-D is left, down, right: 180°
  counterclockwise seen from above with forward at the top. D-S-A is the clockwise one. Eight-way samples (A, A+S, S, S+D, D) feed
  the same accumulator.
- The input source writes one bit per frame, `bDigitalDirection`, from the device that last changed the direction (keyboard or
  d-pad). The bit is part of the input frame, so history, prediction and rollback see the same value on every machine; the
  simulation never asks which device is plugged in.
- The same per-slot threshold serves other gestures that are hard on keys. A dedicated spin button stays possible
  ([spin-drill.md](spin-drill.md) §5).
- Questions KB.1–KB.2 in [open-questions-m4-m10.md](open-questions-m4-m10.md).

## 10. Charge inputs, Guile style (owner question, 2026-10-26)

The owner asked whether a tag added on crouch press and removed on release, read by an ability, is enough for Street Fighter charge
moves. **The plugin already has it, in a better place**: `FGSTN_InputCondition_Charge` (`GSTNInputCore`, test `ConditionCharge`) reads
the **input history**, which is part of the predicted state, so a resimulation after a rollback gets the same answer. A tag alone has
no duration, and a tag derived from Mover's crouch would be seen one frame late and is a Mover query (CLAUDE.md rules 1 and 5).

What the condition does today:
- `Directions`: a **set** of directions that all count as charging (for example back and down-back), in a chosen `Space`
  (`Facing` by default).
- `MinFrames` (default 30): how long the stick must stay inside the set. Standard charge moves use **40 frames** (about 0.65 s) and
  `MaxReleaseFrames` 8, set per move (owner answer CH.2, 2026-10-26).
- `MaxReleaseFrames` (default 8): the **tolerance after release**: the stick may have left the set at most this many frames before the
  move's anchor, so "charge back, then forward + punch" works.
- `MinZone`: how far the stick must be tilted to count.
- A charged **button** is `Release` with `MinHeldFrames`; a move that loops while a button is held is the timeline's `Charge` track.
- Charging is input only, so it continues while the character walks, crouches, blocks or attacks, as in Street Fighter.

Gaps, *Proposal*:
1. **The history keeps the last 16 direction events** (`FGSTN_InputHistory::MaxEvents`). A new event is recorded whenever the stick
   sector, the zone, the facing or the camera yaw changes the stick's direction, so a charge held while the camera turns, or while
   the stick wobbles between back and down-back, can push the start of the run out of the ring: the charge then looks shorter than it
   was and the move fails. Fix: a small **charge tracker** in the input state of `GSTNInputNP`. For each charge slot declared by the
   command set (at most 4), it keeps `uint8 AnchorAngle` (256 steps, item 2), `uint8 HeldFrames` (saturating at 255) and
   `uint8 FramesSinceRelease`, updated every frame from the current stick. It is part of the predicted input state, so rollback
   replays it; cost 12 bytes. The `Charge` condition reads the tracker when its slot is declared and falls back to the event scan
   otherwise.
2. **"Back" in 3D (owner answer CH.1, 2026-10-26 09:53)**: the back-charge works in 3D; the charge holds **as long as the stick stays
   within a limited angle**. In free movement the character turns towards the stick, so "back relative to facing" cannot be held;
   instead the charge is relative to **where it started**. *Proposal*: a new space `EGSTN_DirectionSpace::ChargeAnchor`. When the stick
   enters the tilt zone with no charge running, the tracker stores the stick's angle in `Stick` space (the physical stick, so a camera
   turn does not break the charge) as `AnchorAngle`. The charge keeps counting while the stick stays within `MaxDeviationDeg` of the
   anchor (50°, per slot, owner answer CH.4); past it the charge restarts with a new anchor. **The anchor drifts with time**
   (owner, 09:55): each frame it moves towards the stick by at most `AnchorDriftDegPerSecond` (default **45°/s**, a full turn in 8 s;
   per slot, 0 = no drift; owner CH.4 10:23: 90°/s is too fast for some moves, so a move that needs another rate declares its own
   slot), so a
   stick turned very slowly keeps the charge, even through a full 360°, while a quick turn breaks it. The release reads the anchor
   of the frame it happens on. The release step of the move reads
   its direction in the same space: "forward" means **opposite to the anchor** within its own tolerance, so "hold one way, then push
   the other way + punch" fires the move towards the push. Where facing is locked (the Duel camera, a lock-on) the classic `Facing`
   set (back and down-back) keeps working. Down-charges (crouch held) need neither.
   **Which way the move faces (owner, 09:54)**: while charging, the character usually faces the charge direction, so a move may also
   have its **hitboxes behind** the character instead of in front. That is the timeline's choice, not the input's: `AimPolicy =
   KeepFacing` with hitboxes authored behind the root (negative X in `Root` space) hits towards the push without turning;
   `SnapToStick` turns towards the push and hits forward (owner, 09:55: "it depends on the ability"). Both are data on the move
   (combat.md §6.3, `AimPolicy`).
3. **Side switches (CH.3, inferred from the owner's answers, 2026-10-26 09:56)**: standard charge moves use `ChargeAnchor` in the
   duel too, so the charge does not depend on facing and a cross-up does not erase it. The move still fires opposite to the anchor;
   whether it then turns towards the opponent is the move's aim policy, like any other charge move. A move that wants the Street
   Fighter rule (charge lost on a side switch) uses the `Facing` set instead; that is data on the command.

**Charged buttons (owner question, 2026-10-26 10:25 and 10:26; Cody's Zonk Knuckle and EX Zonk Knuckle)**: hold a normal attack
button for a while, release it to fire the special. Reference behaviour (Street Fighter IV data; Street Fighter V keeps the idea):
the plain Zonk charges on **any one punch** and has **three levels by hold time** (about 60, 90 and 120 frames: more damage and a
longer upper-body invulnerability); the **EX Zonk** charges on **two punches held together**, released together, about 60 frames,
costs EX meter, hits twice and is fully invulnerable during startup.

What exists today: `FGSTN_InputCondition_Release` with `MinHeldFrames` (the negative edge; `MaxHeldFrames` gives the tap slot on the
same button, and a tap slot and a hold slot never match the same release). The release does not need an unconsumed press, so the
press can fire a normal attack and the hold keeps charging through other moves, walking or blocking, as in Street Fighter. A release
during another move is buffered like any command. The EX meter fall-through already exists in the resolver (M3 C17–C18): without
meter, a command marked EX falls through to its plain version. Gaps, *Proposal*:
1. **The 16-event button ring**: if the press of the held button drops out of the ring, `Release` counts the hold as longer than the
   history (`MAX_int32`), which is right for a long hold but wrong when the press was pushed out early by **other button presses**:
   eight presses and releases of other buttons while charging push it out, and a 120-frame charge could fire after 30 frames. Fix:
   the charge tracker of gap 1 gets **button slots**: `uint8 HeldFrames` (saturating at 255, about 4 s) of the current press, one byte
   per declared button, up to 4 (so the three punches of a Zonk character fit next to its stick slots; tracker 12 + 4 = 16 bytes),
   predicted and rolled back. `Release`, `Hold` and the chord below read the slot when the button has one, and keep the event scan
   otherwise. Each button charges on its own, so releasing one of two held punches fires the plain Zonk and the other keeps charging.
2. **Any one of several buttons**: `Release` gets an optional `AlsoButtons` set (any one of them counts; the default stays one
   button), so one command covers "any punch" instead of three slots.
3. **Charge levels by hold time**: one command per level with `MinHeldFrames`/`MaxHeldFrames` bands (60–89, 90–119, 120+), each
   pointing at its own timeline or at one timeline with level data. The bands never overlap, so one release matches one level.
4. **EX: several buttons released together** (owner, 10:26): a new `ReleaseChord` condition, the mirror of `Chord`: every listed
   button (or any `Count` of a set, for "two of the three punches") released within `WithinFrames` (default 3) of each other, each held
   at least `MinHeldFrames`. Anchor = the newest release. Specificity 10 per button, so the EX chord (20) wins over a plain `Release`
   (10) on the same frames, as `Chord` does for presses. The EX command is marked EX, so without meter it falls through to the plain
   Zonk of the level the hold reached. Releases are never consumed, so the plain `Release` slots must not fire on the same
   releases: the resolver's existing one-activation-per-frame rule handles the same frame, and the plain slot's `MaxReleaseLagFrames`
   (new, default 0) stops it from firing a frame later from the second button of the chord.
5. **"Charge ready" feedback** (a flash per level): presentation reads the slots' `HeldFrames`; no simulation change.
6. The charge stays ready as long as the button is held (no automatic release). A move that fires by itself after a maximum hold uses
   a `Hold` condition with that `MinFrames` instead, or the timeline's `Charge` track (loop while held, Smash style).

Questions CH.1–CH.4 in [open-questions-m4-m10.md](open-questions-m4-m10.md); all answered. CB.1 answered (two-button release needed for EX).

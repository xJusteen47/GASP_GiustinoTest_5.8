# GSTNCombat

Fighting-game style combat content for Unreal Engine 5.8, built on [GSTNInput](../GSTNInput/README.md): the standard
button and command tags, the combat lock tags, the default command set and the combat settings. This plugin is
`GSTNCombatCore` only: pure and deterministic, it works without the GAS-NPP fork. Simulating combat with Network
Prediction (the combat NP model, the ability system driven from it) is the separate plugin
[GSTNCombatNP](../GSTNCombatNP/README.md).

Status: **experimental (0.1)**. Timelines, kits, loadouts, stats and the weapon switch are built (M3); hits, ricochet and the rest of the combat
model (M4 onwards) are specified but not built yet. The design lives in
[docs/architecture/combat.md](../../docs/architecture/combat.md) (with one file per milestone, `combat-m1` to
`combat-m5-m9`); the generic input layer is described in [docs/architecture/input.md](../../docs/architecture/input.md).
This README is the map; those documents hold the decisions and the reasons.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatCore` | exists | Combat content built on `GSTNInputCore`, pure and deterministic (no GAS, no Network Prediction): the standard button tags (`Input.Button.Attack`...), the standard command tags and the default command set (`UGSTN_StandardSlots`). The pure timeline layer (`Timeline/`), the extension surface (`Sync/`, `Rules/`, `Hits/`) and the pure kit data (`Kits/`, `Loadout/`), all below. Covered by Automation tests (`GSTNCombat.Core.*`). |
| `GSTNCombatNP` | separate plugin | Moved to the plugin [GSTNCombatNP](../GSTNCombatNP/README.md) (same module name): the combat NP model (`UGSTN_CombatSimComponent`), which needs the fork. |
| `GSTNCombatGAS` | separate plugin | The plugin [GSTNCombatGAS](../GSTNCombatGAS/README.md): native ability-layer tags, attribute sets, meter and weapon-stats effects, and the kit, weapon and super data assets (the ones that hold ability or effect classes). Needs GameplayAbilities, so it is not part of this plugin. |
| `GSTNCombatEditor` | planned | Timeline editor, slot conflict validator UI, debug drawing. |

The generic input code (history, classifier, pattern grammar, matcher, buffer, slots, resolver, bindings, settings) lives in
the [GSTNInput](../GSTNInput/README.md) plugin, which this one depends on (`GSTNCombatCore` -> `GSTNInputCore`). The rule
that decides what stays here: anything about *fighting* (standard buttons, command tags, default slots, hits) is combat,
anything about *input in general* is input. The combat priority tiers and exclusive buttons are the plugin's own settings
(`UGSTN_CombatSettings`, Project Settings > Plugins > GSTN Combat, `GSTNCombat.ini`): defaults in
`Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`, overridable in the project's `Config/DefaultGSTNCombat.ini`. The module
contributes them to the input plugin's settings when it starts (`UGSTN_InputSettings::SetContribution`), which ships both lists
empty; nothing about combat lives in the project's `DefaultGame.ini`.

**Tags.** The combat plugin declares the standard command roots (`Command.Attack`...) and lock tags natively. The standard
button leaves (`Input.Button.Attack`, `.Strong`, `.Special`, `.Crouch`, `.Shield`, `.Jump`) are native too and sit under the
`Input.Button` root that the input plugin owns: an accepted exception for now (the input plugin declares only the root, buttons
are the vocabulary of whoever has them, and `Input.Button` is the filter every tag picker uses), see input.md. Designer buttons
and commands of a game are tags in the game's own tag ini.

**Requirements.** `GSTNCombatCore` needs `GSTNInputCore` and Developer Settings; the descriptor lists only the plugin
`GSTNInput`, so enabling `GSTNCombat` pulls in no fork plugin (GameplayAbilities, AbilitySystemSimulation, NetworkPrediction)
and no Mover. The native lock tags of the combat command roots (`Input.Lock.Command.Attack`, `.Strong`, `.Special`, `.Combo`,
`.Super`) are declared here (`GSTN_CombatLockTags.h`, namespace `GSTNCombatLockTags`), next to the roots' command tags. So are
the `Combat.State.*` leaves the state projection defines and the identity roots `Weapon`, `Kit`, `Super`, `Loadout` and `Skin`
(`GSTN_CombatCoreTags.h`, namespace `GSTNCombatTags`, shared with the GAS layer's own tags): the core does not need the ability
layer to name them. How
the command locks, the activation context and the events work in the simulation is described in the
[GSTNCombatNP README](../GSTNCombatNP/README.md) and the [GSTNInputNP README](../GSTNInputNP/README.md).

**Dependency rule.** Core modules keep no NP, GAS or Mover dependencies: `GSTNCombatCore` depends only on `GSTNInputCore`
and engine modules, and nothing in this plugin depends on `GSTNCombatNP`. An `*NP` module may depend on another plugin's
`*NP` module of the same layer, and on nothing above it; `GSTNCombatNP` depends on this plugin's Core and on `GSTNInputNP` by
design (see the [GSTNCombatNP README](../GSTNCombatNP/README.md)). Shared NP types are not moved into the Core modules.

## Timeline layer (`Timeline/`)

Frame data of a combat ability and its evaluation, pure (no GAS, Network Prediction or Mover; the ability that runs it lives in
`GSTNCombatNP`). Soft references only: montages and curves are `TSoftObjectPtr`, nothing is loaded by the data.

| Type | What it is |
|---|---|
| `FGSTN_CombatTimelineData` | Total and first actionable frame, `FGSTN_TimelineTail`, montage, aim policy, override window, buffer reset rules, the tracks and an optional `FGSTN_RicochetResponse`. Inline on an ability or in the asset. |
| `UGSTN_CombatTimeline` | `UPrimaryDataAsset` (type `GSTN_CombatTimeline`) holding one `FGSTN_CombatTimelineData`; `IsDataValid` runs the validation. |
| `FGSTN_TimelineTrack` + kinds | `TInstancedStruct` tracks: `CancelWindow`, `Movement`, `Tags`, `Event`, `WeaponSwitch`, `Hitbox`, `HurtboxModifier`. A closed set (`EGSTN_TimelineTrackKind`). Hitbox and hurtbox tracks address `FGSTN_PartSelector` (part name, group name or tag), never the whole body, and are ignored until hits exist (M4). |
| `FGSTN_TimelinePlayer` | Stateless `Evaluate(Data, Params, Output)`: active cancel/tag/movement windows, events, swaps, movement starts and ends and the commit, each fired once per `(PreviousFrame, Frame]`; `IsInTail`. O(tracks), the preallocated `FGSTN_TimelineOutput` is reused. A ricochet outcome flag evaluates the recoil data instead. |
| `FGSTN_TimelineValidation` | Pure checks: first actionable frame, tail clamp, deferred-commit rules, track ranges, part selectors, montage length vs frames (only when the length is known), ricochet response. |
| `UGSTN_TimelineUtils` | `BlueprintPure` helpers (`IsInTail`, `GetTailStartFrame`). |

The public types: `EGSTN_AimPolicy`, `EGSTN_CancelCondition` (`Always`, `OnHit`, `OnBlock`, `OnWhiff`), `EGSTN_MoveMixMode`, `EGSTN_MomentumMode`,
`EGSTN_SwitchTarget`, `EGSTN_HurtboxModifierKind`, `EGSTN_TimelineTrackKind`, `FGSTN_PartSelector`, the track structs
`FGSTN_TimelineTrack_{CancelWindow, Movement, Tags, Event, WeaponSwitch, Hitbox, HurtboxModifier}`, `FGSTN_TimelineTail`, `FGSTN_RicochetResponse`,
`FGSTN_TimelineEvalParams` / `FGSTN_TimelineOutput` and `FGSTN_WeaponSwitchRules` (`Resolve`: Next / Previous wrap, an index that must exist).

Ranges are half open (`StartFrame <= frame < EndFrame`). The tail (`FGSTN_TimelineTail`) is off by default; when on, its start
defaults to `FirstActionableFrame` and from there anything cancels the timeline (the flag is `FGSTN_TimelineOutput::bTailCancel`).

**Extending in Blueprint.** All timeline structs are `USTRUCT(BlueprintType)` with `EditAnywhere, BlueprintReadOnly` fields: a
designer authors a timeline in the details panel of an ability or of a `UGSTN_CombatTimeline` asset (the track list is an
instanced-struct array with a type picker), reads it from Blueprint and can call the `UGSTN_TimelineUtils` helpers. Tracks are
a closed C++ set on purpose, because the player evaluates them every simulated frame: a new track kind is a C++ change (add the
struct, a kind and a `switch` case in `FGSTN_TimelinePlayer`), not a Blueprint subclass. Behaviour hooks around timelines come
with the ability layer.

## Extension surface (`Sync/`, `Rules/`, `Hits/`)

What an optional plugin plugs into without `GSTNCombat` naming it (design: modularity-and-tags 5b). Pure types here; the
simulation that uses them is in [GSTNCombatNP](../GSTNCombatNP/README.md).

| Type | What it is |
|---|---|
| `GSTNFeatureIds` | The one central file of fixed feature ids (1 to 127 GSTN plugins, 128 to 239 games, 240 to 255 tests). A feature's sync fragment and tick extension share its id. An id is added here once and never renumbered or reused: it goes on the wire. |
| `FGSTN_SyncFragment` | Base of per-entity state a feature adds to the combat sync state. Virtual `NetSerialize`, `ShouldReconcile` (default: compares the serialized forms), `Interpolate` (default: takes the newer frame) and `ToString`. Integers and frames only. |
| `FGSTN_SyncFragmentRegistry` | Maps the ids to the fragment structs; the owner registers on module startup and unregisters on shutdown. |
| `FGSTN_SyncFragmentList` | Reads and writes a `TArray<TInstancedStruct<FGSTN_SyncFragment>>` kept sorted by id: `FindOrAdd<T>`, `Find<T>`, and the four state operations. |
| `UGSTN_CombatRules` | `UDataAsset`: `Pipeline`, a list of soft step classes. `ResolvePipeline()` loads them once (outside the simulation), `RunPipeline(View)` runs them in order; the first `Reject` ends it. Null, missing or abstract entries are skipped with a warning and reported by `IsDataValid`. Defaults to the one built-in step. |
| `UGSTN_CombatStep` + `UGSTN_CombatStep_Activation` | The step base (`Abstract, Blueprintable, EditInlineNew, DefaultToInstanced`) with one `BlueprintNativeEvent`, `Evaluate(const FGSTN_CombatStepView&) const -> FGSTN_CombatStepResult`, and the built-in pass-through step that documents the contract. |
| `FGSTN_KitExtension`, `FGSTN_KitExtensionList` | Placeholder base for data an optional plugin attaches to kits and weapons, and the `ExtensionData` (`TArray<FInstancedStruct>`) that holds it. Nothing reads it before M3 C5. |
| `FGSTN_HitVictimView` | Read-only description of a thing that can be hit: location, velocity, facing, team byte, hitstop flag, state tags, tagged stats and any number of parts (each with its own state and stats). No GAS type; it assumes neither one health value nor one hurtbox. |

**Wire format of fragments.** An empty list is one flag bit. Otherwise: a packed count, then per fragment, in id order, the id
(one byte), the payload length (packed) and the payload. The length is written on purpose: a machine that does not know the id
(its plugin is not loaded) skips exactly that many bytes, and a fragment that fails to read is dropped without moving the reader, so a
plugin mismatch never desynchronises the rest of the state. It costs one byte per present fragment. Limits: 16 fragments, 512 bytes
each; a larger count or length is a malformed list (archive error). The bytes do not depend on the array order or on plugin load order.
A fragment that cannot be sent (type not registered, a second fragment of the same id, past the 16th, over 512 bytes) is dropped on write
with an ensure, and `FGSTN_SyncFragmentList::ShouldReconcile` ignores it too: it compares only what `NetSerialize` would send, because
the authority's copy never holds the dropped fragment and the client would otherwise reconcile on every frame.

**Extending in Blueprint.** A pipeline step is the Blueprint extension point: create a Blueprint class from `GSTN_CombatStep`, override
`Evaluate` (it receives the view struct, with `BlueprintReadOnly` fields, and returns a result struct) and add the class to the
`Pipeline` of a `UGSTN_CombatRules` asset. The function is `const`, so a Blueprint cannot write its own variables; and it must stay
a pure function of the view, because it runs inside the simulation on every machine and every re-simulation: no world, actor or
component reads, no latent nodes or timers, no state kept between calls (the class default object is shared by all pawns), seeded
random only. Per-frame hooks (`IGSTN_CombatTickExtension` in GSTNCombatNP) stay C++ for the same cost reason. Sync fragments and
kit extensions are structs a plugin adds in C++; designers edit kit extension data in the details panel (instanced-struct picker).

## Kit data (`Kits/`, `Loadout/`)

The parts of the kit, skin and loadout data that name no ability or effect class, so they work without GameplayAbilities. The assets
that hold ability and effect classes (kit, weapon, super) are in [GSTNCombatGAS](../GSTNCombatGAS/README.md). Every content reference
is soft (`TSoftObjectPtr`, `TSoftClassPtr`) or a `FPrimaryAssetId`: no `ConstructorHelpers`, no literal asset path.

| Type | What it is |
|---|---|
| `GSTNCombatAssetTypes` | The primary asset type names (`GSTN_PrimaryKit`, `GSTN_Weapon`, `GSTN_Super`, `GSTN_SuperInputScheme`, `GSTN_Skin`) and `MaxSuperSlots` (8). A project registers the types it uses under `PrimaryAssetTypesToScan` in its own `DefaultGame.ini`; the plugin does not edit the project's config. |
| `FGSTN_CommandSet` | A kit's commands: `BaseSet` (empty = `UGSTN_CombatSettings::DefaultCommandSet`), `AddedSlots`, `RemovedCommands`. `Compose(Base, Out)` is base minus removed plus added: base order kept, removal matches the command tag exactly (a root removes none of its leaves), an added slot with a command already present replaces it in place. Pure over slot specs; `ResolveBaseSet()` loads the soft base and is for loadout and validation code, never the simulation. |
| `UGSTN_SuperInputScheme` | `UPrimaryDataAsset` (`GSTN_SuperInputScheme`): `FixedSlots` (one `FGSTN_InputSlotSpec` per super slot) or `PerSuperCode` (a `CodeEntry` template around each super's own code), and the tag carried while a code is entered. `IsDataValid` checks the slots. |
| `UGSTN_SkinDefinition` | `UPrimaryDataAsset` (`GSTN_Skin`): soft mesh, materials and anim class, `SkinTags`. Presentation only. |
| `FGSTN_LoadoutData` | What a player brings: primary kit id, additional weapon ids, super ids, `FGSTN_SlotCustomisation` list, `DesignerValues` (tag to float) and skin id; ids only. `NetSerialize` and `Serialize` (the second leads with a version, for save games and presets; a newer version flags the archive). Tags and ids travel as names in both, so the format does not depend on the project's tag replication mode; a tag that no longer exists is dropped on load. Both write the map in tag-name order, so the same loadout is the same bytes, and bound every count (`MaxEntries` 64), so a hostile stream cannot allocate. Rules deciding whether a loadout is allowed come with the loadout component. |
| `FGSTN_DesignerIdentity`, `UGSTN_DesignerIdentityUtils` | The identity rules of designer assets, with the root as a parameter (code never names a leaf): `ValidateExactlyOne` (a kit or weapon holds exactly one leaf under its root, the other tags only classify), `FindDuplicateKeys` (uniqueness over a list), and in the editor `GatherFromPrimaryAssets` (reads the key of the other registered assets of a type; nothing is compared until the project registers the type). |
| `FGSTN_KitExtensionList` | The `ExtensionData` of kits and weapons; the assets in GSTNCombatGAS embed it as `Extensions`. |

**Extending in Blueprint.** The data assets are the designer surface: a super scheme or a skin is a data asset (create it from the
Content Browser, no Blueprint subclass needed), every struct is `BlueprintType` with `EditAnywhere, BlueprintReadOnly` fields, and tag
fields carry `Categories` (the identity containers of kits and weapons are the exception: they also hold classifier tags, so the
identity rule is checked by `IsDataValid` instead). `UGSTN_CommandSetUtils::GetCommandSetCommands` and
`UGSTN_DesignerIdentityUtils::FindIdentityTag` are callable from Blueprint. Behaviour an optional plugin adds to a kit goes in a struct
derived from `FGSTN_KitExtension`, picked in the asset's `Extensions` list. There is nothing to subclass in this layer on purpose: it
is data.

## Primary asset types a host project must register

Loadout data stores `FPrimaryAssetId`s, never hard references, so the data layer can validate identities and load assets on demand through the Asset Manager (`UAssetManager::GetPrimaryAssetIdList`, async loads). A type that is not registered has no ids for the Asset Manager to list or load.

| Primary asset type | Base class (`AssetBaseClass`) |
|---|---|
| `GSTN_SuperInputScheme` | `/Script/GSTNCombatCore.GSTN_SuperInputScheme` |
| `GSTN_Skin` | `/Script/GSTNCombatCore.GSTN_SkinDefinition` |
| `GSTN_CombatTimeline` | `/Script/GSTNCombatCore.GSTN_CombatTimeline` |

Add one `+PrimaryAssetTypesToScan=(PrimaryAssetType="...",AssetBaseClass="/Script/<Module>.<Class>",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/<your folder>")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))` line per type under `[/Script/Engine.AssetManagerSettings]` in the project's `DefaultGame.ini`. The plugin never edits the project's config. `AlwaysCook` matters because nothing hard-references these assets: they are found by id.

## Gameplay tags

Native tags, declared in four files (nothing here is defined in an ini; the designer leaves live in the project's tag ini):

| File | Tags |
|---|---|
| `GSTN_CombatCoreTags.h` (`GSTNCombatTags`) | `Combat.State.{Busy, InTimeline, SuperInput, CancelWindow, OverrideWindow, Falling}` (the projection vocabulary; the combat tick does not project them yet), and the identity roots `Weapon`, `Kit`, `Super`, `Loadout`, `Skin` |
| `Input/GSTN_CombatCommandTags.h` | the `Command.*` families (`Attack`, `Strong`, `Special`, `Super`, ...) the standard slots bind |
| `Input/GSTN_CombatButtonTags.h` | the standard buttons |
| `Input/GSTN_CombatLockTags.h` | the combat lock tags (`Combat.Lock.Movement` is declared in GSTNCombatGAS) |

`Command.Super.Slot1..Slot8` are declared in `Loadout/GSTN_CompiledBindingTable.cpp`, beside the only code that uses them (the super code slots).

## How to extend it

The generic extension points (a new input condition, slot, binding or buffer reset rule) are described in the
[GSTNInput README](../GSTNInput/README.md). What is specific to combat:

- **A new standard command**: add its tag to `GSTN_CombatCommandTags` (and give its root a tier in
  the combat settings, `Config/DefaultGSTNCombat.ini`, if it is a new root), add the slot in `UGSTN_StandardSlots::Build`, then update the
  `GSTNCombat.Core.StandardSlots.*` tests (they check the shapes, the chords and that the validator finds no conflicts).
- **A new standard button**: add its tag to `GSTN_CombatButtonTags` and to the project's input config; add it to
  the combat settings' `ExclusiveButtons` if its presses compete with attacks for one move.
- **A new loadout rule**: derive from `UGSTN_LoadoutRule` (Blueprintable, EditInlineNew), override `Evaluate(const FGSTN_LoadoutRulesView&)`
  returning an `FGSTN_LoadoutRuleResult` (errors and warnings as `FText`), add an instance to the `Rules` list of a `UGSTN_LoadoutRules` asset
  and test it like `GSTNCombat.Core.LoadoutRules.DefaultSetAndCustomRule`. The rule sees plain data, never a GAS type.
- **A new timeline track kind**: add the struct (derive `FGSTN_TimelineTrack`), a value in `EGSTN_TimelineTrackKind`, its range check in
  `FGSTN_TimelineValidation`, a `switch` case in `FGSTN_TimelinePlayer::Evaluate` and a list in `FGSTN_TimelineOutput` (reserved in `Reserve`, emptied in
  `Reset`; the evaluation must not allocate), then whoever consumes the output (the combat tick) handles it; add a case to `Timeline.WindowsAtFrame`
  or `Timeline.EventsOnce` and to `Timeline.NoAllocation`.
- **The combat component**: in the [GSTNCombatNP README](../GSTNCombatNP/README.md).

Tests are in `Source/GSTNCombatCore/Private/Tests/` with the helpers of `GSTN_CombatTestUtils.h`. The timeline layer is covered by
`GSTNCombat.Core.Timeline.{WindowsAtFrame, EventsOnce, NoAllocation, Validation, TailCancelFlagAfterStart, TailCancelNotBeforeStart,
TailCancelUnsetMeansNone, TailStartClampedToLength, RicochetOutcomeSwitchesToRecoil}`; the rules pipeline by
`GSTNCombat.Core.Rules.{PipelineOrder, UnknownStepSkipped}`; the tags and settings by `GSTNCombat.Core.{Smoke, NativeTags, OwnSettings, SettingsDefaults, Locks.NativeRoots,
Data.CoreTagsDeclared}`; the standard command set by `GSTNCombat.Core.StandardSlots.{Shapes, ChordCommands, NoConflicts, SlotSetAsset}`; stats by
`GSTNCombat.Core.Stats.{Factor, ScalingIsBaseRelative, WeaponSwitchResolve}`; the compiled table and loadout rules by `GSTNCombat.Core.Table.*` and
`GSTNCombat.Core.LoadoutRules.*`; `Data.LoadoutNetWriteBounds` bounds the loadout wire size; the kit data by `GSTNCombat.Core.Data.{CommandSetComposition, LoadoutNetSerialize, LoadoutSaveSerialize, IdentityExactlyOne, IdentityUniquePerType}`. The fragment and tick extension tests are `GSTNCombat.NP.*`, in GSTNCombatNP.

## Conventions

- Types are prefixed `GSTN_` (`UGSTN_`, `FGSTN_`, `EGSTN_`); export macro is `GSTNCOMBATCORE_API` (`GSTNINPUTCORE_API` for the input plugin).
- Public headers in `Public/<Area>/`, implementations in `Private/<Area>/`.
- Comments explain *why* and the simulation constraints. Headers document the intent of a class: read the header before
  the `.cpp`.
- Tuning values are `UPROPERTY` with `Category = "Area|Section"` and `meta` limits, grouped in small structs.

## Testing

- Pure logic: Automation tests `GSTNCombat.Core.*` (Session Frontend > Automation, or
  `UnrealEditor-Cmd <your project>.uproject -ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit"`, with your project's `.uproject`).
- Network behaviour (combat NP model): see the [GSTNCombatNP README](../GSTNCombatNP/README.md) and
  [testing.md](../../docs/architecture/testing.md).

## Further reading

[combat.md](../../docs/architecture/combat.md) (overview and decisions) ·
[combat-m1-core-input.md](../../docs/architecture/combat-m1-core-input.md) ·
[combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md) ·
[combat-m4-hits.md](../../docs/architecture/combat-m4-hits.md) ·
[input.md](../../docs/architecture/input.md) · [GSTNInput README](../GSTNInput/README.md) · [GSTNCombatNP README](../GSTNCombatNP/README.md) · [gas-npp.md](../../docs/architecture/gas-npp.md) ·
[PATCHES.md](../GASNPP/PATCHES.md)

---

## Compiled binding table and loadout rules (`Loadout/`, added in C6)

The pure half of loadout application. Both files take plain values, so a test builds a loadout in memory and no ability class,
world or content asset is involved.

| Type | What it is |
|---|---|
| `FGSTN_CompileInput` (+ `FGSTN_KitCompileInput`, `FGSTN_WeaponCompileInput`, `FGSTN_SuperCompileInput`, `FGSTN_BindingInput`) | What `Compile` reads: the kit's command set (with its resolved base set), the bindings of the kit and of each weapon after inheritance, the filled super slots, the super input scheme, the customisations and the revision. The ability and timeline override of a binding are opaque `FSoftObjectPath`s, never loaded or inspected here. |
| `FGSTN_BindingTableCompiler` | `Compile(Input, Settings, &Report)` builds the table; `FindConflicts(Table, Settings, Report)` runs the input plugin's validators on it; `GetSuperSlotCommand(N)` is `Command.Super.Slot<N+1>` (native, declared next to the table compiler, 8 of them). |
| `FGSTN_CompiledBindingTable` | `Slots` (distinct slots, first-use order), `Entries` (kit rows, then weapon 0..N, then super slots; by command name inside each), `Registry` (distinct `(ability, timeline override)` pairs, sorted), `GrantedAbilities`, `Revision`, `WriteBytes` (deterministic bytes of the whole table). A row carries `SlotIndex`, the binding's own `PriorityOffset`/`BufferFramesOverride`, `TimelineIndex`, and the rank (`Priority`) of its effective slot (`MakeEffectiveSlot`). The sync state carries row and registry indices, never tags. |
| `FGSTN_LoadoutRulesView`, `FGSTN_LoadoutRuleResult` | The const view a rule reads (the loadout, plain values read from the kit, weapons, supers and skin, and the findings of the compile) and the `{Errors, Warnings}` it returns. All `BlueprintType`, `BlueprintReadOnly` fields. |
| `UGSTN_LoadoutRule` and the built-in rules | `UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)` with one `BlueprintNativeEvent` `Evaluate(View) -> Result`. Built in: `KitExists`, `AdditionalWeapons` (exist, match the kit's query, count), `Supers` (slots, exist, match the kit's query), `Customisations` (only the weapon's allowed alternatives), `Skin` (exists, fits the kit), `NoAmbiguousBindings` (table errors, shadowed slots as warnings), `TimelineRegistry` (every timeline ability the kit can trigger is in the registry). |
| `UGSTN_LoadoutRules` | Data asset holding an `Instanced` list of rule objects (the seven built-ins by default); `Validate(View)` runs them in order and joins the findings. Replace the asset or edit its list to add, drop or reorder rules. |

Rules for the compile: command set = base minus the kit's removed commands plus its added slots; a binding needs a slot in the
set or its own override, otherwise it is reported and skipped (never a failed compile); customisations replace the ability of
every row a weapon has for the command; the registry holds the timeline rows (a row with an override, or what the optional
`IsTimelineAbility` predicate says) plus the triggered abilities and passives that are timeline abilities. An empty tag query on
the kit (weapons, supers, skins) allows none. The two layers split like this: the assets that hold ability classes are read by
`FGSTN_LoadoutAssembly` in [GSTNCombatGAS](../GSTNCombatGAS/README.md), which hands this module the plain input and view.

**Extending in Blueprint.** Derive a Blueprint class from `UGSTN_LoadoutRule`, override `Evaluate` (read the view, return errors
and warnings: no world reads, no latent nodes, no state kept between calls), and add an instance to a `UGSTN_LoadoutRules` asset.
A rule's findings are texts for the loadout screen; an error makes the loadout invalid.

Tests (`GSTNCombat.Core.*`): `Table.{CompileOrder, CommandSetBaseMinusRemovedPlusAdded, RegistryDedupeAndSort, Customisation, SuperCodeSlots, DeterministicBytes, AmbiguousConflict}`
and `LoadoutRules.{KitExists, AdditionalWeapons, Supers, Customisations, Skin, NoAmbiguousBindings, TimelineRegistry, DefaultSetAndCustomRule}`.

## Jump (`Jump/`, added in C13)

Pure and relative to a `GravityDir` the caller passes in (up = -GravityDir, never world Z): `FGSTN_JumpState` (the jump part of the combat sync core, one bit when default), `FGSTN_JumpLogic` (`Step`, `ClassifyJump`, `ApplyTakeoff`, `ApplyFastFall`, `GravityScale`, `SimulateApex`), `FGSTN_GravityPhases` / `FGSTN_GravityBase` (the phase scale applied to the local gravity, remembering the authored one) and `FGSTN_JumpResetRules` data (air jumps Keep / Refill / Set N, coyote Clear / Keep, cancel the hold phase on an external launch; default Refill + Clear + cancel). The rules and `JumpCoyoteFrames` are settings of `UGSTN_CombatSettings`. Native tags `Command.Movement.Jump` / `.FastFall`. Tests: `GSTNCombat.Core.Jump.*`, `GSTNCombat.Core.Gravity.*`.

## Movement kit data (`Movement/`, `Input/GSTN_MovementSlots`, added in C14)

Dash, ground dodge, air dodge and interact are **default kit content**, not framework: nothing in the core assumes them.

- `FGSTN_DashMoveData` + `FGSTN_DashMath`: the speeds of one impulse (forward speed in the plane perpendicular to the local up, speed along up, mix and momentum mode, whether it leaves for the air mode) and the pure math. Up is always `-GravityDir`, never world Z; the facing is projected onto the plane perpendicular to it. `PredictArc` gives air time, apex and range of an arc for tuning and tests.
- `FGSTN_MoveTimelines`: builds the default timelines as plain `FGSTN_CombatTimelineData` (one movement track over the whole length with `bCancelOnLand` / `LandingLagFrames`, and for the dodges a `HurtboxModifier` track on the group `Feet` from take-off to 40 % of the airborne frames). A kit replaces them with its own timeline asset; changing the window is a data edit.
- `UGSTN_MovementSlots`: the slots `Command.Movement.Dash` (press of `Input.Button.Sprint`), `Command.Defence.Dodge` (press of `Input.Button.Dodge`; the bindings pick ground or air dodge by context) and the interact pair. Tap and hold are two `Release` anchors that exclude each other by duration (`FGSTN_InputCondition_Release::MaxHeldFrames`), so a press never fires both and a pending hold never swallows the tap. `InteractHoldFrames` (settings) is the threshold. Default: tap = `Command.Interact.Grab`, hold = `Command.Interact.PickUp`; `Build(true)` swaps them.
- A kit without a dodge lists `Command.Defence.Dodge` in `CommandSet.RemovedCommands`, or simply does not grant the abilities: the command then starts nothing and reserves nothing.
- Tags added: buttons `Input.Button.Dodge/Sprint/Interact`, commands `Command.Movement.Dash`, `Command.Defence.Dodge`, `Command.Interact.Grab/PickUp`, states `Combat.State.Hitstun/Hitstop` (projected by the M4 hit reaction; the default dodges block on them).
- Tests: `GSTNCombat.Core.Dash.*`, `GSTNCombat.Core.Dodge.*`, `GSTNCombat.Core.MovementSlots.*`.

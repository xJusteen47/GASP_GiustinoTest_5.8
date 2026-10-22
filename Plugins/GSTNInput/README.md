# GSTNInput

Fighting-game style input for Unreal Engine 5.8, usable by any game: designer-defined commands (button, direction,
motion, chord, charge, hold, release, mash, spin), time and hold buffering, reset rules, priority between commands and
command to GameplayAbility bindings with context requirements. Meant to be simulated and rolled back by Network
Prediction (the GAS-NPP fork in `Plugins/GASNPP`), but the logic itself does not need it: this plugin is `GSTNInputCore` only and
works without the fork. The simulation adapter is the separate plugin [GSTNInputNP](../GSTNInputNP/README.md).

Status: **experimental (0.1)**. `GSTNInputCore` exists (the plugin `GSTNInputNP` holds the simulator and the rest of the NP side). The design, including what is planned, lives in
[docs/architecture/input.md](../../docs/architecture/input.md); the combat plugin ([GSTNCombat](../GSTNCombat/README.md))
is built on top of this one. This README is the map; the documents hold the decisions and the reasons.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNInputCore` | exists | Pure, deterministic logic with no GAS, no Network Prediction and no Mover: input samples, the stick direction classifier, the input history, the pattern grammar (`FGSTN_InputPattern`, conditions), the matcher, buffer reset rules, slots, the slot resolver, slot validation, command bindings, command locks (`UGSTN_CommandLocks`), the authoritative event stream (`UGSTN_SimEventSubsystem`, `FGSTN_SimEvent`, the outbox), the button map, the input config and the project settings. Automation tests `GSTNInput.Core.*`. |
| `GSTNInputNP` | separate plugin | Moved to the plugin [GSTNInputNP](../GSTNInputNP/README.md) (same module name): the Network Prediction adapter (`FGSTN_InputCmd`, `FGSTN_InputSimState`, `FGSTN_InputSimulator`, binding set, sources, context providers, Mover tracker, standalone component). Automation tests `GSTNInput.NP.*`. |

**Requirements.** This plugin (`GSTNInputCore`) needs only Gameplay Tags, Developer Settings and (privately) Enhanced Input; its
descriptor lists only Enhanced Input, so it works in a project without the GAS-NPP fork (NetworkPrediction, GameplayAbilities fork,
Mover). To simulate it with Network Prediction, enable the plugin [GSTNInputNP](../GSTNInputNP/README.md), which needs the fork.
How to wire the simulation side (embedding it in a model, the standalone component, the activation context, the Mover
tracker, Network Prediction sort priorities) is in the [GSTNInputNP README](../GSTNInputNP/README.md).

The plugin has no game vocabulary: no "attack", "crouch" or combat tags. Buttons, commands and their priority tiers are
data the game provides; the combat plugin provides a fighting-game set.

**Dependency rule.** An `*NP` module may depend on another plugin's `*NP` module of the same layer, and on nothing above it.
`GSTNCombatNP` depends on `GSTNInputNP` by design: the combat NP model embeds the input simulation, one NP model per
character (see input.md). Core modules keep no NP, GAS or Mover dependencies, so shared NP types are not moved into
`GSTNInputCore`. `GSTNCombatNP` may use only the Public headers and interfaces of `GSTNInputNP` (for example
`IGSTN_InputSimHost`), never its internals.

## Setting up a game

1. **Declare buttons and commands as tags.** Buttons go under `Input.Button`, commands under `Command` (natively in C++,
   or in a tag ini of the game, for example `Config/Tags/GSTNCommands.ini` in the project; the sample project has one). The plugin itself declares only the roots, the
   `Input.Context.*` tags, the `Input.BufferEvent.*` frame events and `Input.Event.CommandExecuted` (`GSTN_InputTags.h`).
2. **Make an input config** (`UGSTN_InputConfig` data asset): the list of buttons, each with its tag and the Enhanced
   Input action that drives it (a soft reference: the host resolves the actions once at setup). The array index is the
   button's bit in every mask, so every machine of a match must use the same config and order.
3. **Set the settings** (Project Settings > Plugins > GSTN Input). They are stored in the plugin's own config file,
   `GSTNInput.ini`: the plugin ships its defaults in `Plugins/GSTNInput/Config/DefaultGSTNInput.ini` and a project overrides
   any of them in its own `Config/DefaultGSTNInput.ini`; nothing goes in `DefaultGame.ini`. They are the buffer lengths, stick
   zones, and the two lists that start empty, `CommandTiers` (priority tier per command root) and `ExclusiveButtons` (buttons
   whose presses compete for one move). A game can fill them there, or a plugin brings its own through
   `UGSTN_InputSettings::SetContribution` (what the combat plugin does with its own settings).
   `ExclusiveContextGroups` lists state tags of which a context holds at most one (the plugin ships Grounded + Airborne; add the
   groups of your own movement states, or contribute them through the optional last argument of `SetContribution`): binding
   validation uses them to accept a Grounded and an Airborne binding of one command as distinct, and to reject a binding that
   requires two members of one group.
   `PriorityCriteria` is the ordered list that ranks commands matching at once (default Tier, Specificity, ManualOffset).
4. **Define slots** (`UGSTN_InputSlotAsset` / `UGSTN_InputSlotSet`): a command tag plus the pattern that triggers it, then
   run the slot validator, which reports slots that shadow or conflict with each other.

## Primary asset types a host project must register

Slot assets (`UGSTN_InputSlotAsset`) return `GSTN_InputSlot` from `GetPrimaryAssetId`, so slot sets and tools can enumerate and load them by id. Register the type only if your project uses standalone slot assets.

| Primary asset type | Base class (`AssetBaseClass`) |
|---|---|
| `GSTN_InputSlot` | `/Script/GSTNInputCore.GSTN_InputSlotAsset` |

Add one `+PrimaryAssetTypesToScan=(PrimaryAssetType="...",AssetBaseClass="/Script/<Module>.<Class>",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/<your folder>")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))` line per type under `[/Script/Engine.AssetManagerSettings]` in the project's `DefaultGame.ini`. The plugin never edits the project's config. `AlwaysCook` matters because nothing hard-references these assets: they are found by id.

## How to extend it

Everything in `GSTNInputCore` is testable without a world: add an Automation test next to the existing ones
(`Source/GSTNInputCore/Private/Tests/GSTN_InputTests_*.cpp`). The helpers in `GSTN_TestUtils.h` build conditions, patterns
and slots, play a scripted input (`FGSTN_InputScript`) and resolve it; tests use the test-only tags of `GSTN_TestTags.h`.

### A new input condition

Conditions are structs held in the steps of a pattern; they only see the history, the button map and frame numbers.
Derive from `FGSTN_InputCondition` (`Public/Input/GSTN_InputConditions.h`):

```cpp
USTRUCT(BlueprintType, meta = (DisplayName = "Held Both"))
struct GSTNINPUTCORE_API FGSTN_InputCondition_HeldBoth : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTag ButtonA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FGameplayTag ButtonB;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual int32 GetSpecificity() const override { return 2; }
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override;
};
```

Decide whether it is an *anchor* (it produces a frame: `IsAnchorCondition`) or a check relative to the anchor. Override
`AppendCanonicalInput` so the slot validator and the tests can build the smallest input that satisfies it. Keep `Matches`
allocation free and pure.

### A new priority criterion

When several commands match at once, they are ranked by the criteria of `UGSTN_InputSettings::PriorityCriteria`, in
order: the first criterion a command scores higher on wins, the next ones only break ties, and equal on all goes to slot
order. Derive from `FGSTN_PriorityCriterion` (`Public/Input/GSTN_PriorityCriteria.h`) and override `Score`, which sees
the slot, its pattern and the settings (never the frame):

```cpp
USTRUCT(BlueprintType, meta = (DisplayName = "Chords First"))
struct FGame_PriorityCriterion_ChordsFirst : public FGSTN_PriorityCriterion
{
	GENERATED_BODY()

	virtual int32 Score(const FGSTN_PriorityContext& Context) const override;
};
```

Then add it to the list in Project Settings > GSTN Input, or in code with `TInstancedStruct<FGSTN_PriorityCriterion>::Make<...>()`.
Reorder or remove the built-ins (`Tier`, `Specificity`, `ManualOffset`) the same way; at most 8 entries count. `Score`
must be pure and identical on every machine: it is the same kind of setting as the buffer lengths. See
`Private/Tests/GSTN_InputTests_Priority.cpp` for examples.

### Writing a rule in Blueprint (the slower path)

The three instanced-struct families (conditions, priority criteria, buffer reset rules) cannot be subclassed in Blueprint, so
each has one opt-in wrapper that holds a Blueprintable class with a const `BlueprintNativeEvent`
(`Public/Input/GSTN_BlueprintRules.h`): `FGSTN_InputCondition_Blueprint` + `UGSTN_InputConditionBlueprint::Matches`,
`FGSTN_PriorityCriterion_Blueprint` + `UGSTN_PriorityCriterionBlueprint::Score` and `FGSTN_BufferResetRule_Blueprint` +
`UGSTN_BufferResetRuleBlueprint::ShouldFlush`. Create a Blueprint class from the base, override the function, then pick
"Blueprint (slower)" in the family's list and set the class. The C++ structs stay the default and the fast path.

It is the slower path: the wrapper runs the Blueprint VM on the class default object, and reset rules and conditions are
evaluated every simulated frame (priority criteria only once per slot, when a table is built). Because the rules run inside
the simulation, a Blueprint rule **must stay deterministic**: it answers only from the view struct it receives (a plain-value
snapshot of the input around the anchor frame, the slot's facts, or the frame events), with no world queries, no actors, no
random numbers or time, and no state outside its inputs (the class default object is shared by every pawn and is not rolled
back). The functions are `const` to say so. A Blueprint condition reports no canonical input, so slot validation cannot compare
it with other slots. See `Private/Tests/GSTN_InputTests_Blueprint.cpp`.

The movement context provider has the same kind of Blueprint class, `UGSTN_BlueprintInputContextProvider` (`Context/`), whose
events (`GetBlueprintFacingYaw`, `GetBlueprintContextTags`, `GetBlueprintContextFingerprint`, `GetBlueprintFrameEvents`) are
called every simulated tick under the same rules; the base `UGSTN_InputContextProvider` stays plain C++.

### A new slot (a command)

A slot ties a command tag to a pattern, a priority and a direction policy (`FGSTN_InputSlotSpec`, `GSTN_InputSlot.h`).
Create the command tag, add the slot to a set, run the slot validator (`GSTN_InputSlotValidation`).

### A new binding

Bindings say which command a context runs (`GSTN_CommandBindings.h`, `FGSTN_TagRequirements` for the conditions on the
owner's tags). A command without a binding is never a candidate. `UGSTN_CommandBindingUtils` resolves bindings and
detects conflicts between them.

### Using it with Network Prediction

See the [GSTNInputNP README](../GSTNInputNP/README.md) (embedding the simulation, standalone component, activation context, Mover tracker).

### Tag roots

Tag roots: `Input.Button` (buttons), `Command` (commands), `Input.Context.*` (written by the context provider),
`Input.BufferEvent.*` (reset-rule events), `Input.Lock.Command` (command locks) and `Input.Event.CommandExecuted`
(`GSTN_InputTags.h`). Per-root lock tags (`Input.Lock.Command.<root>`) are declared by whoever declares the command root.

### Command locks

`Input.Lock.Command` on the owner stops every command from starting (presses stay buffered); `Input.Lock.Command.<path>`
stops the commands under `Command.<path>` (a stance that owns `Input.Lock.Command.Attack` stops `Command.Attack.*`
while a dash command under another root still starts). Abilities and effects grant the lock tags like any tag; nothing
in the simulation writes them. `UGSTN_CommandLocks` has the mapping and the checks and is testable without a world. A locked
command reserves its press like an unavailable one, and a command with a slot but no binding is not a candidate, so it
never reserves a press. To lock a designer root, declare `Input.Lock.Command.<root>` next to the root's tag.

### Authoritative events

`UGSTN_SimEventSubsystem` (a world subsystem) is the stream quests, statistics and kill feeds listen to: `Listen` takes a
`FGameplayTagQuery` over the event's type and tags (an empty query takes everything), `ListenNative` takes a C++ callback.
The input simulation publishes `Input.Event.CommandExecuted` with the command tag, the owner's context tags and the ability's
asset tags. Events are written into an outbox by the simulation on every machine and published by the host after the
Network Prediction tick **on the authority only**, so each activation is counted once however often a client re-simulates
it. The authority is `HasAuthority()` and nothing else: a listen-server host's own pawn is the authority and also has a local
player, and its events are published like any other pawn's. Nothing is replicated; clients that need feedback use
replicated results or predicted presentation.

### Soft references and what is resolved when

Assets the plugin's data points at are soft references, so a config or a binding set does not drag a project's input actions,
slot sets and abilities into memory with it: `FGSTN_InputButtonDef::Action` and `UGSTN_InputConfig::MoveAction`
(`TSoftObjectPtr<UInputAction>`), `UGSTN_CommandBindingSet::SlotSets` (`TSoftObjectPtr`), `FGSTN_CommandBinding::Ability`
(`TSoftClassPtr<UGameplayAbility>`) and the hosts' `InputConfig` / `SlotBindings`. Anything the simulation or the input
source reads every frame is **resolved once at setup, never in a tick**: the host loads its config and binding set in
`BeginPlay`, `UGSTN_InputConfig::ResolveActions` loads the actions, and `UGSTN_CommandBindingSet::BuildTable` loads the slot sets
and abilities into the `FGSTN_CommandTable` (`FGSTN_CompiledBinding::Ability` is a plain `TSubclassOf`). The loadout of a
running match is fixed, like the table: change it only by rebuilding at a safe point.

### A new buffer reset rule

Rules flush buffered presses when something happens (`GSTN_BufferResetRules.h`). Derive from `FGSTN_BufferResetRule`,
override `ShouldFlush` and, if the rule watches tags, `GetWatchedTags`. Existing examples: `OnEvent` (a frame event such
as `Input.BufferEvent.Landed`) and `OnTagAdded`. Rules are listed at global, weapon and move level and applied in that
order.

## Conventions

- Gameplay tags: the plugin declares its roots and its own vocabulary natively. Leaves that are game data (a game's buttons,
  commands, lock tags) belong in the game's tag ini, not in code.
- Types are prefixed `GSTN_` (`UGSTN_`, `FGSTN_`, `EGSTN_`); export macros `GSTNINPUTCORE_API` and `GSTNINPUTNP_API`; log category
  `LogGSTNInput`.
- Public headers in `Public/<Area>/`, implementations in `Private/<Area>/`.
- Comments explain *why* and the simulation constraints. Headers document the intent of a class: read the header before
  the `.cpp`.
- Simulation code must be deterministic and rollback-safe (rules in
  [gas-npp.md](../../docs/architecture/gas-npp.md)).

## Testing

Automation tests `GSTNInput.Core.*`: Session Frontend > Automation, or headless
`UnrealEditor-Cmd <your project>.uproject -ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit" -unattended -nullrhi`
(with your project's `.uproject`).
See [testing.md](../../docs/architecture/testing.md).

## Further reading

[input.md](../../docs/architecture/input.md) (design and decisions) · [combat.md](../../docs/architecture/combat.md) ·
[combat-m1-core-input.md](../../docs/architecture/combat-m1-core-input.md) ·
[combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[gas-npp.md](../../docs/architecture/gas-npp.md) · [testing.md](../../docs/architecture/testing.md)

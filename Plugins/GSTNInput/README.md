# GSTNInput

Fighting-game style input for Unreal Engine 5.8, usable by any game: designer-defined commands (button, direction,
motion, chord, charge, hold, release, mash, spin), time and hold buffering, reset rules, priority between commands and
command to GameplayAbility bindings with context requirements. Meant to be simulated and rolled back by Network
Prediction (the GAS-NPP fork in `Plugins/GASNPP`), but the logic itself does not need it.

Status: **experimental (0.1)**. Only `GSTNInputCore` exists. The design, including what is planned, lives in
[docs/architecture/input.md](../../docs/architecture/input.md); the combat plugin ([GSTNCombat](../GSTNCombat/README.md))
is built on top of this one. This README is the map; the documents hold the decisions and the reasons.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNInputCore` | exists | Pure, deterministic logic with no GAS, no Network Prediction and no Mover: input samples, the stick direction classifier, the input history, the pattern grammar (`FGSTN_InputPattern`, conditions), the matcher, buffer reset rules, slots, the slot resolver, slot validation, command bindings, the button map, the input config and the project settings. Automation tests `GSTNInput.Core.*`. |
| `GSTNInputNP` | planned | The adapter to the Network Prediction fork: input command and sim state, simulator, input sources, the Mover sim-state tracker, the standalone input component. Today this code is in `GSTNCombatNP` and moves here in step S2. |

The plugin has no game vocabulary: no "attack", "crouch" or combat tags. Buttons, commands and their priority tiers are
data the game provides; the combat plugin provides a fighting-game set.

## Setting up a game

1. **Declare buttons and commands as tags.** Buttons go under `Input.Button`, commands under `Command` (natively in C++,
   or in a tag ini such as `Config/Tags/GSTNCommands.ini`). The plugin itself declares only the roots, the
   `Input.Context.*` tags, the `Input.BufferEvent.*` frame events and `Input.Event.CommandExecuted` (`GSTN_InputTags.h`).
2. **Make an input config** (`UGSTN_InputConfig` data asset): the list of buttons, each with its tag and the Enhanced
   Input action that drives it. The array index is the button's bit in every mask, so every machine of a match must use
   the same config and order.
3. **Set the project settings** (Project Settings > Plugins > GSTN Input, stored in `Config/DefaultGame.ini` under
   `[/Script/GSTNInputCore.GSTN_InputSettings]`): buffer lengths, stick zones, and the two lists that start empty,
   `CommandTiers` (priority tier per command root) and `ExclusiveButtons` (buttons whose presses compete for one move).
   `PriorityCriteria` is the ordered list that ranks commands matching at once (default Tier, Specificity, ManualOffset).
4. **Define slots** (`UGSTN_InputSlotAsset` / `UGSTN_InputSlotSet`): a command tag plus the pattern that triggers it, then
   run the slot validator, which reports slots that shadow or conflict with each other.

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

### A new slot (a command)

A slot ties a command tag to a pattern, a priority and a direction policy (`FGSTN_InputSlotSpec`, `GSTN_InputSlot.h`).
Create the command tag, add the slot to a set, run the slot validator (`GSTN_InputSlotValidation`).

### A new binding

Bindings say which command a context runs (`GSTN_CommandBindings.h`, `FGSTN_TagRequirements` for the conditions on the
owner's tags). A command without a binding is never a candidate. `UGSTN_CommandBindingUtils` resolves bindings and
detects conflicts between them.

### A new buffer reset rule

Rules flush buffered presses when something happens (`GSTN_BufferResetRules.h`). Derive from `FGSTN_BufferResetRule`,
override `ShouldFlush` and, if the rule watches tags, `GetWatchedTags`. Existing examples: `OnEvent` (a frame event such
as `Input.BufferEvent.Landed`) and `OnTagAdded`. Rules are listed at global, weapon and move level and applied in that
order.

## Conventions

- Types are prefixed `GSTN_` (`UGSTN_`, `FGSTN_`, `EGSTN_`); export macro `GSTNINPUTCORE_API`; log category
  `LogGSTNInput`.
- Public headers in `Public/<Area>/`, implementations in `Private/<Area>/`.
- Comments explain *why* and the simulation constraints. Headers document the intent of a class: read the header before
  the `.cpp`.
- Simulation code must be deterministic and rollback-safe (rules in
  [gas-npp.md](../../docs/architecture/gas-npp.md)).

## Testing

Automation tests `GSTNInput.Core.*`: Session Frontend > Automation, or headless
`UnrealEditor-Cmd GASP_Giustino2.uproject -ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit" -unattended -nullrhi`.
See [testing.md](../../docs/architecture/testing.md).

## Further reading

[input.md](../../docs/architecture/input.md) (design and decisions) · [combat.md](../../docs/architecture/combat.md) ·
[combat-m1-core-input.md](../../docs/architecture/combat-m1-core-input.md) ·
[combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[gas-npp.md](../../docs/architecture/gas-npp.md) · [testing.md](../../docs/architecture/testing.md)

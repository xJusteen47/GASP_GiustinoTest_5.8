# GSTNCombat

Fighting-game style combat for Unreal Engine 5.8: input classification, a command buffer with priorities, moves as
gameplay abilities and a hit system, all simulated by Network Prediction (the GAS-NPP fork in `Plugins/GASNPP`) so it
rolls back and re-simulates together with the ability system and Mover.

Status: **experimental (0.1)**. Input and the combat simulation component exist; moves, kits and hits are specified but
not built yet. The design lives in [docs/architecture/combat.md](../../docs/architecture/combat.md) (with one file per
milestone, `combat-m1` to `combat-m5-m9`); the generic input layer is described in
[docs/architecture/input.md](../../docs/architecture/input.md). This README is the map; those documents hold the
decisions and the reasons.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatCore` | exists | Pure, deterministic logic with no GAS and no Network Prediction: the stick direction classifier, input history, buttons, the pattern grammar (`FGSTN_InputPattern`, conditions), the matcher, buffer reset rules, slots, the slot resolver, the command bindings and the default command set. Covered by Automation tests (`GSTNCombat.Core.*`). |
| `GSTNCombatNP` | exists | The adapter to the Network Prediction fork: the combat NP model (`UGSTN_CombatSimComponent`, sync/aux state, input command) and the Mover sim-state tracker. Everything that touches the ability system goes through here. |
| `GSTNCombatGAS` | planned | Native tags, attribute set, damage effect and execution, weapons and loadouts. |
| `GSTNCombatEditor` | planned | Timeline editor, slot conflict validator UI, debug drawing. |

Per input.md the generic input code (history, matcher, bindings...) is to move into a separate `GSTNInput` plugin; until
that split happens it lives in `GSTNCombatCore`. The rule that decides what stays here: anything about *fighting*
(standard buttons, command tags, default slots, hits) is combat, anything about *input in general* is input.

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

## How to extend it

All examples are in `GSTNCombatCore` and are testable without a world: add an Automation test next to the existing ones
(`Private/Tests/GSTN_InputTests_*.cpp`, helpers in `GSTN_TestUtils.h`).

### A new input condition

Conditions are structs held in the steps of a pattern; they only see the history, the button map and frame numbers.
Derive from `FGSTN_InputCondition` (`Public/Input/GSTN_InputConditions.h`):

```cpp
USTRUCT(BlueprintType, meta = (DisplayName = "Held Both"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_HeldBoth : public FGSTN_InputCondition
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
`AppendCanonicalInput` so the slot validator and the tests can build the smallest input that satisfies it. Keep
`Matches` allocation free and pure.

### A new slot (a command)

A slot ties a command tag to a pattern, a priority and a direction policy (`FGSTN_InputSlotSpec`, `GSTN_InputSlot.h`).
Slots live in `UGSTN_InputSlotAsset` / `UGSTN_InputSlotSet` data assets; the default set is built by
`UGSTN_StandardSlots`. To add one: create the command tag (`GSTN_CombatCommandTags`), add the slot to a set, then run
the slot validator (`GSTN_InputSlotValidation`), which reports slots that shadow or conflict with each other.

### A new binding

Bindings say which command a context runs (`GSTN_CommandBindings.h`, `FGSTN_TagRequirements` for the conditions on the
owner's tags). A command without a binding is never a candidate. `UGSTN_CommandBindingUtils` resolves bindings and
detects conflicts between them.

### A new buffer reset rule

Rules flush buffered presses when something happens (`GSTN_BufferResetRules.h`). Derive from `FGSTN_BufferResetRule`,
override `ShouldFlush` and, if the rule watches tags, `GetWatchedTags`. Existing examples: `OnEvent` (a combat-model event
such as a hit received) and `OnTagAdded`. Rules are listed at global, weapon and move level and applied in that order.

## Conventions

- Types are prefixed `GSTN_` (`UGSTN_`, `FGSTN_`, `EGSTN_`); export macros are `GSTNCOMBATCORE_API` / `GSTNCOMBATNP_API`.
- Public headers in `Public/<Area>/`, implementations in `Private/<Area>/`.
- Comments explain *why* and the simulation constraints. Headers document the intent of a class: read the header before
  the `.cpp`.
- Tuning values are `UPROPERTY` with `Category = "Area|Section"` and `meta` limits, grouped in small structs.

## Testing

- Pure logic: Automation tests `GSTNCombat.Core.*` (Session Frontend > Automation, or
  `UnrealEditor-Cmd GASP_Giustino2.uproject -ExecCmds="Automation RunTests GSTNCombat; Quit"`).
- Network behaviour: the PIE harness `Tools/GASNPPTest/gasnpp_test.py` (dedicated server + 2 clients), scenario
  `combat_spike`. See [testing.md](../../docs/architecture/testing.md).

## Further reading

[combat.md](../../docs/architecture/combat.md) (overview and decisions) ·
[combat-m1-core-input.md](../../docs/architecture/combat-m1-core-input.md) ·
[combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md) ·
[combat-m4-hits.md](../../docs/architecture/combat-m4-hits.md) ·
[input.md](../../docs/architecture/input.md) · [gas-npp.md](../../docs/architecture/gas-npp.md) ·
[PATCHES.md](../GASNPP/PATCHES.md)

# GSTNAnimToolsets

Editor-only AI toolsets for Epic's editor MCP (`ToolsetRegistry`, the `unreal-mcp` server on `127.0.0.1:8000`). They give an agent
what Epic's toolsets lack for animation work: Animation Blueprint state machines (in any graph, anim layers included) and Chooser
Tables. Status: 0.1, built and tested on UE 5.9. Enabled in the project (`EnabledByDefault` false, listed in the `.uproject`).

## Modules

| Module | Type | Contents |
|---|---|---|
| `GSTNAnimToolsets` | Editor, `PostEngineInit` | `UGSTN_AnimBlueprintToolset`, `UGSTN_ChooserTableToolset`, registered in `GSTNAnimToolsetsModule.cpp`. |

Dependencies: plugins `ToolsetRegistry`, `Chooser`; engine modules `AnimGraph`, `BlueprintGraph`, `UnrealEd`, `ChooserEditor`.
No GSTN plugin depends on it and it names none. No gameplay tags.

## The toolsets

**`UGSTN_AnimBlueprintToolset`** (works on nodes and graphs, so its results compose with Epic's `BlueprintToolset` and `ObjectToolset`):

| Tool | What it does |
|---|---|
| `ListAnimGraphs` | Every graph with its kind (AnimGraph, AnimLayer, StateMachine, State, TransitionRule, Conduit, ...) and path, e.g. `SkidLayer/Skid States/Skid Loop`. |
| `FindStateMachines` | Machines by name anywhere (layers, nested in states). |
| `GetStateMachine` | States/conduits/aliases with their animations, entry state, transitions with a readable rule. |
| `AddStateMachine`, `AddState`, `FindState`, `RenameState`, `RemoveState`, `SetEntryState` | Machine and state editing. The first state becomes the entry. |
| `SetStateAnimation` | Replaces/creates the asset player on the state result (sequence, blend space, ...). |
| `AddTransition`, `GetTransition`, `RemoveTransition` | Transitions. Crossfade, priority, blend mode: `ObjectToolset.SetProperties` on the transition node. |
| `SetTransitionRule` | Rule in OR-of-ANDs form (`AnyOf` groups of `AllOf` conditions). Operands: BP/parent variables, pure functions (own or static, with arguments), state machine getters (`GetRelevantAnimTimeRemaining`, `Time Remaining (ratio)` of the source state's player, ...). Compares bool, numbers, enums, names, strings. `bAutomatic` sets the automatic rule. Works on conduits too. Validates everything before touching the graph. |
| `AddAnimNode` | Any concrete `AnimGraphNode_Base` class (Slot, Blend Poses by Bool, Inertialization, cached poses, linked layers...). |

Pins, links and compile are Epic's `BlueprintToolset` (`GetNodeInfos`, `ConnectPins`, `SetPinValue`, `CompileBlueprint`).

**`UGSTN_ChooserTableToolset`**: a typed, error-raising wrapper of Epic's `UChooserTableEditorLibrary` (same behaviour as the
Chooser editor, undoable): `GetChooserTable` (signature, columns with bindings, rows with cells and results, fallback, nested
choosers in one call), `ListChooserTypes`, `ListChooserColumnInputTypes`, `AddChooserColumn`, `RemoveChooserColumn`,
`MoveChooserColumn`, `SetChooserColumnInput`, `SetChooserColumnProperty`, `AddChooserRow`, `RemoveChooserRows`, `MoveChooserRow`,
`SetChooserRowDisabled`, `SetChooserRowResult` (row -1 = fallback), `SetChooserCell`. The library's warning text becomes the error.

## Tests

`GSTN.Toolsets.AnimBlueprint` and `GSTN.Toolsets.ChooserTable` (automation specs, editor context). Fixtures live in
`/Automation/` sandboxes and are deleted afterwards; the Skid test runs on a sandbox copy of `ABP_GSTN_Locomotion`.

```
Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test_animtoolsets.log -Tests GSTN.Toolsets
```

## Extending

Add a static `UFUNCTION(meta = (AICallable, AIAccessMode = ...))` to one of the toolsets (see the `create-toolset` skill), raise
errors with `UE::ToolsetRegistry::RaiseScriptError`, and add a spec case for the success path and each error. A new toolset class
is registered in `GSTNAnimToolsetsModule.cpp`.

Third-party code: see [ThirdPartyNotices.md](ThirdPartyNotices.md) (EngineWire, MIT).

# AbilitySystemSimulation

The Network Prediction simulation of the Gameplay Ability System: the plugin that turns an ability system component into
one NP simulation per pawn, so abilities, effects, tags, attributes, cues and montages are predicted on the owning client
and rolled back and re-simulated on server corrections. It is the top layer of the GAS-NPP fork in
[`Plugins/GASNPP/`](../README.md) and needs the forked [GameplayAbilities](../GameplayAbilities/README.md) and
[NetworkPrediction](../NetworkPrediction/README.md) plugins next to it. Written by Kai / Sirkai (Yohoho Productions),
ported to UE 5.8 and patched by this project (see [PATCHES.md](../PATCHES.md)). Design notes:
[docs/architecture/gas-npp.md](../../../docs/architecture/gas-npp.md).

Status: **experimental**; `.uplugin` version 1.0 (`IsExperimentalVersion` is false in the descriptor, but the code
comments mark several areas as work in progress and the project treats it as such). `CanContainContent` is true but the
plugin ships no `Content/` folder.

## How it works in one paragraph

`UNpAbilitySystemComponent` (`Public/Abilities/NpAbilitySystemComponent.h`) is a normal `UAbilitySystemComponent` that
registers itself with the Network Prediction world manager as the model `AbilitySystemSimulation` (sort priority
`PreKinematicMovers - 2`, so it ticks before Mover). Every fixed tick it runs `SimulationTick`, which ticks the input
action events, the abilities and their prediction tasks, the montage simulator and the projectiles simulator, then
`FillSyncState` copies the component's whole state (tags, blocked ability tags, granted abilities and their instances,
active effects, attribute sets, montage state, synced target, projectiles, cues) into `FAbilitySimSyncState`. On a server
correction NP calls `RestoreFrame`, which force-grants, force-activates, force-removes and re-applies whatever differs
from the authority state, including each ability instance's prediction tasks and sync variables, and re-simulates from
there. GAS's own replication (prediction keys, ability RPCs, replicated tags, cue RPCs) is bypassed: tags are forced to
`EGameplayTagReplicationState::None`, cues become NP cues or are sent after the frame is finalized. Input comes from
Enhanced Input actions: the component binds the actions of the mapping contexts listed in the settings and sends their
state in `FAbilitySimInputCmd`.

## Modules

| Module | Type / phase | Contents |
|---|---|---|
| `AbilitySystemSimulation` | Runtime, PreDefault | Everything below. Module class `FAbilitySystemSimulationModule` (`AbilitySystemSimulationModule.cpp`) is empty. |
| `AbilitySystemSimulationEditor` | UncookedOnly, PreDefault | Blueprint support: registers `FPredictionAbilityBlueprintCompilerContext` as the compiler of `UNpGameplayAbilityBlueprint` and makes every Blueprint child of `UNpGameplayAbility` use that Blueprint class (`OverrideBPTypeForClass`); the `UK2Node_PredictionTask` graph node ("Prediction Task" category) that each task class gets automatically; details customizations for abilities and task nodes (`Private/AbilitySystemSimulationEditor.cpp`). |

Runtime module layout (`Source/AbilitySystemSimulation/Public/`):

| Folder | Main classes | Role |
|---|---|---|
| `Abilities/` | `UNpAbilitySystemComponent`, `UNpGameplayAbility` | The simulated component and the only ability base class it accepts. |
| `DataTypes/` | `FAbilitySimInputCmd`, `FAbilitySimSyncState`, `FAbilitySimAuxState`, `FAbilitySystemTimeStep` (`AbilitySimulationDataTypes.h`); `FAbilityActivationTrigger` (`AbilitiesDataTypes.h`); effects, cues, targeting and task sync data; `FBaseSyncVar` (`BaseSyncedVariableData.h`) and the concrete sync variables `FSyncBool`, `FSyncByte`, `FSyncInteger`, `FSyncInteger64`, `FSyncFloat`, `FSyncName`, `FSyncString`, `FSyncVector`, `FSyncRotator`, `FSyncObject`, `FSyncGameplayTag` (`SyncedvariableTypes/BasicSyncedVariablesTypes.h`); `FAbilityEffectLayeredMove` (`GameplayEffectLayaredMove.h`). | The NP state types and everything that is serialized and rolled back. |
| `Tasks/` | `UBasePredictionTask` and the tasks in `GASTasks/` (`Wait`, `Tick`, `TickForDuration`, `RepeatAction`, `DynamicBind`, `PlayMontageAndWait`, `PlayMontageAndWaitEvent`, `WaitAbilityActivate`, `WaitAbilityCommit`, `WaitAttributeChange`, `WaitGameplayEffectRemoved`, `WaitGameplayEvent`, `WaitGameplayTag`, `WaitInputEvent`, `WaitAnyInputEvent`, `WaitMovementModeChanged`), `TargetingTasks/` (`Targeting`, `WaitInputTargeting`), `ProjectilesTasks/` (`SpawnProjectileAndWait`); all named `U<Name>PredictionTask`. | The replacement for stock `UAbilityTask`s: per-ability-instance objects, ticked inside the simulation, with synced data that is rolled back. |
| `EffectsComponents/` | `UApplyMoveGameplayEffectComponent`, `UBaseMoveEffectCreator`, `UMoveToLayeredMoveCreator` | A gameplay effect component that queues a Mover layered move or instant movement effect when the effect is applied. The only sanctioned way for an ability to move the character. |
| `MontageSimulator/` | `UNetMontageSimulator`, `ISyncedNotifyInterface`, `FMontageSimSyncState` | Synced montage playback (play, stop, pause, section jumps, play rate) with simulated notify states and root motion handed to Mover. |
| `Targeting/` | `UTargetingProcessor`; `UTraceTargetingProcessor`, `UOverlapTargetingProcessor`, `UTraceThenOverlapTargetingProcessor` (`BasicTargetingProcessors.h`); `UAbilityTargetingFilter`, `UAbilityTargetingFilterByClass` | Const targeting logic used by the targeting tasks; `FSyncedTarget` on the component. |
| `ProjectilesSimulator/` | `UProjectilesSimulator`, `ASyncedProjectileBase`, trajectory data | Predicted projectiles owned by the component (spawn, bounce, hit, end of life). |
| `Library/` | `UAbilitySimulationLibrary`, `UTargetingLibrary` | Data extraction for the sync state and Blueprint helpers. |
| `Compilation/` | `UNpGameplayAbilityBlueprint`, `UNpAbilityGeneratedClass` | Blueprint asset and generated class types for abilities. |
| `AbilitySimulationSettings.h` | `UAbilitySimulationSettings` | Project Settings > Ability Simulation Settings: `AbilitySystemMappingContexts`. Config section `[/Script/AbilitySystemSimulation.AbilitySimulationSettings]` in `DefaultGame.ini`. |

## Dependencies

Plugins (`AbilitySystemSimulation.uplugin`, all non-optional): `GameplayAbilities`, `NetworkPrediction`,
`NetworkPredictionExtras`, `EnhancedInput`, `Mover`, `MotionWarping`. In this project the first two are the forks in
`Plugins/GASNPP/` and Mover is the copy in `Plugins/EngineOverrides/Mover`.

Modules (`*.Build.cs`):

- `AbilitySystemSimulation` public: `Core`, `GameplayAbilities`, `GameplayTasks`, `GameplayTags`, `NetworkPrediction`,
  `EnhancedInput`, `Mover`, `MotionWarping`, `NetCore`; private: `CoreUObject`, `Engine`, `Slate`, `SlateCore`,
  `DeveloperSettings`. No module of `NetworkPredictionExtras` is referenced, although the plugin is listed as a dependency.
- `AbilitySystemSimulationEditor` public: `AbilitySystemSimulation`, `BlueprintGraph`, `GameplayAbilities`, Slate,
  Engine; private: `UnrealEd`, `KismetCompiler`, `Kismet`, `GraphEditor`, `PropertyEditor`, `EditorFramework`.

What depends on it: the project module (`Source/GASP_Giustino2/GASP_Giustino2.Build.cs`), `GSTNInputNP`
(`UGSTN_InputSimComponent` drives the component from `OnSimulationPreTick`) and `GSTNCombatNP`
(`UGSTN_CombatSimComponent`, same hook). The pure Core modules `GSTNInputCore` and `GSTNCombatCore` and the GAS-only
`GSTNCombatGAS` do not depend on it, by design.

## Gameplay tags

The plugin declares no gameplay tags: no `UE_DEFINE_GAMEPLAY_TAG*` and no `Config/Tags/*.ini`. Tags used in its
Blueprint-facing API (targeting state and confirmation tags, `SetByCaller` tags, the synced target type) are supplied by
the project.

## Project changes vs. the upstream fork

The fork was imported in one commit (`53c8716`), so git does not separate the upstream author's work from the port to
5.8. The changes made in this project after the import are the ones in [PATCHES.md](../PATCHES.md):

| Change | Where |
|---|---|
| `UNpAbilitySystemComponent::OnSimulationPreTick` delegate, broadcast at the start of every (re)simulated tick so an earlier-sorted simulation can activate, cancel or pause montages with the right frame (`d6039e2`). | `Public/Abilities/NpAbilitySystemComponent.h`, `Private/Abilities/NpAbilitySystemComponent.cpp` |
| `FMontageSimSyncState::ShouldReconcile` ignores `bIsPaused` when no montage plays (`3520b35`). | `Private/MontageSimulator/NetMontageSimulatorData.cpp` |
| `FActiveEffectSyncData::ShouldReconcile` names each compared field with `UE_NP_TRACE_RECONCILE` so `np.LogReconcileField` can attribute an effect mismatch (`86d7afc`, diagnostic only). | `Private/DataTypes/EffectsDataTypes.cpp` |
| `UNpAbilitySystemComponent::MonitoredTagChanged` returns at once while `GetIsRestoringFrame()`: `RestoreTags` broadcasts the tag delegates, and an owned-tag trigger fired inside `RestoreFrame` double- or phantom-activated an ability on the owning client; the re-simulation fires the trigger again from the tag changes it simulates (`4838224`, PATCHES.md 1, with the re-apply recipe). | `Private/Abilities/NpAbilitySystemComponent.cpp` |
| `FAbilitySimInputCmd::PrepareRepeatedInput()`: clears one-shot input action events when NP repeats an input after a hitch (`5a5fbcd`). | `Public/DataTypes/AbilitySimulationDataTypes.h` |
| Spike S-FP (forward prediction of simulated proxies), inert behind `np.Spike.SPPredictActivation` and `np.Spike.SPSendAttributeBase` (both default 0): `CanPredictAsSimulatedProxy()`, `bInSimulationTick` (`bdfa7e8`). | `NpAbilitySystemComponent.h/.cpp`, `Private/Abilities/NpGameplayAbility.cpp`, `Private/DataTypes/EffectsDataTypes.cpp` |

The UE 5.8 adaptations of the tag API (`EGameplayTagReplicationState`, the removed `Add/RemoveReplicatedLooseGameplayTag`)
are commented in `NpAbilitySystemComponent.h` ("UE 5.8: ...") and belong to the port itself.

## How to extend

The rules in [gas-npp.md](../../../docs/architecture/gas-npp.md) ("Hard rules for simulation code") apply to all of
this: deterministic code, state in sync variables, no net-role branches, no game-frame component reads.

### Writing a predicted ability

**C++.** Derive from `UNpGameplayAbility`; in this project from `UGSTN_GameplayAbility`
(`Source/GASP_Giustino2/Public/GAS/GSTN_GameplayAbility.h`), which adds the Mover helpers and `AddActivationInput`.
A stock `UGameplayAbility` subclass is refused by `GiveAbility` (error log "not child of UNppGameplayAbility").

- Input: fill `ActivationInputs` (`FAbilityActivationTrigger`: an `UInputAction` and an `ETriggerEvent`) in the
  constructor; `GetActivatedByTrigger` tells which one fired. `UGSTN_GA_Jump` does
  `AddActivationInput(TEXT("/Game/Input/IA_Jump.IA_Jump"))`. The action must come from a mapping context listed in
  `AbilitySystemMappingContexts`; the project binds them through `UGSTN_AbilityInputComponent`.
- Activation: override `ActivateAbility` as usual; relationships to other abilities go through the standard tag
  settings (`ActivationOwnedTags`, `ActivationBlockedTags`, cancel/block tags), as `UGSTN_GA_Jump` and
  `UGSTN_GA_WallSlide` do with `State.Movement.*`.
- Per-frame logic: override `OnSimulationTick(const FAbilitySystemTimeStep&)`, called after the ability's prediction
  tasks each simulated frame. `UGSTN_GA_WallSlide` ends itself there when Mover leaves `WallModeNames`.
- State across frames: a `UPROPERTY()` member of a `FSync*` type (`FSyncInteger FramesSinceActivation` in
  `UGSTN_GA_WallSlide` and `UGSTN_GA_TestAttack`, `GASNPPTest/GSTN_TestCombatAbilities.h`). A plain member is not rolled
  back and, with `InstancedPerActor`, not reset on reactivation either; reset it in `EndAbility`.
- Restore: `OnAbilityRestored()` (and the Blueprint event `K2_OnAbilityRestored`) runs when a correction re-activates
  the instance after its tasks and sync vars were restored; `IsRestoringFrame()` tells whether the component is inside
  a restore.
- Waiting: use the `*PredictionTask` classes, not `UAbilityTask`. From C++ add them in the constructor with
  `AddNativePredictionTask(TaskClass, ObjectInitializer)` and keep the returned `FGuid` (`GetTaskFromGuid`); the task
  header recommends keeping the graph in Blueprint and the logic in C++.
- Costs, cooldowns, instancing: standard `UGameplayAbility` settings. The test abilities
  (`GASNPPTest/GSTN_TestAbilities.h`: `UGSTN_GA_TestDash` with a stamina cost and a cooldown effect) show the plain case.

**Blueprint.** A Blueprint child of any `UNpGameplayAbility` class is created the normal way (right click the class or
"Create Blueprint class"); the editor module gives it the `UNpGameplayAbilityBlueprint` type and compiler, so the graph
has the Prediction Task nodes (one node per task class, delegates as output pins, inputs of the start function as
input pins). Available in Blueprint: `ActivationInputs`, `GetActivatedByTrigger`, `GetPredictionTaskByName`,
`CancelPredictionTaskByName`, `GetPredictionTasks`, `GetNpAbilitySystemComponent`, `IsRestoringFrame`, the event
`K2_OnAbilityRestored`, and the sync variable structs (all `BlueprintType`) as ability variables. The project's gameplay
abilities are C++ classes with Blueprint children used only for tuning (`docs/architecture/gas-npp.md`, "Where the
Blueprints are").

### Moving the character from an ability

An ability never writes location or velocity. It applies a gameplay effect that carries
`UApplyMoveGameplayEffectComponent`; the component's `MoveProcessor` (a `UBaseMoveEffectCreator`, `Instanced`) builds
the move in `MakeMove` (`BlueprintNativeEvent`, so a creator can be a Blueprint class or C++) and the component queues
it on the target's Mover: a layered move for duration effects (`FAbilityEffectLayeredMove` stops when the effect ends
and pauses when it is inhibited, per `FLayeredMoveDefaultParams`), an instant movement effect for instant ones.
Examples: `UGSTN_JumpImpulseMoveCreator` + `UGSTN_GE_Jump` (`GAS/GSTN_GA_Jump.h`), `UGSTN_DashMoveCreator` +
`UGSTN_GE_Dash` (`GASNPPTest/GSTN_TestEffects.h`), and the plugin's own `UMoveToLayeredMoveCreator`. In Blueprint,
`Fill Move Container` (`K2_FillMoveContainer`) turns a Mover move struct into the `FEffectMoveContainer` output.

### Effects, attributes, cues

Gameplay effects and attribute sets are the stock classes (`UGameplayEffect` subclasses or data-only Blueprints,
`UAttributeSet` with `FGameplayAttributeData`); nothing changes in how they are authored. What changes is timing and
replication: effect start times and durations come from the simulation clock (`GetCurrentSimulationTimeMS`), attribute
sets added through `DefaultAttributes` are serialized into the sync state and restored on rollback, and cue events fire
from `FinalizeFrame` so a predicted cue is not played twice during re-simulation. Modify attributes only through
effects (`GSTN_TestAttributeSet` + `UGSTN_GE_StaminaRegen` in `GASNPPTest/` is the project example). Local-only tags
are the exception: tags added with the `AddNonReplicatedLooseTag(s)` functions the fork added to
`UAbilitySystemComponent` (the Blueprint library uses them when the replication state is `None`) are removed from the
sync state in `FillSyncState`, so they are neither sent nor rolled back.

### Other extension points

| Extension point | Kind | Notes |
|---|---|---|
| `UBasePredictionTask` | C++ only | Subclass, set `StartTaskFunctionName` in the constructor, `ShouldTaskTick` for ticking tasks, a `FAbilityTaskDataBase` subtype as synced data with `WriteToSyncedData` / `ReadFromSyncedData`, react in `StartTaskRollback`, and always check `CanEffectSimulation()` before broadcasting delegates. The header of `Tasks/BasePredictionTask.h` is the reference. The Blueprint node is generated for the new class. |
| `FBaseSyncVar` | C++ only | New sync variable types (custom structs) follow `BasicSyncedVariablesTypes.h`; they need `NetSerialize`. |
| `UNpAbilitySystemComponent` | C++ | Subclass for `GetMontagePlayRateMultiplier` and project defaults; other simulations hook `OnSimulationPreTick` (combat model, input simulation). `CustomInput` (`FInstancedStruct`) plus `OnPreProduceInput` / `OnPostProduceInput` (Blueprint-assignable) carry extra per-frame input; `bSendCameraLocation`, `bSendMouseScreenLocation` add the camera and mouse to the input. |
| `UBaseMoveEffectCreator` | C++ or Blueprint | `MakeMove` is a `BlueprintNativeEvent`; `ApplyMove` is virtual for creators that apply something other than a layered or instant move. |
| `UTargetingProcessor`, `UAbilityTargetingFilter` | C++ or Blueprint | `Blueprintable`, `EditInlineNew`; all targeting functions are const and get their state from the task's synced inputs. `ValidHitResult` / `ValidHitActor` are `BlueprintNativeEvent`s. |
| `ASyncedProjectileBase` | C++ or Blueprint | `Blueprintable` actor with the trajectory and bounce parameters; the hit reaction is the projectile's. Spawned through `UNpAbilitySystemComponent::SpawnProjectile` from simulation code. |
| `ISyncedNotifyInterface` | C++ or Blueprint | Anim notify states that take part in the simulation implement `SimulationBegin` / `SimulationTick` / `SimulationEnd` / `RestoreFrame` (`BlueprintNativeEvent`s). `AnimNotifyState_SyncedSkewWarping` (private) is the shipped example. |

Must stay in C++: new prediction tasks and their sync data, new sync variable types, new simulators or new fields of
`FAbilitySimSyncState`, and anything with `NetSerialize`. These change the wire format: both ends need the same build.

## Using it standalone in another project

1. Copy the whole `Plugins/GASNPP/` folder (this plugin alone does not build: it needs the `GameplayAbilities` fork, whose
   `Np*` functions and `GetCurrentSimulationTimeMS` it calls, and the forked `NetworkPrediction`). Project plugins with
   the names `GameplayAbilities` and `NetworkPrediction` override the engine ones; the `GameplayAbilities` fork's
   `Build.cs` adds an engine Internal include path for that reason.
2. Copy `Plugins/EngineOverrides/Mover` (and `ChaosMover`), because Mover must be built against the forked NP and
   implements the `PrepareRepeatedInput` hook; see that folder's README.
3. Enable `AbilitySystemSimulation`, `GameplayAbilities`, `NetworkPrediction`, `NetworkPredictionExtras`,
   `EnhancedInput`, `Mover`, `MotionWarping` in the `.uproject`; add `GameplayAbilities` and `AbilitySystemSimulation`
   to the game module's `Build.cs`.
4. Network Prediction settings: fixed ticking policy (the Mover README recommends Fixed, Interpolated simulated proxies,
   fixed tick smoothing). The project runs at 62.5 Hz.
5. `Config/DefaultGame.ini`: list the mapping contexts whose actions activate abilities under
   `[/Script/AbilitySystemSimulation.AbilitySimulationSettings]` `+AbilitySystemMappingContexts=...`.
6. On the pawn: a `UNpAbilitySystemComponent` (`DefaultAbilities`, `DefaultAttributes`, `DefaultEffects`) next to a
   Mover component, and something that calls `AddMappingContext` once the pawn is locally controlled (this project's
   `UGSTN_AbilityInputComponent`; for a Blueprint-only setup the component offers `RefreshAvatarActor`).

Assumptions: the ASC lives on the pawn, not the PlayerState (the component casts its owner to `APawn` to find the
controller and the NP player controller component; stock GAS advice on PlayerState-owned ASCs does not apply); the pawn
uses Mover (the move effect component and `GetSyncedControlRotation` expect a Mover component); Enhanced Input. The
project's own classes (`GSTN_*`) are not needed.

## Where to read more

[gas-npp.md](../../../docs/architecture/gas-npp.md) (the fork, tick order, project classes, hard rules) ·
[combat.md](../../../docs/architecture/combat.md) sections 12.7 and 17 (network model, spike) ·
[testing.md](../../../docs/architecture/testing.md) (PIE harness: scenarios `abilities`, `forced_rollback`,
`server_dash`, `combat_spike`) · [PATCHES.md](../PATCHES.md) · [GASNPP/README.md](../README.md) ·
the `add-gstn-ability` skill (`.claude/skills/add-gstn-ability/SKILL.md`) for the project checklist.

# GAS + Network Prediction ("GAS-NPP fork")

Source: `Source/GASP_Giustino2/{Public,Private}/GAS/`, plugins in `Plugins/GASNPP/`.

## What the fork changes versus stock GAS

- `Plugins/GASNPP/GameplayAbilities` and `Plugins/GASNPP/AbilitySystemSimulation` replace the stock Gameplay Ability System. The ability system is **simulated by the Network Prediction plugin**: activation, effects, tags and attributes are predicted on the owning client and **rolled back and re-simulated** on server corrections, instead of GAS's own prediction keys.
- The component is `UNpAbilitySystemComponent`; abilities must derive from `UNpGameplayAbility` (the fork rejects stock `UGameplayAbility`).
- Ability-side per-tick logic goes in `OnSimulationTick(const FAbilitySystemTimeStep&)`. Waiting is done with `*PredictionTask` classes (`Tasks/GASTasks/`) rather than stock `UAbilityTask`s.
- Ability state that must survive a rollback is a **sync variable** (`FSyncInteger`, ... in `DataTypes/SyncedvariableTypes/BasicSyncedVariablesTypes.h`). A plain member is not rolled back. Example: `FramesSinceActivation` in `UGSTN_GA_WallSlide`.
- Input: abilities declare `ActivationInputs`; the ability system binds the Enhanced Input actions of the mapping contexts listed in Project Settings > Ability Simulation Settings (`AbilitySystemMappingContexts` in `Config/DefaultGame.ini`) and sends their state with the predicted input.
- Local patches to the fork (details in [combat.md](combat.md) section 17): on rollback every instance's state view is set to the restored frame before re-simulation; `UNpAbilitySystemComponent::OnSimulationPreTick` lets another simulation drive the ASC from inside its tick; the montage pause flag counts in `ShouldReconcile` only while a montage plays.
- Stock GAS docs (README in `Plugins/GASNPP/GameplayAbilities/README.md`, tranek's GASDocumentation) still describe concepts (attributes, tags, effects, cues) correctly. They are **wrong for prediction, replication and ability tasks** in this project.

## Tick order (important)

Fixed tick: **ability system first, then Mover**.

- Tags and effects granted in frame N are visible to Mover's simulation in frame N.
- A Mover mode change in frame N is visible to abilities in frame N+1. That is why `UGSTN_GA_WallSlide` has `EntryGraceFrames`.

## Project classes

| Class | Role |
|---|---|
| `UGSTN_GameplayAbility` (abstract) | Base of all project abilities. Helpers: `GetMoverComponentFromActorInfo`, `GetCharacterMoverFromActorInfo`, `GetMoverSimState`, `AddActivationInput` (constructor only). |
| `UGSTN_GA_Jump` | Ground jump. Applies `UGSTN_GE_Jump` whose `UGSTN_JumpImpulseMoveCreator` queues Mover's jump impulse. Turns off `UCharacterMoverComponent` built-in jump on the avatar (`bTakeOverMoverJump`). |
| `UGSTN_GA_WallSlide` | "Being on a wall". No input. Owns `State.Movement.WallSliding`; ends when Mover leaves `WallModeNames`. Movement itself stays in the wall modes. |
| `UGSTN_AbilityWallEntryTransition` | Mover transition (Falling -> wall mode) that grabs a wall only if an ability with `AbilityTags` is granted and `CanActivate`, then activates it. |
| `UGSTN_MoverSimStateTracker` | Component (FindOrAdd on the Mover owner). Mirrors Mover's sync state per simulated/rolled-back frame. Lives in the GSTNCombat plugin (`GSTNCombatNP/Mover/`, include `"Mover/GSTN_MoverSimStateTracker.h"`) because the combat model needs it too; the project module depends on `GSTNCombatNP`. |
| `UGSTN_AbilityInputComponent` | Binds the local player's Enhanced Input to the pawn's `UNpAbilitySystemComponent` once controlled. |
| `GSTNAbilityTags::*` | Native tags: `Ability.Movement.Jump`, `Ability.Movement.WallSlide`, `State.Movement.WallSliding`, `State.Movement.WallSlideBlocked`, `State.Movement.JumpBlocked`. |

## Hard rules for simulation code

1. **Read movement state from `GetMoverSimState(ActorInfo)`**, not from `UMoverComponent::GetMovementModeName / HasGameplayTag / IsOnGround`. The component returns the state cached for the *game* frame; during re-simulation it gives the newest state, so the ability decides differently from the server and causes reconciles.
2. **Move the character only through Mover queued moves**: a gameplay effect carrying `UApplyMoveGameplayEffectComponent` (+ a `UBaseMoveEffectCreator`, see `UGSTN_JumpImpulseMoveCreator`). Never `SetActorLocation`, never write velocity.
3. **No networking branches** (`HasAuthority`, "is resimulating") inside simulated logic. Same inputs must give the same outputs on client and server.
4. State needed across frames = sync variable or Mover sync/aux state. Everything else must be reconstructible.
5. Block/cancel relationships between abilities are expressed with tags (`State.Movement.*`), set via `ActivationOwnedTags`, `ActivationBlockedTags`, or effects. Not with direct calls between abilities.
6. Attribute changes only through gameplay effects.
7. Cooldown and cost are the standard `UGameplayAbility` settings; set them on a Blueprint child, not in C++ constants.
8. Instancing: `InstancedPerActor` unless there is a reason. With it, variables are **not reset on end**: reset them in `EndAbility` (or use sync vars).
9. An NP input type with edge-triggered data (a "just pressed" bit) implements `void PrepareRepeatedInput()`: clear the edges, keep the held state. When a client's input does not reach the server in time (starved, or lost), the server repeats the last input it has, and this keeps one press from acting twice. Implemented by `FAbilitySimInputCmd` (one-shot action events), `FGSTN_CombatInputCmd` (`PressedButtons`, `ForcedCommand`) and Mover's `FMoverInputCmdContext` (`FCharacterDefaultInputs::bIsJumpJustPressed` and `SuggestedMovementMode`; an engine-override change in `MoverSimulationTypes.h/.cpp`, marked "GAS-NPP fork", to carry over when re-syncing Mover). A new Mover input struct with an edge is cleared in `FMoverInputCmdContext::PrepareRepeatedInput`.

## Where the Blueprints are

- `SandboxCharacter_Mover` (`/Game/Blueprints/`) owns the `NpAbilitySystem` (DefaultAbilities: GA_Jump, GA_WallSlide) and the `GSTN_AbilityInputComponent`. The Falling mode's transition is `GSTN_AbilityWallEntryTransition`.
- `BP_GASNPP_TestCharacter` (`/Game/GASNPPTest/`) inherits that setup and overrides the ability system with test content (Dash/Buff, `GSTN_TestAttributeSet`, `GSTN_GE_StaminaRegen`) plus `GSTN_GASNPPTestComponent`.
- `/Game/GAS/Input/IMC_GAS_Abilities` lists the ability input actions (IA_Jump, keys copied from `IMC_Sandbox`).
- `Tools/GAS/setup_gas_assets.py` can (re)create the asset setup idempotently; see its header for the current layout.

## Adding things

- New ability: use the `add-gstn-ability` skill (`.claude/skills/`).
- New tag: declare in `GSTN_AbilityTags.h`, define in `GSTN_AbilityTags.cpp`, document the intent in a one-line comment.
- New attribute set: derive from `UAttributeSet` as in `GASNPPTest/GSTN_TestAttributeSet.h`; add through the ASC's defaults, modify only via effects.

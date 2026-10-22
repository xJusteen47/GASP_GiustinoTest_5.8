---
name: add-gstn-ability
description: Add or modify a gameplay ability, gameplay effect, native tag or attribute in this project's GAS-NPP (Network Prediction) setup. Use for any task like "add a dash/crouch/slide ability", "new GA_", "grant a tag", "make X an ability", or when touching Source/GASP_Giustino2/**/GAS/.
---

# Add a GSTN gameplay ability

First read `docs/architecture/gas-npp.md` (rules for simulation code). Then follow this checklist.

## 1. Decide the shape

- Does it move the character? Then it needs a **gameplay effect** with `UApplyMoveGameplayEffectComponent` and a `UBaseMoveEffectCreator` subclass (model: `UGSTN_JumpImpulseMoveCreator` + `UGSTN_GE_Jump` in `GAS/GSTN_GA_Jump.h`). The ability applies the effect; it never sets velocity or location.
- Is it a *state* (like being on a wall) rather than an action? Model it on `UGSTN_GA_WallSlide`: no input, owns a state tag, ends by observing Mover.
- Does it need to be triggered by Mover (a transition)? Model it on `UGSTN_AbilityWallEntryTransition`: the transition checks `CanActivate` and activates the ability.

## 2. Write the class

- Derive from `UGSTN_GameplayAbility` (never `UGameplayAbility`: the fork only accepts `UNpGameplayAbility`).
- Header in `Public/GAS/GSTN_GA_<Name>.h`, source in `Private/GAS/GSTN_GA_<Name>.cpp`; first line comment `// Gameplay Ability System setup (GAS-NPP fork).`; class doc comment explains behaviour, input, tags owned/blocked, and the rollback-relevant constraints, as in the existing abilities.
- Input: set `bRequiresActivationInput = true;` in the constructor and name the action in `Config/DefaultGame.ini`: `[/Script/GASP_Giustino2.GSTN_GA_<Name>]` / `ActivationInput=(Action="/Game/Input/IA_<Name>.IA_<Name>",Trigger=Started)`. No content path in C++ (`FGSTN_SoftActivationInput` resolves it when the ability is granted and logs an error if it is missing).
- Per-frame logic goes in `OnSimulationTick(const FAbilitySystemTimeStep&)`. State carried across frames is an `FSync*` variable (`UPROPERTY() FSyncInteger ...`), not a plain member.
- Read movement state with `GetMoverSimState(ActorInfo)`, not Mover component queries.
- No `HasAuthority()` / resimulation branches. Reset instance variables in `EndAbility` if they are not sync vars.
- Relationships to other abilities via tags: `ActivationOwnedTags`, `ActivationBlockedTags`, `CancelAbilitiesWithTag`, `BlockAbilitiesWithTag`. Declare new native tags in `GAS/GSTN_AbilityTags.h` (`UE_DECLARE_GAMEPLAY_TAG_EXTERN`) and define them in the `.cpp`, with a one-line comment on who grants/reads them.
- Tuning values: `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "<Name>", meta = (ClampMin, ForceUnits))`.

## 3. Wire the assets (ask the user first: Blueprints are binary, LFS, no locks)

1. Input action asset in `/Game/Input/` and its key in the player's mapping context (`IMC_Sandbox`); the same action in `/Game/GAS/Input/IMC_GAS_Abilities`.
2. `IMC_GAS_Abilities` listed under Ability System Mapping Contexts (`Config/DefaultGame.ini`, already the case).
3. Add the ability class to the `NpAbilitySystem` DefaultAbilities on `SandboxCharacter_Mover`.
4. Prefer extending `Tools/GAS/setup_gas_assets.py` (idempotent, backs up Blueprints to `Saved/Backups/GAS`) or the `unreal-mcp` tools over hand-editing.

## 4. Verify

- Build (Live Coding for `.cpp` only; full build if headers/reflection changed).
- Run the harness from `docs/architecture/testing.md`: `scenario_abilities`, `scenario_forced_rollback` with lag/jitter/loss. Server and client snapshots must match and there must be no unforced reconciles.
- If the ability interacts with walls, also run `scenario_jump_wall` and `force_reconcile_on_mode`.

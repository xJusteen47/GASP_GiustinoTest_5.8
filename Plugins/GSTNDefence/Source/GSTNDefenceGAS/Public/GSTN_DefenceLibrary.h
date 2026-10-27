// Defence, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_ShieldAbilityData.h"
#include "Data/GSTN_DefenceRules.h"
#include "Input/GSTN_CommandBindings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPath.h"
#include "GSTN_DefenceLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGSTN_DefenceAttributeSet;
class UGSTN_PrimaryKitDefinition;
struct FGSTN_BindingInput;
struct FGSTN_CompileInput;
struct FGSTN_DefenceKitExtension;
struct FGSTN_GetupSet;
struct FGSTN_LoadoutExtensionContext;
struct FGSTN_TechSet;

/** The get-up abilities of a kit with their classes checked (UGSTN_DefenceLibrary::ResolveGetupSet). Null = none or not an ability */
USTRUCT(BlueprintType)
struct GSTNDEFENCEGAS_API FGSTN_ResolvedGetupSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Getup")
	TSubclassOf<UGameplayAbility> GroundBounce;

	UPROPERTY(BlueprintReadOnly, Category = "Getup")
	TSubclassOf<UGameplayAbility> Roll;

	UPROPERTY(BlueprintReadOnly, Category = "Getup")
	TSubclassOf<UGameplayAbility> Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Getup")
	TArray<TSubclassOf<UGameplayAbility>> Attacks;
};

/**
 * Entry points of the defence plugin for the loadout, for scripts and for the abilities of the NP module (m6 spec 1d, 4, 6).
 * Everything here runs OUTSIDE the simulation (loadout apply, grant, tools) except the pure decisions, which take their inputs as
 * arguments and read no world: CanExecuteShield and GetupNeutralRequirements are what the abilities call from a tick.
 *
 * Kit- and mode-dependent defence (spec 1d): the one availability test is UGSTN_DefenceMath::IsShieldAvailable(rules, kit has a
 * shield). Where it is false the loadout contribution builds no Command.Defence.Shield binding (the slot Input.Button.Shield
 * stays free for the kit's own binding), the attribute set is not granted, and the shield ability refuses as a safeguard.
 */
UCLASS()
class GSTNDEFENCEGAS_API UGSTN_DefenceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Rules and availability ----

	/** The id this plugin registers its loadout extension under */
	static FName GetLoadoutExtensionId();

	/** The defence entry of a kit's extension list, null when the kit has none (then it has no shield and no defence abilities) */
	static const FGSTN_DefenceKitExtension* FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit);

	/**
	 * The rules a fighter plays: the kit extension's own asset, else the match's (the defence extension of the combat rules asset
	 * found through WorldContext, null = no match), else the plugin settings' default; null when none names an asset. Loads
	 * synchronously: for the loadout and tools, never from a tick. Every machine resolves the same asset (soft references, same data)
	 */
	static const UGSTN_DefenceRules* ResolveRules(const FGSTN_DefenceKitExtension* Extension, const UObject* WorldContext);

	/** IsShieldAvailable for a kit in a world: the resolved rules enable the shield and the kit's extension names a shield ability */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence", meta = (WorldContext = "WorldContext"))
	static bool IsShieldAvailableForKit(const UGSTN_PrimaryKitDefinition* Kit, const UObject* WorldContext);

	// ---- Decisions the abilities make (pure) ----

	/**
	 * Whether the shield command may execute (spec 5.3, 4): the shield is available, the value is above zero, the shield is not
	 * breaking, and the entity is grounded (no wall or air shield). OutReason names the first failing rule. The NP shield ability
	 * calls it from CanExecute with values read from the combat state of the frame, so the answer is the same on every machine
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static bool CanExecuteShield(const UGSTN_DefenceRules* Rules, bool bKitHasShield, double ShieldValue, bool bBreaking, bool bGrounded, EGSTN_ShieldRefusal& OutReason);

	/** The context the neutral get-up needs: Combat.State.Knockdown required, nothing ignored (spec 6: Press(Jump) while downed) */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_TagRequirements GetupNeutralRequirements();

	// ---- Kit extension resolution (loads classes: outside the simulation) ----

	/** The ability class a soft UObject class of the kit extension names; null (with a warning naming What) when it is empty or not an ability */
	static TSubclassOf<UGameplayAbility> ResolveAbilityClass(const TSoftClassPtr<UObject>& SoftClass, const TCHAR* What);

	/** The tech abilities of a kit by tech kind, classes checked; an entry that is not an ability is dropped. Empty for a kit without tech */
	static void ResolveTechSet(const FGSTN_TechSet& Tech, TMap<EGSTN_TechType, TSubclassOf<UGameplayAbility>>& OutAbilities);

	/** The get-up abilities of a kit, classes checked */
	static FGSTN_ResolvedGetupSet ResolveGetupSet(const FGSTN_GetupSet& Getup);

	// ---- Loadout contribution (the registered loadout extension; FGSTN_LoadoutExtensionRegistry) ----

	/**
	 * The shield binding: Command.Defence.Shield, requiring Input.Context.Grounded, anchored on the press of Input.Button.Shield with
	 * the button held (Hold(Shield, 0), spec 5.3) as a slot override, so the kit's command set needs no shield slot of its own
	 */
	static FGSTN_BindingInput MakeShieldBindingInput(const FSoftObjectPath& ShieldAbility);

	/** The neutral get-up binding: Command.Movement.Jump with GetupNeutralRequirements, no slot override (the jump slot is the kit's) */
	static FGSTN_BindingInput MakeGetupNeutralBindingInput(const FSoftObjectPath& NeutralAbility);

	/**
	 * What a kit's defence extension adds to the compile input: the shield binding where the shield is available, the get-up
	 * neutral binding, and the tech, ground bounce, get-up roll and get-up attack classes as triggered abilities (granted and in the
	 * timeline registry, so the model can start them Forced). A kit without the extension adds nothing. Deterministic over the
	 * resolved assets, as every machine compiles the same table
	 */
	static void ContributeToLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	/**
	 * Grants the attribute set, initialised from the rules, where the shield is available for the kit; nothing otherwise. Authority
	 * only, called by the loadout component after the abilities. Granting twice does nothing (the set is found by class)
	 */
	static void GrantDefence(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem);

	/** Removes the attribute set GrantDefence added, if any */
	static void RevokeDefence(UAbilitySystemComponent& AbilitySystem);

	/** The defence attribute set of an ability system, null where the shield is not available for it */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static const UGSTN_DefenceAttributeSet* FindDefenceAttributeSet(const UAbilitySystemComponent* AbilitySystem);
};

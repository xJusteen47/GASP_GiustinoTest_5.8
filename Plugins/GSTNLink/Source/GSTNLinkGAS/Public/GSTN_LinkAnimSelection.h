// Links, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_LinkAnimSelection.generated.h"

class UAnimInstance;
class UGSTN_LinkRegistry;
class UGSTN_PrimaryKitDefinition;
class UGSTN_WeaponDefinition;
struct FGSTN_LinkKitExtension;
struct FGSTN_LinkState;

/**
 * Which animation layer an entity plays while it holds a link role (m6 spec 4: the grabbed side has no ability; its reaction is
 * state, the linked Mover mode and the role's anim layer, chosen by the kit). It follows the M3 presentation pattern: the kit and the
 * weapon carry soft anim layer classes as data (FGSTN_KitLinkAnims per role tag in the link kit extension), and presentation links the
 * selected layer into the main animation blueprint; nothing here loads a class, so a dedicated server never pulls an animation.
 *
 * Lookup order: the weapon's link extension, then the kit's, as the weapon's anim layer is the more specific one in M3. A role with no
 * entry in either gives a null soft class: the entity plays nothing extra. The link type knows no animation, so a new link type needs
 * only the kits that want a pose for its roles to list them.
 */
UCLASS()
class GSTNLINKGAS_API UGSTN_LinkAnimSelection : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The anim layer an extension lists for a role tag; null when the extension is null or has no entry */
	static TSoftClassPtr<UAnimInstance> FindRoleAnimLayer(const FGSTN_LinkKitExtension* Extension, const FGameplayTag& RoleTag);

	/** The anim layer of a role for a kit and its current weapon (weapon first, then kit); either may be null */
	UFUNCTION(BlueprintPure, Category = "Link|Animation")
	static TSoftClassPtr<UAnimInstance> SelectRoleAnimLayer(const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* Weapon,
		FGameplayTag RoleTag);

	/**
	 * The anim layer for a channel's link state: the state's type index and role give the role tag through the registry, then as
	 * SelectRoleAnimLayer. Null for an idle state, an unknown type or a role with no entry
	 */
	static TSoftClassPtr<UAnimInstance> SelectForLinkState(const UGSTN_LinkRegistry& Registry, const FGSTN_LinkState& State,
		const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* Weapon);

	/** Every role anim layer the kit and weapon list, without duplicates: what a client preloads so the first link does not hitch */
	static void GatherRoleAnimLayers(const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* Weapon,
		TArray<TSoftClassPtr<UAnimInstance>>& OutLayers);
};

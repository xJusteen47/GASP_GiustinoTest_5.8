// Links, ability-system half (no Network Prediction, no Mover).

#include "GSTN_LinkAnimSelection.h"

#include "Animation/AnimInstance.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "GSTN_LinkRegistry.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "State/GSTN_LinkState.h"

namespace GSTNLinkAnimPrivate
{
	const FGSTN_LinkKitExtension* WeaponExtension(const UGSTN_WeaponDefinition* Weapon)
	{
		return Weapon != nullptr ? Weapon->Extensions.Find<FGSTN_LinkKitExtension>() : nullptr;
	}

	const FGSTN_LinkKitExtension* KitExtension(const UGSTN_PrimaryKitDefinition* Kit)
	{
		return Kit != nullptr ? Kit->Extensions.Find<FGSTN_LinkKitExtension>() : nullptr;
	}
}

TSoftClassPtr<UAnimInstance> UGSTN_LinkAnimSelection::FindRoleAnimLayer(const FGSTN_LinkKitExtension* Extension, const FGameplayTag& RoleTag)
{
	if (Extension == nullptr || !RoleTag.IsValid())
	{
		return nullptr;
	}
	const FGSTN_KitLinkAnims* Anims = Extension->AnimsByRole.Find(RoleTag);
	return Anims != nullptr ? Anims->AnimLayer : nullptr;
}

TSoftClassPtr<UAnimInstance> UGSTN_LinkAnimSelection::SelectRoleAnimLayer(const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* Weapon,
	FGameplayTag RoleTag)
{
	const TSoftClassPtr<UAnimInstance> FromWeapon = FindRoleAnimLayer(GSTNLinkAnimPrivate::WeaponExtension(Weapon), RoleTag);
	if (!FromWeapon.IsNull())
	{
		return FromWeapon;
	}
	return FindRoleAnimLayer(GSTNLinkAnimPrivate::KitExtension(Kit), RoleTag);
}

TSoftClassPtr<UAnimInstance> UGSTN_LinkAnimSelection::SelectForLinkState(const UGSTN_LinkRegistry& Registry, const FGSTN_LinkState& State,
	const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* Weapon)
{
	if (!State.IsLinked())
	{
		return nullptr;
	}
	return SelectRoleAnimLayer(Kit, Weapon, Registry.GetRoleTag(State.LinkTypeIndex, State.Role));
}

void UGSTN_LinkAnimSelection::GatherRoleAnimLayers(const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* Weapon,
	TArray<TSoftClassPtr<UAnimInstance>>& OutLayers)
{
	for (const FGSTN_LinkKitExtension* Extension : { GSTNLinkAnimPrivate::WeaponExtension(Weapon), GSTNLinkAnimPrivate::KitExtension(Kit) })
	{
		if (Extension == nullptr)
		{
			continue;
		}
		for (const TPair<FGameplayTag, FGSTN_KitLinkAnims>& Pair : Extension->AnimsByRole)
		{
			if (!Pair.Value.AnimLayer.IsNull())
			{
				OutLayers.AddUnique(Pair.Value.AnimLayer);
			}
		}
	}
}

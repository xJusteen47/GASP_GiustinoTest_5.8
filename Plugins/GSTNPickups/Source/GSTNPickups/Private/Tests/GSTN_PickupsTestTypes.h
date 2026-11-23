// Equipment pickups, test types (M11 L2).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "GSTN_PickupsTestTypes.generated.h"

/** A loadout extension that stays with the player through a kit swap (stands in for a backpack, M11 L7) */
USTRUCT()
struct FGSTN_PickupsTestKeptExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;

	virtual bool StaysWithPlayerOnKitSwap() const override { return true; }
};

/** A loadout extension that follows the mode's mods rule (stands in for the mods, M11.4) */
USTRUCT()
struct FGSTN_PickupsTestModsExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;

	virtual bool StaysWithPlayerOnKitSwapUnder(const FGSTN_KitSwapCarryOver& Rules) const override { return Rules.bModsStayWithPlayer; }
};

/** A loadout extension that travels with the kit (the default) */
USTRUCT()
struct FGSTN_PickupsTestPlainExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;
};

/** A loadout component that answers the asset ids it is told and grants into no ability system (the grant stays pending) */
UCLASS(Hidden)
class UGSTN_PickupsTestLoadoutComponent : public UGSTN_LoadoutComponent
{
	GENERATED_BODY()

public:
	TMap<FPrimaryAssetId, TObjectPtr<UObject>> Assets;

protected:
	virtual UObject* ResolveAsset(const FPrimaryAssetId& Id) const override
	{
		const TObjectPtr<UObject>* Found = Assets.Find(Id);
		return Found ? Found->Get() : nullptr;
	}
	virtual UAbilitySystemComponent* GetTargetAbilitySystem() const override { return nullptr; }
};

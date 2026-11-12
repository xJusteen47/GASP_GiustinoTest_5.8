// Equipment pickups, test types (M11 L2).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "GSTN_PickupsTestTypes.generated.h"

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

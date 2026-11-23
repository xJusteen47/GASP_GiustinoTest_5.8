#include "Tests/GSTN_LoadoutTestTypes.h"

UObject* UGSTN_TestLoadoutComponent::ResolveAsset(const FPrimaryAssetId& Id) const
{
	UObject* const* Found = Assets.Find(Id);
	return Found ? *Found : nullptr;
}

void UGSTN_TestLoadoutComponent::ListAssetIds(const FPrimaryAssetType& Type, TArray<FPrimaryAssetId>& OutIds) const
{
	OutIds.Reset();
	for (const TPair<FPrimaryAssetId, UObject*>& Pair : Assets)
	{
		if (Pair.Key.PrimaryAssetType == Type)
		{
			OutIds.Add(Pair.Key);
		}
	}
}

void UGSTN_TestLoadoutComponent::ListAssetsOfClass(const UClass* Class, TArray<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Reset();
	for (UObject* Object : ClassAssets)
	{
		if (Object && Class && Object->IsA(Class))
		{
			OutPaths.Add(FSoftObjectPath(Object));
		}
	}
}

UAbilitySystemComponent* UGSTN_TestLoadoutComponent::GetTargetAbilitySystem() const
{
	return Target;
}

bool UGSTN_TestLoadoutSubsystem::IsAssetIdKnown(const FPrimaryAssetId& Id) const
{
	return bEverythingKnown || Known.Contains(Id);
}

bool UGSTN_TestLoadoutSubsystem::ReadStorage(TArray<uint8>& OutBytes) const
{
	OutBytes = Storage;
	return !Storage.IsEmpty();
}

bool UGSTN_TestLoadoutSubsystem::WriteStorage(const TArray<uint8>& Bytes) const
{
	Storage = Bytes;
	return true;
}

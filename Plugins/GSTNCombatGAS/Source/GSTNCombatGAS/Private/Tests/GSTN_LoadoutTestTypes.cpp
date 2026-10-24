#include "Tests/GSTN_LoadoutTestTypes.h"

UObject* UGSTN_TestLoadoutComponent::ResolveAsset(const FPrimaryAssetId& Id) const
{
	UObject* const* Found = Assets.Find(Id);
	return Found ? *Found : nullptr;
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

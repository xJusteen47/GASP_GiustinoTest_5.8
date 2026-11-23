// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutSaveGame.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLoadoutSave, Log, All);

bool UGSTN_LoadoutSaveGame::ToBytes(TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	return UGameplayStatics::SaveGameToMemory(this, OutBytes);
}

UGSTN_LoadoutSaveGame* UGSTN_LoadoutSaveGame::FromBytes(const TArray<uint8>& Bytes)
{
	return Bytes.IsEmpty() ? nullptr : Cast<UGSTN_LoadoutSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
}

int32 UGSTN_LoadoutSaveGame::DropUnknownAssets(FGSTN_LoadoutData& Data, TFunctionRef<bool(const FPrimaryAssetId&)> IsKnown, const FString& Context)
{
	int32 Dropped = 0;

	auto IsUnknown = [&IsKnown](const FPrimaryAssetId& Id)
	{
		return Id.IsValid() && !IsKnown(Id);
	};
	auto Report = [&Context, &Dropped](const TCHAR* What, const FPrimaryAssetId& Id)
	{
		++Dropped;
		UE_LOG(LogGSTNLoadoutSave, Warning, TEXT("Loadout '%s': dropped the %s %s, no such asset exists any more."), *Context, What, *Id.ToString());
	};

	if (IsUnknown(Data.PrimaryKit))
	{
		Report(TEXT("kit"), Data.PrimaryKit);
		Data.PrimaryKit = FPrimaryAssetId();
	}
	if (IsUnknown(Data.Skin))
	{
		Report(TEXT("skin"), Data.Skin);
		Data.Skin = FPrimaryAssetId();
	}
	for (FPrimaryAssetId& SuperId : Data.Supers)
	{
		if (IsUnknown(SuperId))
		{
			Report(TEXT("super"), SuperId);
			SuperId = FPrimaryAssetId();
		}
	}

	// Weapon index in the loadout (0 = the kit's primary weapon, 1.. = AdditionalWeapons) to its index afterwards, INDEX_NONE if dropped
	TArray<int32> NewIndex;
	NewIndex.Add(0);
	TArray<FPrimaryAssetId> KeptWeapons;
	for (const FPrimaryAssetId& WeaponId : Data.AdditionalWeapons)
	{
		if (IsUnknown(WeaponId))
		{
			Report(TEXT("weapon"), WeaponId);
			NewIndex.Add(INDEX_NONE);
		}
		else
		{
			KeptWeapons.Add(WeaponId);
			NewIndex.Add(KeptWeapons.Num());
		}
	}
	Data.AdditionalWeapons = KeptWeapons;

	for (int32 Index = Data.SlotCustomisations.Num() - 1; Index >= 0; --Index)
	{
		FGSTN_SlotCustomisation& Customisation = Data.SlotCustomisations[Index];
		if (!NewIndex.IsValidIndex(Customisation.WeaponIndex))
		{
			// Names a weapon the loadout never had: the loadout rules report it, it is not a missing asset
			continue;
		}
		if (NewIndex[Customisation.WeaponIndex] == INDEX_NONE)
		{
			++Dropped;
			UE_LOG(LogGSTNLoadoutSave, Warning, TEXT("Loadout '%s': dropped a customisation of %s, its weapon was dropped."), *Context, *Customisation.Command.ToString());
			Data.SlotCustomisations.RemoveAt(Index);
		}
		else
		{
			Customisation.WeaponIndex = NewIndex[Customisation.WeaponIndex];
		}
	}
	return Dropped;
}

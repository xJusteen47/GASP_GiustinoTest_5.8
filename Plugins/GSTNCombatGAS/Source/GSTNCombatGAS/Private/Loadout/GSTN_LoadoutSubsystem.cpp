// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Loadout/GSTN_LoadoutSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLoadoutSubsystem, Log, All);

bool UGSTN_LoadoutSubsystem::IsAssetIdKnown(const FPrimaryAssetId& Id) const
{
	return UAssetManager::IsInitialized() && UAssetManager::Get().GetPrimaryAssetPath(Id).IsValid();
}

bool UGSTN_LoadoutSubsystem::ReadStorage(TArray<uint8>& OutBytes) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	const int32 UserIndex = LocalPlayer ? LocalPlayer->GetControllerId() : 0;
	return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(), UserIndex) && UGameplayStatics::LoadDataFromSlot(OutBytes, GetSaveSlotName(), UserIndex);
}

bool UGSTN_LoadoutSubsystem::WriteStorage(const TArray<uint8>& Bytes) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	const int32 UserIndex = LocalPlayer ? LocalPlayer->GetControllerId() : 0;
	return UGameplayStatics::SaveDataToSlot(Bytes, GetSaveSlotName(), UserIndex);
}

void UGSTN_LoadoutSubsystem::EnsureLoaded()
{
	if (Save)
	{
		return;
	}
	TArray<uint8> Bytes;
	if (ReadStorage(Bytes))
	{
		Save = UGSTN_LoadoutSaveGame::FromBytes(Bytes);
		if (!Save)
		{
			UE_LOG(LogGSTNLoadoutSubsystem, Warning, TEXT("The loadout save slot does not hold a loadout save game, starting empty."));
		}
	}
	if (!Save)
	{
		Save = NewObject<UGSTN_LoadoutSaveGame>(this);
	}
}

bool UGSTN_LoadoutSubsystem::Flush()
{
	TArray<uint8> Bytes;
	if (!Save || !Save->ToBytes(Bytes))
	{
		return false;
	}
	return WriteStorage(Bytes);
}

bool UGSTN_LoadoutSubsystem::SaveLoadout(const FString& PresetName, const FGSTN_LoadoutData& Data)
{
	if (PresetName.IsEmpty())
	{
		return false;
	}
	EnsureLoaded();
	Save->Loadouts.Add(PresetName, Data);
	return Flush();
}

bool UGSTN_LoadoutSubsystem::LoadLoadout(const FString& PresetName, FGSTN_LoadoutData& OutData)
{
	EnsureLoaded();
	const FGSTN_LoadoutData* Found = Save->Loadouts.Find(PresetName);
	if (!Found)
	{
		return false;
	}
	// A copy is cleaned, not the stored preset: the file keeps what the player saved
	FGSTN_LoadoutData Copy = *Found;
	UGSTN_LoadoutSaveGame::DropUnknownAssets(Copy, [this](const FPrimaryAssetId& Id) { return IsAssetIdKnown(Id); }, PresetName);
	OutData = MoveTemp(Copy);
	return true;
}

bool UGSTN_LoadoutSubsystem::DeleteLoadout(const FString& PresetName)
{
	EnsureLoaded();
	return Save->Loadouts.Remove(PresetName) > 0 && Flush();
}

bool UGSTN_LoadoutSubsystem::HasLoadout(const FString& PresetName)
{
	EnsureLoaded();
	return Save->Loadouts.Contains(PresetName);
}

TArray<FString> UGSTN_LoadoutSubsystem::ListLoadouts()
{
	EnsureLoaded();
	TArray<FString> Names;
	Save->Loadouts.GetKeys(Names);
	Names.Sort();
	return Names;
}

bool UGSTN_LoadoutSubsystem::RequestLoadout(const FString& PresetName, UGSTN_LoadoutComponent* Component)
{
	FGSTN_LoadoutData Data;
	if (!Component || !LoadLoadout(PresetName, Data))
	{
		return false;
	}
	Component->ServerRequestLoadout(Data);
	return true;
}

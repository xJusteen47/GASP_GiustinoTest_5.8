// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_PiggybackContentLibrary.h"

#include "Engine/AssetManager.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_MountTypes.h"
#include "GSTN_PiggybackTags.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Misc/PackageName.h"

bool UGSTN_PiggybackContentLibrary::WriteMountKitData(UGSTN_PrimaryKitDefinition* Kit, float RiderWeight, float CarryCapacity)
{
	if (!Kit)
	{
		return false;
	}
	FGSTN_MountKitExtension Data;
	Data.Rider.Weight = RiderWeight;
	Data.Carrier.CarryCapacity = CarryCapacity;

	TArray<FInstancedStruct>& Entries = Kit->Extensions.ExtensionData;
	for (FInstancedStruct& Entry : Entries)
	{
		if (const FGSTN_MountKitExtension* Existing = Entry.GetPtr<FGSTN_MountKitExtension>())
		{
			if (Existing->Rider.Weight == RiderWeight && Existing->Carrier.CarryCapacity == CarryCapacity)
			{
				return true;
			}
			Kit->Modify();
			Entry = FInstancedStruct::Make(Data);
			return true;
		}
	}
	Kit->Modify();
	Entries.Add(FInstancedStruct::Make(Data));
	return true;
}

bool UGSTN_PiggybackContentLibrary::GetMountKitData(const UGSTN_PrimaryKitDefinition* Kit, float& RiderWeight, float& CarryCapacity)
{
	const FGSTN_MountKitExtension* Data = Kit ? Kit->Extensions.Find<FGSTN_MountKitExtension>() : nullptr;
	RiderWeight = Data ? Data->Rider.Weight : 0.f;
	CarryCapacity = Data ? Data->Carrier.CarryCapacity : 0.f;
	return Data != nullptr;
}

bool UGSTN_PiggybackContentLibrary::ScanPluginKits()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager || !FPackageName::MountPointExists(FStringView(TEXT("/GSTNPiggyback/"))))
	{
		return false;
	}
	// The same base class and flags as the project's own scan of the type, or the asset manager reports a mismatch
	AssetManager->ScanPathsForPrimaryAssets(GSTNCombatAssetTypes::PrimaryKit(), { TEXT("/GSTNPiggyback") }, UGSTN_PrimaryKitDefinition::StaticClass(), false, false, true);
	return true;
}

int32 UGSTN_PiggybackContentLibrary::GetMountTypeIndex()
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	return Registry ? Registry->GetTypeIndex(GSTNPiggybackTags::Type_Mount) : 0;
}

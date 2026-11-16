// Bridge between backpacks and pickups: the backpack payload.

#include "GSTN_BackpackPickupPayload.h"
#include "GSTN_BackpackTypes.h"
#include "Modules/ModuleManager.h"

FPrimaryAssetId FGSTN_BackpackPickupPayload::GetDisplayId() const
{
	// The asset type name is the backpack definition's; this module does not link the ability layer to read it
	return IsEmpty() ? FPrimaryAssetId() : FPrimaryAssetId(FPrimaryAssetType(TEXT("GSTNBackpack")), FName(*Backpack.GetAssetName()));
}

bool FGSTN_BackpackPickupPayload::MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const
{
	FSoftObjectPath Previous;
	if (!FGSTN_BackpackLoadoutRules::Equip(Current, Backpack, OutNew, Previous, OutErrors))
	{
		return false;
	}
	OutLeftBehind.Reset();
	if (Previous.IsValid())
	{
		OutLeftBehind = FInstancedStruct::Make(FGSTN_BackpackPickupPayload(Previous));
	}
	return true;
}

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNBackpackPickups);

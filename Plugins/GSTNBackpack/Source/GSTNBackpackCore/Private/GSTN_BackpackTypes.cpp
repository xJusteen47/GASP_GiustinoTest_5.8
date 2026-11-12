// Backpacks, pure data.

#include "GSTN_BackpackTypes.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(GSTNBackpackTags::Backpack, "Backpack", "Root of the backpack identity tags (Backpack.<Name>)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(GSTNBackpackTags::Command_Backpack_Use, "Command.Backpack.Use", "The backpack's main verb; its button is data");

const FName FGSTN_LoadoutBackpackExt::WireId(TEXT("Backpack"));

FSoftObjectPath FGSTN_BackpackLoadoutRules::GetBackpack(const FGSTN_LoadoutData& Loadout)
{
	const FGSTN_LoadoutBackpackExt* Ext = GSTNLoadoutExtension::Find<FGSTN_LoadoutBackpackExt>(Loadout.Extensions);
	return Ext ? Ext->Backpack : FSoftObjectPath();
}

bool FGSTN_BackpackLoadoutRules::Equip(const FGSTN_LoadoutData& Current, const FSoftObjectPath& Backpack, FGSTN_LoadoutData& OutNew, FSoftObjectPath& OutPrevious,
	TArray<FString>& OutErrors)
{
	if (!Backpack.IsValid())
	{
		OutErrors.Add(TEXT("Backpack: no backpack to put on"));
		return false;
	}
	OutPrevious = GetBackpack(Current);
	if (OutPrevious == Backpack)
	{
		OutErrors.Add(FString::Printf(TEXT("Backpack: %s is carried already"), *Backpack.ToString()));
		return false;
	}
	OutNew = Current;
	// The carry-over choice of the backpack being replaced is kept: it is the player's (or the mode's), not the backpack's
	const FGSTN_LoadoutBackpackExt* Old = GSTNLoadoutExtension::Find<FGSTN_LoadoutBackpackExt>(Current.Extensions);
	FGSTN_LoadoutBackpackExt Ext = Old ? *Old : FGSTN_LoadoutBackpackExt();
	Ext.Backpack = Backpack;
	GSTNLoadoutExtension::Set(OutNew.Extensions, Ext);
	return true;
}

void FGSTN_BackpackLoadoutRules::Remove(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FSoftObjectPath& OutRemoved)
{
	OutNew = Current;
	OutRemoved = GetBackpack(Current);
	OutNew.Extensions.RemoveAll([](const FInstancedStruct& Entry) { return Entry.GetScriptStruct() == FGSTN_LoadoutBackpackExt::StaticStruct(); });
}

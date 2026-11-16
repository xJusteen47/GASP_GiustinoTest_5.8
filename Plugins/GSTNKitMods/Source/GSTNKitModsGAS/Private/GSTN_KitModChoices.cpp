// Kit mods, ability layer: the mod rows of the loadout screen.

#include "GSTN_KitModChoices.h"

#include "GSTN_KitModBaking.h"
#include "GSTN_KitModDefinition.h"
#include "GSTN_KitModTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutChoices.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"

#define LOCTEXT_NAMESPACE "GSTNKitModChoices"

const FName FGSTN_KitModChoices::ProviderId(TEXT("GSTNKitMods.Screen"));

namespace
{
	/** The draft with the kit's configuration replaced by BySlot (one mod path per kit slot, empty = none) */
	FGSTN_LoadoutData WithSlots(const FGSTN_LoadoutData& Draft, const FGameplayTag& KitKey, const TArray<FGameplayTag>& SlotTypes, const TArray<FSoftObjectPath>& BySlot)
	{
		FGSTN_LoadoutData Out = Draft;
		FGSTN_LoadoutModsExt Ext;
		if (const FGSTN_LoadoutModsExt* Existing = GSTNLoadoutExtension::Find<FGSTN_LoadoutModsExt>(Out.Extensions))
		{
			Ext = *Existing;
		}
		Ext.Configs.RemoveAll([&KitKey](const FGSTN_KitModConfig& Config) { return Config.Kit == KitKey; });

		FGSTN_KitModConfig Config;
		Config.Kit = KitKey;
		for (int32 Index = 0; Index < BySlot.Num(); ++Index)
		{
			if (BySlot[Index].IsValid())
			{
				FGSTN_EquippedMod& Equipped = Config.Mods.AddDefaulted_GetRef();
				Equipped.Slot = SlotTypes[Index];
				Equipped.Mod = BySlot[Index];
			}
		}
		if (!Config.Mods.IsEmpty() || KitKey.IsValid())
		{
			// A kit-specific entry is kept even when empty: it says "no mods on this kit" over a wildcard entry
			Ext.Configs.Add(MoveTemp(Config));
		}
		if (Ext.Configs.IsEmpty())
		{
			Out.Extensions.RemoveAll([](const FInstancedStruct& Entry) { return Entry.GetScriptStruct() == FGSTN_LoadoutModsExt::StaticStruct(); });
		}
		else
		{
			GSTNLoadoutExtension::Set(Out.Extensions, Ext);
		}
		return Out;
	}
}

void FGSTN_KitModChoices::Gather(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows)
{
	const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved.Kit;
	const FGSTN_KitModSlots* Slots = Kit ? Kit->Extensions.Find<FGSTN_KitModSlots>() : nullptr;
	if (!Slots || Slots->Slots.IsEmpty())
	{
		return;
	}
	FGameplayTagContainer MatchTags = Kit->KitTags;
	if (Context.Resolved.Weapons.Num() > 0 && Context.Resolved.Weapons[0])
	{
		MatchTags.AppendTags(Context.Resolved.Weapons[0]->WeaponTags);
	}
	const FGameplayTag KitKey = Kit->KitTags.IsEmpty() ? FGameplayTag() : Kit->KitTags.GetByIndex(0);

	// What each kit slot holds now: a mod takes the first free slot of its type, in list order (FGSTN_KitModLoadout::ResolveMods)
	TArray<FSoftObjectPath> BySlot;
	BySlot.SetNum(Slots->Slots.Num());
	const FGSTN_LoadoutModsExt* Ext = GSTNLoadoutExtension::Find<FGSTN_LoadoutModsExt>(Context.Draft.Extensions);
	if (const FGSTN_KitModConfig* Config = Ext ? FGSTN_KitModBaking::SelectConfig(*Ext, Kit->KitTags) : nullptr)
	{
		for (const FGSTN_EquippedMod& Equipped : Config->Mods)
		{
			const UGSTN_KitModDefinition* Mod = Cast<UGSTN_KitModDefinition>(Equipped.Mod.TryLoad());
			const FGameplayTag SlotType = Equipped.Slot.IsValid() ? Equipped.Slot : (Mod ? Mod->Slot : FGameplayTag());
			for (int32 Index = 0; Index < BySlot.Num(); ++Index)
			{
				if (!BySlot[Index].IsValid() && Slots->Slots[Index] == SlotType)
				{
					BySlot[Index] = Equipped.Mod;
					break;
				}
			}
		}
	}

	TArray<FSoftObjectPath> Paths;
	Context.ListAssetsOfClass(UGSTN_KitModDefinition::StaticClass(), Paths);
	TArray<TPair<FSoftObjectPath, const UGSTN_KitModDefinition*>> Mods;
	for (const FSoftObjectPath& Path : Paths)
	{
		const UGSTN_KitModDefinition* Mod = Cast<UGSTN_KitModDefinition>(Path.TryLoad());
		if (Mod && (Mod->CompatibleKits.IsEmpty() || Mod->CompatibleKits.Matches(MatchTags)))
		{
			Mods.Emplace(Path, Mod);
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < Slots->Slots.Num(); ++SlotIndex)
	{
		const FGameplayTag SlotType = Slots->Slots[SlotIndex];
		FString Leaf = SlotType.ToString();
		Leaf.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		FGSTN_LoadoutChoiceRow& Row = OutRows.AddDefaulted_GetRef();
		Row.Id = FName(*FString::Printf(TEXT("Mod.%d"), SlotIndex));
		Row.Label = FText::Format(LOCTEXT("ModRow", "Mod {0}"), FText::FromString(Leaf));

		auto AddOption = [&](const FSoftObjectPath& Choice, const FText& Label)
		{
			FGSTN_LoadoutChoiceOption& Option = Row.Options.AddDefaulted_GetRef();
			Option.Label = Label;
			if (Choice == BySlot[SlotIndex])
			{
				Option.Result = Context.Draft;
				Row.Selected = Row.Options.Num() - 1;
				return;
			}
			TArray<FSoftObjectPath> Changed = BySlot;
			Changed[SlotIndex] = Choice;
			Option.Result = WithSlots(Context.Draft, KitKey, Slots->Slots, Changed);
		};
		AddOption(FSoftObjectPath(), LOCTEXT("NoMod", "Nessuna"));
		for (const TPair<FSoftObjectPath, const UGSTN_KitModDefinition*>& Mod : Mods)
		{
			if (Mod.Value->Slot == SlotType)
			{
				AddOption(Mod.Key, FGSTN_LoadoutChoiceRegistry::PrettyName(Mod.Key));
			}
		}
		if (Row.Options.Num() < 2)
		{
			// Only "none": no mod of this slot type exists, the row would offer nothing
			OutRows.Pop();
		}
	}
}

#undef LOCTEXT_NAMESPACE

// Kit mods, content helpers for setup scripts and the test hub (M11 L10).

#include "GSTN_KitModContentLibrary.h"

#include "GSTN_KitModDefinition.h"
#include "GSTN_KitModTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"

bool UGSTN_KitModContentLibrary::SetKitModSlots(UGSTN_PrimaryKitDefinition* Kit, const TArray<FGameplayTag>& Slots, float MaxReachScale)
{
	if (!Kit)
	{
		return false;
	}
	FGSTN_KitModSlots Wanted;
	Wanted.Slots = Slots;
	Wanted.Limits.MaxReachScale = MaxReachScale;

	for (FInstancedStruct& Entry : Kit->Extensions.ExtensionData)
	{
		if (FGSTN_KitModSlots* Existing = Entry.GetMutablePtr<FGSTN_KitModSlots>())
		{
			if (FGSTN_KitModSlots::StaticStruct()->CompareScriptStruct(&Wanted, Existing, PPF_None))
			{
				return false;
			}
			*Existing = Wanted;
			Kit->MarkPackageDirty();
			return true;
		}
	}
	Kit->Extensions.ExtensionData.AddDefaulted_GetRef().InitializeAs<FGSTN_KitModSlots>(Wanted);
	Kit->MarkPackageDirty();
	return true;
}

bool UGSTN_KitModContentLibrary::WriteAllMovesMod(UGSTN_KitModDefinition* Mod, FGameplayTag ModTag, FGameplayTag Slot, float TimeScale,
	float HitboxScale, float ReachScale, float DamageScale, float KnockbackScale)
{
	if (!Mod)
	{
		return false;
	}
	FGSTN_MoveModifier Modifier;
	Modifier.TimeScale = TimeScale;
	Modifier.HitboxScale = FVector(HitboxScale);
	Modifier.ReachScale = ReachScale;
	Modifier.DamageScale = DamageScale;
	Modifier.KnockbackScale = KnockbackScale;
	const FGameplayTagContainer Tags = ModTag.IsValid() ? FGameplayTagContainer(ModTag) : FGameplayTagContainer();

	const bool bSame = Mod->ModTags == Tags && Mod->Slot == Slot && Mod->MoveModifiers.Num() == 1
		&& FGSTN_MoveModifier::StaticStruct()->CompareScriptStruct(&Modifier, &Mod->MoveModifiers[0], PPF_None);
	if (bSame)
	{
		return false;
	}
	Mod->ModTags = Tags;
	Mod->Slot = Slot;
	Mod->MoveModifiers = { Modifier };
	Mod->MarkPackageDirty();
	return true;
}

FGSTN_LoadoutData UGSTN_KitModContentLibrary::WithKitMods(const FGSTN_LoadoutData& Loadout, const TArray<FString>& ModPaths)
{
	FGSTN_LoadoutData Out = Loadout;
	FGSTN_KitModConfig AnyKit;
	for (const FString& Text : ModPaths)
	{
		const FSoftObjectPath Path(Text.TrimStartAndEnd());
		if (Path.IsNull())
		{
			continue;
		}
		FGSTN_EquippedMod& Equipped = AnyKit.Mods.AddDefaulted_GetRef();
		Equipped.Mod = Path;
		// The slot type comes from the mod asset at compile time when left empty (FGSTN_KitModLoadout::ResolveMods)
	}
	FGSTN_LoadoutModsExt Ext;
	if (const FGSTN_LoadoutModsExt* Existing = GSTNLoadoutExtension::Find<FGSTN_LoadoutModsExt>(Out.Extensions))
	{
		Ext = *Existing;
		Ext.Configs.RemoveAll([](const FGSTN_KitModConfig& Config) { return !Config.Kit.IsValid(); });
	}
	if (!AnyKit.Mods.IsEmpty())
	{
		Ext.Configs.Add(MoveTemp(AnyKit));
	}
	GSTNLoadoutExtension::Set(Out.Extensions, Ext);
	return Out;
}

TArray<FString> UGSTN_KitModContentLibrary::GetAnyKitMods(const FGSTN_LoadoutData& Loadout)
{
	TArray<FString> Paths;
	if (const FGSTN_LoadoutModsExt* Ext = GSTNLoadoutExtension::Find<FGSTN_LoadoutModsExt>(Loadout.Extensions))
	{
		for (const FGSTN_KitModConfig& Config : Ext->Configs)
		{
			if (!Config.Kit.IsValid())
			{
				for (const FGSTN_EquippedMod& Equipped : Config.Mods)
				{
					Paths.Add(Equipped.Mod.ToString());
				}
			}
		}
	}
	return Paths;
}

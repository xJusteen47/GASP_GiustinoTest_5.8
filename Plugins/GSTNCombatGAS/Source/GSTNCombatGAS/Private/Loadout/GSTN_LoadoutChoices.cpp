// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutChoices.h"

#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_SuperDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutComponent.h"

#define LOCTEXT_NAMESPACE "GSTNLoadoutChoices"

namespace
{
	struct FProviderEntry
	{
		FName Id;
		int32 Priority = 0;
		FGSTN_LoadoutChoiceProvider Provider;
	};

	TArray<FProviderEntry>& Providers()
	{
		static TArray<FProviderEntry> Entries;
		return Entries;
	}

	/** The kit row: every kit this machine knows. A new kit keeps the look and the extensions (mods per kit, backpack); its weapons, supers and specials start over */
	void GatherKitRow(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows)
	{
		TArray<FPrimaryAssetId> Kits;
		Context.ListAssetIds(GSTNCombatAssetTypes::PrimaryKit(), Kits);
		Kits.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName); });

		FGSTN_LoadoutChoiceRow& Row = OutRows.AddDefaulted_GetRef();
		Row.Id = TEXT("Kit");
		Row.Label = LOCTEXT("KitRow", "Kit");
		for (const FPrimaryAssetId& Id : Kits)
		{
			FGSTN_LoadoutChoiceOption& Option = Row.Options.AddDefaulted_GetRef();
			const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(Context.ResolveAsset(Id));
			Option.Label = Kit && !Kit->DisplayName.IsEmpty() ? Kit->DisplayName : FGSTN_LoadoutChoiceRegistry::PrettyNameFromString(Id.PrimaryAssetName.ToString());
			if (Id == Context.Draft.PrimaryKit)
			{
				Option.Result = Context.Draft;
				continue;
			}
			Option.Result.PrimaryKit = Id;
			Option.Result.Skin = Context.Draft.Skin;
			Option.Result.Extensions = Context.Draft.Extensions;
			Option.Result.DesignerValues = Context.Draft.DesignerValues;
		}
	}

	/** The label of a weapon or super: its DisplayName, else the readable asset name */
	FText AssetLabel(const FText& DisplayName, const FPrimaryAssetId& Id)
	{
		return DisplayName.IsEmpty() ? FGSTN_LoadoutChoiceRegistry::PrettyNameFromString(Id.PrimaryAssetName.ToString()) : DisplayName;
	}

	/**
	 * Support weapon rows (M11 leftover, priority 50): one per additional weapon slot of the kit (MaxAdditionalWeapons), each shown once the
	 * slot before it is filled. Options: none, then every weapon whose tags match the kit's AllowedAdditionalWeapons (not the primary one).
	 * A change drops the special picks of the support weapons (their indexes move); the primary weapon's stay
	 */
	void GatherSupportWeaponRows(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows)
	{
		const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved.Kit;
		if (!Kit || Kit->MaxAdditionalWeapons <= 0)
		{
			return;
		}
		TArray<FPrimaryAssetId> Ids;
		Context.ListAssetIds(GSTNCombatAssetTypes::Weapon(), Ids);
		Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName); });
		const FSoftObjectPath PrimaryPath = Kit->PrimaryWeapon.ToSoftObjectPath();
		TArray<TPair<FPrimaryAssetId, FText>> Allowed;
		for (const FPrimaryAssetId& Id : Ids)
		{
			const UGSTN_WeaponDefinition* Weapon = Cast<UGSTN_WeaponDefinition>(Context.ResolveAsset(Id));
			if (Weapon && FSoftObjectPath(Weapon) != PrimaryPath
				&& (Kit->AllowedAdditionalWeapons.IsEmpty() || Kit->AllowedAdditionalWeapons.Matches(Weapon->WeaponTags)))
			{
				Allowed.Emplace(Id, AssetLabel(Weapon->DisplayName, Id));
			}
		}
		if (Allowed.IsEmpty())
		{
			return;
		}
		const TArray<FPrimaryAssetId>& Current = Context.Draft.AdditionalWeapons;
		const int32 Slots = FMath::Min(Kit->MaxAdditionalWeapons, Current.Num() + 1);
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			FGSTN_LoadoutChoiceRow& Row = OutRows.AddDefaulted_GetRef();
			Row.Id = FName(*FString::Printf(TEXT("Support.%d"), Slot));
			Row.Label = Kit->MaxAdditionalWeapons == 1 ? LOCTEXT("SupportRow", "Arma di supporto")
				: FText::Format(LOCTEXT("SupportRowN", "Arma di supporto {0}"), FText::AsNumber(Slot + 1));
			const FPrimaryAssetId Held = Current.IsValidIndex(Slot) ? Current[Slot] : FPrimaryAssetId();
			auto With = [&Context, Slot](const FPrimaryAssetId& Id)
			{
				FGSTN_LoadoutData Result = Context.Draft;
				if (Id.IsValid())
				{
					if (Result.AdditionalWeapons.IsValidIndex(Slot))
					{
						Result.AdditionalWeapons[Slot] = Id;
					}
					else
					{
						Result.AdditionalWeapons.Add(Id);
					}
				}
				else if (Result.AdditionalWeapons.IsValidIndex(Slot))
				{
					Result.AdditionalWeapons.RemoveAt(Slot);
				}
				Result.SlotCustomisations.RemoveAll([](const FGSTN_SlotCustomisation& Entry) { return Entry.WeaponIndex > 0; });
				return Result;
			};
			FGSTN_LoadoutChoiceOption& None = Row.Options.AddDefaulted_GetRef();
			None.Label = LOCTEXT("NoSupport", "Nessuna");
			None.Result = Held.IsValid() ? With(FPrimaryAssetId()) : Context.Draft;
			for (const TPair<FPrimaryAssetId, FText>& Entry : Allowed)
			{
				// A weapon already in another slot is not offered twice
				if (Entry.Key != Held && Current.Contains(Entry.Key))
				{
					continue;
				}
				FGSTN_LoadoutChoiceOption& Option = Row.Options.AddDefaulted_GetRef();
				Option.Label = Entry.Value;
				Option.Result = Entry.Key == Held ? Context.Draft : With(Entry.Key);
			}
		}
	}

	/**
	 * Super rows (M11 leftover, priority 60): one per super slot of the kit (SuperSlotCount). Options: none, then every super whose SuperTags
	 * match the kit's AllowedSupers. A pick writes the slot (the list is sized to the kit's slots; an invalid id is an empty slot)
	 */
	void GatherSuperRows(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows)
	{
		const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved.Kit;
		if (!Kit || Kit->SuperSlotCount <= 0)
		{
			return;
		}
		TArray<FPrimaryAssetId> Ids;
		Context.ListAssetIds(GSTNCombatAssetTypes::Super(), Ids);
		Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName); });
		TArray<TPair<FPrimaryAssetId, FText>> Allowed;
		for (const FPrimaryAssetId& Id : Ids)
		{
			const UGSTN_SuperDefinition* Super = Cast<UGSTN_SuperDefinition>(Context.ResolveAsset(Id));
			if (Super && (Kit->AllowedSupers.IsEmpty() || Kit->AllowedSupers.Matches(Super->SuperTags)))
			{
				Allowed.Emplace(Id, AssetLabel(Super->DisplayName, Id));
			}
		}
		if (Allowed.IsEmpty())
		{
			return;
		}
		for (int32 Slot = 0; Slot < Kit->SuperSlotCount; ++Slot)
		{
			FGSTN_LoadoutChoiceRow& Row = OutRows.AddDefaulted_GetRef();
			Row.Id = FName(*FString::Printf(TEXT("Super.%d"), Slot));
			Row.Label = Kit->SuperSlotCount == 1 ? LOCTEXT("SuperRow", "Super") : FText::Format(LOCTEXT("SuperRowN", "Super {0}"), FText::AsNumber(Slot + 1));
			const FPrimaryAssetId Held = Context.Draft.Supers.IsValidIndex(Slot) ? Context.Draft.Supers[Slot] : FPrimaryAssetId();
			auto With = [&Context, Kit, Slot](const FPrimaryAssetId& Id)
			{
				FGSTN_LoadoutData Result = Context.Draft;
				Result.Supers.SetNum(FMath::Max(Result.Supers.Num(), Kit->SuperSlotCount));
				Result.Supers[Slot] = Id;
				return Result;
			};
			FGSTN_LoadoutChoiceOption& None = Row.Options.AddDefaulted_GetRef();
			None.Label = LOCTEXT("NoSuper", "Nessuna");
			None.Result = Held.IsValid() ? With(FPrimaryAssetId()) : Context.Draft;
			for (const TPair<FPrimaryAssetId, FText>& Entry : Allowed)
			{
				FGSTN_LoadoutChoiceOption& Option = Row.Options.AddDefaulted_GetRef();
				Option.Label = Entry.Value;
				Option.Result = Entry.Key == Held ? Context.Draft : With(Entry.Key);
			}
		}
	}

	/** One row per command of each weapon that allows alternatives (combat-loadout-mods 7): the weapon's own ability first, then the allowed ones */
	void GatherSpecialRows(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows)
	{
		for (int32 WeaponIndex = 0; WeaponIndex < Context.Resolved.Weapons.Num(); ++WeaponIndex)
		{
			const UGSTN_WeaponDefinition* Weapon = Context.Resolved.Weapons[WeaponIndex];
			if (!Weapon)
			{
				continue;
			}
			TArray<FGameplayTag> Commands;
			Weapon->AllowedAlternatives.GetKeys(Commands);
			Commands.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.GetTagName().LexicalLess(B.GetTagName()); });
			for (const FGameplayTag& Command : Commands)
			{
				const FGSTN_AbilityClassList& Allowed = Weapon->AllowedAlternatives.FindChecked(Command);
				if (Allowed.Abilities.IsEmpty())
				{
					continue;
				}
				FGSTN_LoadoutChoiceRow& Row = OutRows.AddDefaulted_GetRef();
				Row.Id = FName(*FString::Printf(TEXT("Special.%d.%s"), WeaponIndex, *Command.ToString()));
				FString Leaf = Command.ToString();
				Leaf.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				Row.Label = WeaponIndex == 0
					? FText::Format(LOCTEXT("SpecialRow", "Speciale {0}"), FText::FromString(Leaf))
					: FText::Format(LOCTEXT("SpecialRowWeapon", "Speciale {0} ({1})"), FText::FromString(Leaf), FGSTN_LoadoutChoiceRegistry::PrettyName(FSoftObjectPath(Weapon)));

				FGSTN_LoadoutChoiceOption& Own = Row.Options.AddDefaulted_GetRef();
				Own.Label = LOCTEXT("OwnSpecial", "Standard");
				Own.Result = UGSTN_LoadoutComponent::WithSpecial(Context.Draft, WeaponIndex, Command, TSoftClassPtr<UObject>());
				TSet<FSoftObjectPath> Seen;
				for (const TSoftClassPtr<UGameplayAbility>& Ability : Allowed.Abilities)
				{
					if (Ability.IsNull() || Seen.Contains(Ability.ToSoftObjectPath()))
					{
						continue;
					}
					Seen.Add(Ability.ToSoftObjectPath());
					FGSTN_LoadoutChoiceOption& Option = Row.Options.AddDefaulted_GetRef();
					Option.Label = FGSTN_LoadoutChoiceRegistry::PrettyName(Ability.ToSoftObjectPath());
					Option.Result = UGSTN_LoadoutComponent::WithSpecial(Context.Draft, WeaponIndex, Command, TSoftClassPtr<UObject>(Ability.ToSoftObjectPath()));
				}
			}
		}
	}
}

void FGSTN_LoadoutChoiceRegistry::Register(FName Id, int32 Priority, FGSTN_LoadoutChoiceProvider Provider)
{
	check(IsInGameThread());
	Unregister(Id);
	Providers().Add({ Id, Priority, MoveTemp(Provider) });
	Providers().StableSort([](const FProviderEntry& A, const FProviderEntry& B)
	{
		return A.Priority != B.Priority ? A.Priority < B.Priority : A.Id.LexicalLess(B.Id);
	});
}

bool FGSTN_LoadoutChoiceRegistry::Unregister(FName Id)
{
	check(IsInGameThread());
	return Providers().RemoveAll([Id](const FProviderEntry& Entry) { return Entry.Id == Id; }) > 0;
}

bool FGSTN_LoadoutChoiceRegistry::IsRegistered(FName Id)
{
	return Providers().ContainsByPredicate([Id](const FProviderEntry& Entry) { return Entry.Id == Id; });
}

void FGSTN_LoadoutChoiceRegistry::Gather(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows)
{
	check(IsInGameThread());
	OutRows.Reset();

	// Built-in rows at their documented priorities (kit 0, support weapons 50, supers 60, specials 100), registered ones around them
	bool bKitDone = false;
	bool bSupportDone = false;
	bool bSupersDone = false;
	bool bSpecialsDone = false;
	auto RunBuiltInsUpTo = [&](int32 Priority)
	{
		if (!bKitDone && Priority >= 0)
		{
			GatherKitRow(Context, OutRows);
			bKitDone = true;
		}
		if (!bSupportDone && Priority >= 50)
		{
			GatherSupportWeaponRows(Context, OutRows);
			bSupportDone = true;
		}
		if (!bSupersDone && Priority >= 60)
		{
			GatherSuperRows(Context, OutRows);
			bSupersDone = true;
		}
		if (!bSpecialsDone && Priority >= 100)
		{
			GatherSpecialRows(Context, OutRows);
			bSpecialsDone = true;
		}
	};
	for (const FProviderEntry& Entry : Providers())
	{
		// A provider at a built-in's priority runs after it
		RunBuiltInsUpTo(Entry.Priority);
		if (Entry.Provider)
		{
			Entry.Provider(Context, OutRows);
		}
	}
	RunBuiltInsUpTo(MAX_int32);

	OutRows.RemoveAll([](const FGSTN_LoadoutChoiceRow& Row) { return Row.Options.IsEmpty(); });
	for (FGSTN_LoadoutChoiceRow& Row : OutRows)
	{
		if (Row.Selected == INDEX_NONE)
		{
			Row.Selected = FindSelected(Row, Context.Draft);
		}
	}
}

FText FGSTN_LoadoutChoiceRegistry::PrettyName(const FSoftObjectPath& Path)
{
	const FString Name = Path.GetSubPathString().IsEmpty() ? Path.GetAssetName() : Path.GetSubPathString();
	return PrettyNameFromString(Name.IsEmpty() ? Path.ToString() : Name);
}

FText FGSTN_LoadoutChoiceRegistry::PrettyNameFromString(FString Name)
{
	Name.RemoveFromEnd(TEXT("_C"));
	// One type prefix (KIT_, MOD_, BP_, GA_...) and the "Test_" of the test content
	int32 Underscore = INDEX_NONE;
	if (Name.FindChar(TEXT('_'), Underscore) && Underscore > 0 && Underscore <= 4 && Name.Left(Underscore).ToUpper() == Name.Left(Underscore))
	{
		Name.RightChopInline(Underscore + 1);
	}
	Name.RemoveFromStart(TEXT("Test_"));
	return FText::FromString(Name.Replace(TEXT("_"), TEXT(" ")));
}

int32 FGSTN_LoadoutChoiceRegistry::FindSelected(const FGSTN_LoadoutChoiceRow& Row, const FGSTN_LoadoutData& Draft)
{
	return Row.Options.IndexOfByPredicate([&Draft](const FGSTN_LoadoutChoiceOption& Option) { return Option.Result == Draft; });
}

#undef LOCTEXT_NAMESPACE

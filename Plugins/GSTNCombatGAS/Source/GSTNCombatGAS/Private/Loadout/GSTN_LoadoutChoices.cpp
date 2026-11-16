// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutChoices.h"

#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
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

	// Built-in rows at their documented priorities (kit 0, specials 100), registered ones around them
	bool bKitDone = false;
	bool bSpecialsDone = false;
	auto RunBuiltInsUpTo = [&](int32 Priority)
	{
		if (!bKitDone && Priority >= 0)
		{
			GatherKitRow(Context, OutRows);
			bKitDone = true;
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

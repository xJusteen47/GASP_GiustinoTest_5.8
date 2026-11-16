// Backpacks, ability layer: the backpack row of the loadout screen.

#include "GSTN_BackpackChoices.h"

#include "GSTN_BackpackDefinition.h"
#include "GSTN_BackpackTypes.h"
#include "Loadout/GSTN_LoadoutChoices.h"

#define LOCTEXT_NAMESPACE "GSTNBackpackChoices"

const FName FGSTN_BackpackChoices::ProviderId(TEXT("GSTNBackpack.Screen"));

void FGSTN_BackpackChoices::Gather(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows)
{
	if (!Context.Draft.PrimaryKit.IsValid())
	{
		return;
	}
	TArray<FSoftObjectPath> Backpacks;
	Context.ListAssetsOfClass(UGSTN_BackpackDefinition::StaticClass(), Backpacks);
	if (Backpacks.IsEmpty())
	{
		return;
	}
	const FSoftObjectPath Current = FGSTN_BackpackLoadoutRules::GetBackpack(Context.Draft);

	FGSTN_LoadoutChoiceRow& Row = OutRows.AddDefaulted_GetRef();
	Row.Id = TEXT("Backpack");
	Row.Label = LOCTEXT("BackpackRow", "Zaino");

	FGSTN_LoadoutChoiceOption& None = Row.Options.AddDefaulted_GetRef();
	None.Label = LOCTEXT("NoBackpack", "Nessuno");
	FSoftObjectPath Removed;
	FGSTN_BackpackLoadoutRules::Remove(Context.Draft, None.Result, Removed);
	if (!Current.IsValid())
	{
		None.Result = Context.Draft;
		Row.Selected = 0;
	}

	for (const FSoftObjectPath& Backpack : Backpacks)
	{
		FGSTN_LoadoutChoiceOption& Option = Row.Options.AddDefaulted_GetRef();
		Option.Label = FGSTN_LoadoutChoiceRegistry::PrettyName(Backpack);
		if (Backpack == Current)
		{
			Option.Result = Context.Draft;
			Row.Selected = Row.Options.Num() - 1;
			continue;
		}
		FSoftObjectPath Previous;
		TArray<FString> Errors;
		if (!FGSTN_BackpackLoadoutRules::Equip(Context.Draft, Backpack, Option.Result, Previous, Errors))
		{
			Option.Result = Context.Draft;
			Option.bValid = false;
			Option.Problem = Errors.IsEmpty() ? FString() : Errors[0];
		}
	}
}

#undef LOCTEXT_NAMESPACE

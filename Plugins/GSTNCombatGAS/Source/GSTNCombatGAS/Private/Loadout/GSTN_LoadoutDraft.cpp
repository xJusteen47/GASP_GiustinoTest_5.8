// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutDraft.h"

#include "Loadout/GSTN_LoadoutComponent.h"

void FGSTN_LoadoutDraft::Begin(const UGSTN_LoadoutComponent& InComponent, const FGSTN_LoadoutData& Start)
{
	Component = &InComponent;
	Data = Start;
	Cursor = 0;
	Rebuild();
	if (bValid)
	{
		return;
	}
	// No usable start (a first match, a saved kit that is gone): the first kit the rules accept
	for (const FGSTN_LoadoutChoiceRow& Row : Rows)
	{
		if (Row.Id != TEXT("Kit"))
		{
			continue;
		}
		for (const FGSTN_LoadoutChoiceOption& Option : Row.Options)
		{
			if (Option.bValid)
			{
				Data = Option.Result;
				Rebuild();
				return;
			}
		}
	}
}

bool FGSTN_LoadoutDraft::MoveCursor(int32 Step)
{
	if (Rows.IsEmpty())
	{
		return false;
	}
	const int32 Count = Rows.Num();
	Cursor = ((Cursor + Step) % Count + Count) % Count;
	return true;
}

bool FGSTN_LoadoutDraft::Cycle(int32 Step)
{
	if (!Rows.IsValidIndex(Cursor) || Step == 0)
	{
		return false;
	}
	const FGSTN_LoadoutChoiceRow& Row = Rows[Cursor];
	const int32 Count = Row.Options.Num();
	const int32 Direction = Step > 0 ? 1 : -1;
	int32 Index = Row.Selected == INDEX_NONE ? (Direction > 0 ? -1 : 0) : Row.Selected;
	for (int32 Tries = 0; Tries < Count; ++Tries)
	{
		Index = ((Index + Direction) % Count + Count) % Count;
		if (Index == Row.Selected)
		{
			return false;
		}
		if (Row.Options[Index].bValid)
		{
			Data = Row.Options[Index].Result;
			Rebuild();
			return true;
		}
	}
	return false;
}

void FGSTN_LoadoutDraft::Rebuild()
{
	const FName CursorRow = Rows.IsValidIndex(Cursor) ? Rows[Cursor].Id : NAME_None;
	Rows.Reset();
	bValid = false;
	Problem.Reset();
	const UGSTN_LoadoutComponent* Owner = Component.Get();
	if (!Owner)
	{
		Problem = TEXT("No loadout component.");
		return;
	}
	Owner->GatherChoices(Data, Rows);

	TArray<FString> Errors;
	bValid = Owner->ValidateLoadout(Data, Errors);
	Problem = bValid || Errors.IsEmpty() ? FString() : Errors[0];

	const int32 Kept = Rows.IndexOfByPredicate([CursorRow](const FGSTN_LoadoutChoiceRow& Row) { return Row.Id == CursorRow; });
	Cursor = Kept != INDEX_NONE ? Kept : FMath::Clamp(Cursor, 0, FMath::Max(Rows.Num() - 1, 0));
}

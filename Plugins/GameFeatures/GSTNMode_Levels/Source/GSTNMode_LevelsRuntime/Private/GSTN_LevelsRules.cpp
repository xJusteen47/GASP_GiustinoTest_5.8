// Levels mode: the mode's piece of the game rules.

#include "GSTN_LevelsRules.h"

UGSTN_LevelsRules::UGSTN_LevelsRules()
{
	// TEST VALUES: a first guess to tune by playing. One player is the identity; the rest grow with the team
	const struct { int32 Count; float Enemies; float Health; } Rows[] = { { 1, 1.f, 1.f }, { 2, 1.4f, 1.3f }, { 3, 1.8f, 1.6f }, { 4, 2.2f, 1.9f } };
	for (const auto& Row : Rows)
	{
		FGSTN_PlayerCountScaling& Entry = Scaling.AddDefaulted_GetRef();
		Entry.PlayerCount = Row.Count;
		Entry.EnemyCountScale = Row.Enemies;
		Entry.EnemyHealthScale = Row.Health;
	}
}

FGSTN_PlayerCountScaling UGSTN_LevelsRules::GetScaling(int32 PlayerCount) const
{
	const FGSTN_PlayerCountScaling* Best = nullptr;
	const FGSTN_PlayerCountScaling* Lowest = nullptr;
	for (const FGSTN_PlayerCountScaling& Entry : Scaling)
	{
		if (Entry.PlayerCount <= PlayerCount && (Best == nullptr || Entry.PlayerCount > Best->PlayerCount))
		{
			Best = &Entry;
		}
		if (Lowest == nullptr || Entry.PlayerCount < Lowest->PlayerCount)
		{
			Lowest = &Entry;
		}
	}
	// Fewer players than the table's first row (a table that starts at 2): its first row; an empty table: the identity
	if (Best)
	{
		return *Best;
	}
	return Lowest ? *Lowest : FGSTN_PlayerCountScaling();
}

// Levels mode: the ordered list of maps.

#include "GSTN_LevelSetDefinition.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

namespace
{
	/** "/Game/Maps/UEDPIE_0_L_X.L_X" or "L_X" -> "L_X" */
	FString ShortMapName(const FString& MapName)
	{
		FString Name = UWorld::RemovePIEPrefix(MapName);
		const int32 DotIndex = Name.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (DotIndex != INDEX_NONE)
		{
			Name.LeftInline(DotIndex);
		}
		return FPaths::GetCleanFilename(Name);
	}
}

FName FGSTN_LevelEntry::GetId() const
{
	return LevelId.IsNone() ? FName(*ShortMapName(Map.GetLongPackageName())) : LevelId;
}

int32 UGSTN_LevelSetDefinition::FindIndexByMap(const FString& MapName) const
{
	if (MapName.IsEmpty())
	{
		return INDEX_NONE;
	}
	const FString Wanted = ShortMapName(MapName);
	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		if (Levels[Index].IsValidEntry() && ShortMapName(Levels[Index].Map.GetLongPackageName()).Equals(Wanted, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UGSTN_LevelSetDefinition::GetNextIndex(int32 Current) const
{
	const int32 Num = Levels.Num();
	if (Num == 0)
	{
		return INDEX_NONE;
	}
	// Walk at most one lap so a set of only invalid entries ends instead of looping forever
	for (int32 Step = 1; Step <= Num; ++Step)
	{
		int32 Candidate = Current + Step;
		if (Candidate >= Num)
		{
			if (!bLoop)
			{
				return INDEX_NONE;
			}
			Candidate %= Num;
		}
		if (Levels[Candidate].IsValidEntry())
		{
			return Candidate;
		}
	}
	return INDEX_NONE;
}

FString UGSTN_LevelSetDefinition::BuildTravelURL(int32 Index, bool bListen) const
{
	const FGSTN_LevelEntry* Entry = GetEntry(Index);
	if (!Entry || !Entry->IsValidEntry())
	{
		return FString();
	}
	FString URL = Entry->Map.GetLongPackageName();
	if (bListen)
	{
		URL += TEXT("?listen");
	}
	if (!Entry->ExtraOptions.IsEmpty())
	{
		URL += Entry->ExtraOptions.StartsWith(TEXT("?")) ? Entry->ExtraOptions : (TEXT("?") + Entry->ExtraOptions);
	}
	return URL;
}

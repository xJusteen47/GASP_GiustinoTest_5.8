// Interaction focus: pure data types (interaction.md §9). No GAS, no Network Prediction, no Mover.

#include "GSTN_InteractTypes.h"

#include "Misc/Crc.h"

FIntVector FGSTN_InteractCandidate::QuantiseCm(const FVector& Location)
{
	return FIntVector(FMath::RoundToInt(Location.X), FMath::RoundToInt(Location.Y), FMath::RoundToInt(Location.Z));
}

bool FGSTN_InteractRules::Sanitise()
{
	const FGSTN_InteractRules Before = *this;
	HardMaxReachCm = FMath::Max(HardMaxReachCm, 1.f);
	ReachCm = FMath::Clamp(ReachCm, 1.f, HardMaxReachCm);
	NearRadiusCm = FMath::Clamp(NearRadiusCm, 0.f, ReachCm);
	ArcFarDeg = FMath::Clamp(ArcFarDeg, 1.f, 180.f);
	ArcNearDeg = FMath::Clamp(ArcNearDeg, ArcFarDeg, 180.f);
	AimArcDeg = FMath::Clamp(AimArcDeg, 1.f, 180.f);
	StickyReachScale = FMath::Max(StickyReachScale, 1.f);
	StickyArcScale = FMath::Max(StickyArcScale, 1.f);
	TieEpsilon = FMath::Max(TieEpsilon, 1.f / 4096.f);
	MaxCandidatesPerProbe = FMath::Max(MaxCandidatesPerProbe, 1);
	MaxLosTraces = FMath::Max(MaxLosTraces, 0);
	MaxFilterCalls = FMath::Max(MaxFilterCalls, 0);
	MinDwellFrames = FMath::Max(MinDwellFrames, 0);
	ProbeEveryNFrames = FMath::Max(ProbeEveryNFrames, 1);
	MaxRecords = FMath::Max(MaxRecords, 1);
	return Before.ReachCm != ReachCm ||Before.HardMaxReachCm != HardMaxReachCm || Before.NearRadiusCm != NearRadiusCm
		|| Before.ArcFarDeg != ArcFarDeg || Before.ArcNearDeg != ArcNearDeg || Before.AimArcDeg != AimArcDeg || Before.TieEpsilon != TieEpsilon
		|| Before.StickyReachScale != StickyReachScale || Before.StickyArcScale != StickyArcScale;
}

bool FGSTN_InteractRules::IsValid(FString* OutError) const
{
	auto Fail = [OutError](const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	};
	if (ReachCm > HardMaxReachCm)
	{
		return Fail(TEXT("ReachCm exceeds HardMaxReachCm"));
	}
	if (NearRadiusCm > ReachCm)
	{
		return Fail(TEXT("NearRadiusCm exceeds ReachCm"));
	}
	if (ArcNearDeg < ArcFarDeg)
	{
		return Fail(TEXT("ArcNearDeg is narrower than ArcFarDeg (the arc widens towards the feet)"));
	}
	if (StickyReachScale < 1.f || StickyArcScale < 1.f)
	{
		return Fail(TEXT("a sticky scale below 1 would make the incumbent harder to keep than to acquire"));
	}
	return true;
}

void FGSTN_InteractMeaningTable::Build(TConstArrayView<FGSTN_InteractMeaning> InEntries, TArray<FString>* OutErrors)
{
	Entries.Reset();
	for (const FGSTN_InteractMeaning& Entry : InEntries)
	{
		if (!Entry.Tag.IsValid())
		{
			if (OutErrors)
			{
				OutErrors->Add(TEXT("a meaning has no tag"));
			}
			continue;
		}
		const bool bDuplicate = Entries.ContainsByPredicate([&Entry](const FGSTN_InteractMeaning& Other) { return Other.Tag == Entry.Tag; });
		if (bDuplicate)
		{
			if (OutErrors)
			{
				OutErrors->Add(FString::Printf(TEXT("meaning %s is listed twice"), *Entry.Tag.ToString()));
			}
			continue;
		}
		if (Entries.Num() >= 255)
		{
			if (OutErrors)
			{
				OutErrors->Add(TEXT("more than 255 meanings"));
			}
			break;
		}
		Entries.Add(Entry);
	}

	// Sorted by tag name: every machine derives the same index whatever order the settings list them in
	Entries.Sort([](const FGSTN_InteractMeaning& A, const FGSTN_InteractMeaning& B) { return A.Tag.ToString() < B.Tag.ToString(); });

	uint32 Hash = 0;
	for (const FGSTN_InteractMeaning& Entry : Entries)
	{
		Hash = FCrc::StrCrc32(*Entry.Tag.ToString(), Hash);
		Hash = FCrc::MemCrc32(&Entry.DefaultPriority, sizeof(int32), Hash);
		Hash = FCrc::MemCrc32(&Entry.HoldFrames, sizeof(int32), Hash);
	}
	SignatureValue = Hash;
}

int32 FGSTN_InteractMeaningTable::IndexOf(const FGameplayTag& Tag) const
{
	const int32 Found = Entries.IndexOfByPredicate([&Tag](const FGSTN_InteractMeaning& Entry) { return Entry.Tag == Tag; });
	return Found == INDEX_NONE ? 0 : Found + 1;
}

const FGSTN_InteractMeaning* FGSTN_InteractMeaningTable::Get(int32 Index) const
{
	return (Index >= 1 && Index <= Entries.Num()) ? &Entries[Index - 1] : nullptr;
}

FGameplayTag FGSTN_InteractMeaningTable::TagOf(int32 Index) const
{
	const FGSTN_InteractMeaning* Entry = Get(Index);
	return Entry ? Entry->Tag : FGameplayTag();
}

int32 FGSTN_InteractMeaningTable::DefaultPriorityOf(int32 Index) const
{
	const FGSTN_InteractMeaning* Entry = Get(Index);
	return Entry ? FMath::Clamp(Entry->DefaultPriority, 0, 255) : 0;
}

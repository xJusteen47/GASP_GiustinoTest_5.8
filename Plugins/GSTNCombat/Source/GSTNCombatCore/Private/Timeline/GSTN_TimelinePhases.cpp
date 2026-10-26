// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_TimelinePhases.h"

#include "Hits/GSTN_CombatHitTags.h"

#define LOCTEXT_NAMESPACE "GSTNCombatCore"

bool FGSTN_TimelinePhases::CountsAsActive(const FGSTN_TimelineTrack_Hitbox& Track)
{
	if (Track.Hitboxes.IsEmpty())
	{
		return true;
	}

	// The four relationships a victim can have. A hitbox that ignores all of them can never hit anyone
	const FGameplayTag Relationships[] = {
		GSTNCombatHitTags::Relationship_Self.GetTag(), GSTNCombatHitTags::Relationship_Ally.GetTag(),
		GSTNCombatHitTags::Relationship_Enemy.GetTag(), GSTNCombatHitTags::Relationship_Neutral.GetTag() };

	for (const FGSTN_HitboxSpec& Spec : Track.Hitboxes)
	{
		bool bIgnoresAll = true;
		for (const FGameplayTag& Relationship : Relationships)
		{
			const FGSTN_HitVariant* Variant = Spec.RelationshipVariants.Find(Relationship);
			if (Variant == nullptr || !Variant->bIgnore)
			{
				bIgnoresAll = false;
				break;
			}
		}
		if (!bIgnoresAll)
		{
			return true;
		}
	}
	return false;
}

FGSTN_TimelinePhases FGSTN_TimelinePhases::Compute(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_TimelinePhases Result;
	Result.Total = Data.TotalFrames;
	Result.FirstActionableFrame = Data.FirstActionableFrame;
	Result.RecoveryEnd = Data.FirstActionableFrame;

	// Count first so the output array is allocated once, and not at all for a move with no hitbox
	int32 WindowCount = 0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Instance.GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (Track != nullptr && Track->EndFrame > Track->StartFrame && CountsAsActive(*Track))
		{
			++WindowCount;
		}
	}

	if (WindowCount > 0)
	{
		TArray<FIntPoint> Windows;
		Windows.Reserve(WindowCount);
		for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
		{
			const FGSTN_TimelineTrack_Hitbox* Track = Instance.GetPtr<FGSTN_TimelineTrack_Hitbox>();
			if (Track != nullptr && Track->EndFrame > Track->StartFrame && CountsAsActive(*Track))
			{
				Windows.Add(FIntPoint(FMath::Max(Track->StartFrame, 0), Track->EndFrame));
			}
		}
		Windows.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X < B.X; });

		// Sorted by start: a window that starts inside or right after the previous range extends it
		Result.ActiveRanges.Reserve(WindowCount);
		for (const FIntPoint& Window : Windows)
		{
			if (!Result.ActiveRanges.IsEmpty() && Window.X <= Result.ActiveRanges.Last().Y)
			{
				Result.ActiveRanges.Last().Y = FMath::Max(Result.ActiveRanges.Last().Y, Window.Y);
			}
			else
			{
				Result.ActiveRanges.Add(Window);
			}
		}

		Result.bHasActive = true;
		Result.ActiveStart = Result.ActiveRanges[0].X;
		Result.ActiveEnd = Result.ActiveRanges.Last().Y;
		Result.StartupEnd = Result.ActiveStart;
		Result.FirstHitFrame1Based = Result.ActiveStart + 1;
	}

	Result.TailStart = Data.GetTailStartFrame();
	Result.TailBlendOutFrames = Data.Tail.ResolveBlendOutFrames(Data.TotalFrames, Data.FirstActionableFrame);
	return Result;
}

FText FGSTN_TimelinePhases::FormatSummary(float TickRate) const
{
	const int32 Milliseconds = TickRate > 0.f ? FMath::RoundToInt(static_cast<double>(Total) * 1000.0 / TickRate) : 0;
	if (!bHasActive)
	{
		return FText::Format(LOCTEXT("SummaryNoHitbox", "No hitbox | FAF {0} | Total {1} ({2} ms)"),
			FText::AsNumber(FirstActionableFrame), FText::AsNumber(Total), FText::AsNumber(Milliseconds));
	}
	return FText::Format(LOCTEXT("Summary", "Startup {0} | Active {1} | Recovery {2} | FAF {3} | Total {4} ({5} ms)"),
		FText::AsNumber(GetStartupFrames()), FText::AsNumber(GetActiveFrames()), FText::AsNumber(GetRecoveryFrames()),
		FText::AsNumber(FirstActionableFrame), FText::AsNumber(Total), FText::AsNumber(Milliseconds));
}

#undef LOCTEXT_NAMESPACE

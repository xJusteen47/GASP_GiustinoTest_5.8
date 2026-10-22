#include "Timeline/GSTN_TimelineValidation.h"
#include "Animation/AnimMontage.h"

namespace
{
	void Add(TArray<FGSTN_TimelineMessage>& Out, bool bError, FString&& Text)
	{
		FGSTN_TimelineMessage& Message = Out.AddDefaulted_GetRef();
		Message.bError = bError;
		Message.Text = MoveTemp(Text);
	}

	void CheckSelector(const TCHAR* Kind, int32 Index, const FGSTN_PartSelector& Part, const FGSTN_TimelineValidationContext& Context, TArray<FGSTN_TimelineMessage>& Out)
	{
		if (Part.IsEmpty())
		{
			Add(Out, true, FString::Printf(TEXT("Track %d (%s) addresses no part, group or tag. A whole-body target is spelled with the group that holds every part."), Index, Kind));
			return;
		}
		if (Context.KnownPartNames && !Part.PartName.IsNone() && !Context.KnownPartNames->Contains(Part.PartName))
		{
			Add(Out, true, FString::Printf(TEXT("Track %d (%s): unknown part '%s'."), Index, Kind, *Part.PartName.ToString()));
		}
		if (Context.KnownGroupNames && !Part.GroupName.IsNone() && !Context.KnownGroupNames->Contains(Part.GroupName))
		{
			Add(Out, true, FString::Printf(TEXT("Track %d (%s): unknown group '%s'."), Index, Kind, *Part.GroupName.ToString()));
		}
	}
}

FGSTN_TimelineValidationContext FGSTN_TimelineValidation::MakeContext(const FGSTN_CombatTimelineData& Data, float TickRate, int32 DefaultOverrideFrames)
{
	FGSTN_TimelineValidationContext Context;
	Context.TickRate = TickRate;
	Context.DefaultOverrideFrames = DefaultOverrideFrames;

	// Never loads: a montage that is not in memory is simply not checked
	if (const UAnimMontage* Montage = Data.Montage.Get())
	{
		Context.MontageLengthSeconds = Montage->GetPlayLength();
		if (!Data.MontageSection.IsNone())
		{
			const int32 SectionIndex = Montage->GetSectionIndex(Data.MontageSection);
			if (SectionIndex != INDEX_NONE)
			{
				Context.MontageSectionSeconds = Montage->GetSectionLength(SectionIndex);
			}
		}
	}
	return Context;
}

void FGSTN_TimelineValidation::Validate(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineValidationContext& Context, TArray<FGSTN_TimelineMessage>& OutMessages)
{
	const int32 Total = Data.TotalFrames;

	if (Data.FirstActionableFrame > Total)
	{
		Add(OutMessages, true, FString::Printf(TEXT("FirstActionableFrame (%d) is after TotalFrames (%d)."), Data.FirstActionableFrame, Total));
	}

	if (Data.Tail.bHasTail)
	{
		if (Data.Tail.StartFrame > Total)
		{
			Add(OutMessages, false, FString::Printf(TEXT("Tail start (%d) is after TotalFrames (%d): it is clamped to the end and the tail is empty."), Data.Tail.StartFrame, Total));
		}
		const int32 TailStart = Data.GetTailStartFrame();
		if (TailStart < Data.FirstActionableFrame)
		{
			Add(OutMessages, false, FString::Printf(TEXT("The tail starts at frame %d, before FirstActionableFrame (%d): anything could cancel the locked part."), TailStart, Data.FirstActionableFrame));
		}
		if (Data.Tail.BlendOutFrames > Total - TailStart)
		{
			Add(OutMessages, false, FString::Printf(TEXT("Tail blend-out (%d frames) is longer than the tail (%d): it is clamped."), Data.Tail.BlendOutFrames, FMath::Max(Total - TailStart, 0)));
		}
	}

	// Deferred commit: the cost, events, movement and hitboxes start where the override window ends
	const int32 Commit = Data.ResolveOverrideFrames(Context.DefaultOverrideFrames);
	if (Commit > Total)
	{
		Add(OutMessages, true, FString::Printf(TEXT("The override window (%d frames) is longer than the timeline (%d)."), Commit, Total));
	}
	else if (Commit > Data.FirstActionableFrame)
	{
		Add(OutMessages, false, FString::Printf(TEXT("The override window (%d frames) ends after FirstActionableFrame (%d)."), Commit, Data.FirstActionableFrame));
	}

	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack* Track = Data.Tracks[Index].GetPtr<FGSTN_TimelineTrack>();
		if (!Track)
		{
			Add(OutMessages, true, FString::Printf(TEXT("Track %d is empty."), Index));
			continue;
		}

		const EGSTN_TimelineTrackKind Kind = Track->GetKind();
		const bool bPoint = Kind == EGSTN_TimelineTrackKind::Event || Kind == EGSTN_TimelineTrackKind::WeaponSwitch;
		const int32 KeyFrame = Kind == EGSTN_TimelineTrackKind::WeaponSwitch ? static_cast<const FGSTN_TimelineTrack_WeaponSwitch*>(Track)->SwapFrame : Track->StartFrame;

		if (bPoint)
		{
			if (Total > 0 && KeyFrame >= Total)
			{
				Add(OutMessages, true, FString::Printf(TEXT("Track %d fires at frame %d, outside the timeline (%d frames)."), Index, KeyFrame, Total));
			}
		}
		else
		{
			if (Track->EndFrame <= Track->StartFrame)
			{
				Add(OutMessages, true, FString::Printf(TEXT("Track %d has an empty range [%d, %d)."), Index, Track->StartFrame, Track->EndFrame));
			}
			if (Track->EndFrame > Total)
			{
				Add(OutMessages, false, FString::Printf(TEXT("Track %d ends at frame %d, after TotalFrames (%d)."), Index, Track->EndFrame, Total));
			}
		}

		switch (Kind)
		{
		case EGSTN_TimelineTrackKind::Event:
		case EGSTN_TimelineTrackKind::Movement:
		case EGSTN_TimelineTrackKind::WeaponSwitch:
		case EGSTN_TimelineTrackKind::Hitbox:
			if (KeyFrame < Commit)
			{
				Add(OutMessages, true, FString::Printf(TEXT("Track %d starts at frame %d, before the deferred commit (frame %d): a timeline that is replaced in its override window must not have acted yet."), Index, KeyFrame, Commit));
			}
			break;
		default:
			break;
		}

		if (Kind == EGSTN_TimelineTrackKind::Hitbox)
		{
			CheckSelector(TEXT("Hitbox"), Index, static_cast<const FGSTN_TimelineTrack_Hitbox*>(Track)->Part, Context, OutMessages);
		}
		else if (Kind == EGSTN_TimelineTrackKind::HurtboxModifier)
		{
			CheckSelector(TEXT("HurtboxModifier"), Index, static_cast<const FGSTN_TimelineTrack_HurtboxModifier*>(Track)->Part, Context, OutMessages);
		}
	}

	// Montage length against the frames. Only when the length is known: a soft pointer is never loaded here
	if (Context.MontageLengthSeconds >= 0.f && Context.TickRate > 0.f && Data.MontagePlayRate > 0.f)
	{
		const float Seconds = (!Data.MontageSection.IsNone() && Context.MontageSectionSeconds >= 0.f) ? Context.MontageSectionSeconds : Context.MontageLengthSeconds;
		const float MontageFrames = Seconds / Data.MontagePlayRate * Context.TickRate;
		if (FMath::Abs(MontageFrames - static_cast<float>(Total)) > 1.f)
		{
			Add(OutMessages, false, FString::Printf(TEXT("The montage lasts %.1f frames at the play rate, TotalFrames is %d."), MontageFrames, Total));
		}
	}

	if (Data.bHasRicochetResponse)
	{
		const FGSTN_RicochetResponse& Response = Data.RicochetResponse;
		if (!Response.IsAuthored())
		{
			Add(OutMessages, false, TEXT("A ricochet response is enabled but has no recoil frames: it is ignored."));
		}
		else if (Response.bHasMovement)
		{
			if (Response.Movement.EndFrame <= Response.Movement.StartFrame)
			{
				Add(OutMessages, true, FString::Printf(TEXT("The recoil movement has an empty range [%d, %d)."), Response.Movement.StartFrame, Response.Movement.EndFrame));
			}
			if (Response.Movement.EndFrame > Response.RecoilFrames)
			{
				Add(OutMessages, false, FString::Printf(TEXT("The recoil movement ends at frame %d, after the recoil (%d frames)."), Response.Movement.EndFrame, Response.RecoilFrames));
			}
		}
	}
}

// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_ChargeHold.h"

#include "Input/GSTN_InputHistory.h"
#include "Layout/GSTN_ButtonLayout.h"
#include "Timeline/GSTN_StartupSteer.h"

FGameplayTag FGSTN_TimelineTrack_ChargeHold::ResolveHoldButton() const
{
	return LayoutFunction.IsNone() ? HoldButton : UGSTN_ButtonLayout::GetButton(LayoutFunction, HoldButton);
}

const FGSTN_TimelineTrack_ChargeHold* FGSTN_ChargeHold::FindTrack(const FGSTN_CombatTimelineData& Data, int32* OutIndex)
{
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack_ChargeHold* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_ChargeHold>();
		if (Track && Track->MaxHoldFrames > 0)
		{
			if (OutIndex)
			{
				*OutIndex = TrackIndex;
			}
			return Track;
		}
	}
	if (OutIndex)
	{
		*OutIndex = INDEX_NONE;
	}
	return nullptr;
}

bool FGSTN_ChargeHold::IsHeldSinceStart(const FGSTN_InputHistory& History, int32 Button, int32 TimelineStartFrame)
{
	if (Button < 0 || Button >= 32 || (History.GetHeldButtons() & (1u << Button)) == 0)
	{
		return false;
	}
	const int32 Latest = History.GetLatestFrame();
	int32 HeldFrames = 0;
	if (Latest == INDEX_NONE || !History.IsButtonHeldAt(Button, Latest, HeldFrames))
	{
		return false;
	}
	// Pressed on Latest - HeldFrames: on or before the start (a buffered press counts). MAX_int32 = older than the history, held all along
	return HeldFrames == MAX_int32 || static_cast<int64>(Latest) - HeldFrames <= TimelineStartFrame;
}

bool FGSTN_ChargeHold::ShouldStall(const FGSTN_TimelineTrack_ChargeHold& Track, int32 TimelineFrame, int32 FramesSinceStart, bool bHeldSinceStart)
{
	// The k-th stall is the tick with FramesSinceStart = StartFrame + k (the timeline reached StartFrame on tick StartFrame)
	return bHeldSinceStart && Track.MaxHoldFrames > 0 && Track.StartFrame > 0 && TimelineFrame == Track.StartFrame && FramesSinceStart > Track.StartFrame
		&& FramesSinceStart - Track.StartFrame <= Track.MaxHoldFrames;
}

int32 FGSTN_ChargeHold::GetDirectionLockFrame(const FGSTN_CombatTimelineData& Data)
{
	const FGSTN_TimelineTrack_ChargeHold* Track = FindTrack(Data);
	return Track && Track->bLockDirection ? FMath::Max(Track->StartFrame, 0) : MAX_int32;
}

float FGSTN_ChargeHold::GetDamageScale(const FGSTN_CombatTimelineData& Data, int32 ChargeFrames)
{
	const FGSTN_TimelineTrack_ChargeHold* Track = FindTrack(Data);
	if (!Track || Track->FullChargeDamageScale <= 1.f || ChargeFrames <= 0)
	{
		return 1.f;
	}
	const float Alpha = FMath::Clamp(static_cast<float>(ChargeFrames) / static_cast<float>(Track->MaxHoldFrames), 0.f, 1.f);
	return FMath::Lerp(1.f, Track->FullChargeDamageScale, Alpha);
}

void FGSTN_ChargeHold::Validate(const FGSTN_CombatTimelineData& Data, const FString& Label, TArray<FString>& OutErrors)
{
	int32 Index = INDEX_NONE;
	const FGSTN_TimelineTrack_ChargeHold* Track = FindTrack(Data, &Index);
	if (!Track)
	{
		return;
	}
	if (!Track->HoldButton.IsValid() && Track->LayoutFunction.IsNone())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the Charge Hold has no button and no layout function, so it never holds."), *Label));
	}
	if (Track->StartFrame <= 0)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the Charge Hold stands on frame %d; the first tick never stalls, use frame 1 or later."), *Label, Track->StartFrame));
	}
	const int32 FirstActive = FGSTN_StartupSteer::GetSteerEndFrame(Data);
	if (FirstActive > 0 && Track->StartFrame >= FirstActive)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the Charge Hold frame %d is not in the startup (first active frame %d): the hitbox would stay out while held."),
			*Label, Track->StartFrame, FirstActive));
	}
}

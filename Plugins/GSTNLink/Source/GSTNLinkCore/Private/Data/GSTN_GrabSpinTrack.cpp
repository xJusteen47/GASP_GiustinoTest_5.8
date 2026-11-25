// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_GrabSpinTrack.h"

#include "Timeline/GSTN_TimelineTypes.h"

const FGSTN_GrabSpinTrack* FGSTN_GrabSpinTrack::Find(const FGSTN_CombatTimelineData& Data)
{
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instanced : Data.Tracks)
	{
		if (const FGSTN_GrabSpinTrack* Track = Instanced.GetPtr<FGSTN_GrabSpinTrack>())
		{
			return Track;
		}
	}
	return nullptr;
}

bool FGSTN_GrabSpinTrack::YawAt(float OriginYawDeg, int32 TimelineFrame, float& OutYawDeg) const
{
	const int32 Length = EndFrame - StartFrame;
	// The end frame itself is written too, so the last turn closes exactly on the origin + Turns
	if (Length <= 0 || TimelineFrame < StartFrame || TimelineFrame > EndFrame)
	{
		return false;
	}
	const double Alpha = static_cast<double>(TimelineFrame - StartFrame) / static_cast<double>(Length);
	const double Sign = Sense == EGSTN_GrabSpinSense::Clockwise ? 1.0 : -1.0;
	OutYawDeg = static_cast<float>(FRotator::NormalizeAxis(static_cast<double>(OriginYawDeg) + Sign * 360.0 * static_cast<double>(FMath::Max(Turns, 0.f)) * Alpha));
	return true;
}

void FGSTN_GrabSpinTrack::Validate(const FString& Label, TArray<FString>& OutErrors) const
{
	if (EndFrame <= StartFrame)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the grab spin track [%d, %d) is empty or inverted."), *Label, StartFrame, EndFrame));
	}
	if (Turns <= 0.f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the grab spin track turns %.2f times, so the partner never swings."), *Label, Turns));
	}
}

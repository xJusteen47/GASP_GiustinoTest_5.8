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
	const int32 TurnEnd = ResolveTurnEndFrame();
	const double Sign = Sense == EGSTN_GrabSpinSense::Clockwise ? 1.0 : -1.0;
	const double Whole = 360.0 * static_cast<double>(FMath::Max(Turns, 0.f));
	double Offset = 0.0;
	if (TimelineFrame <= TurnEnd)
	{
		Offset = Whole * static_cast<double>(TimelineFrame - StartFrame) / static_cast<double>(TurnEnd - StartFrame);
	}
	else
	{
		// The settle: past the last turn and back, ending exactly on it (sin(pi) = 0) so the tail leaves the facing where the turns did
		const double U = static_cast<double>(TimelineFrame - TurnEnd) / static_cast<double>(EndFrame - TurnEnd);
		Offset = Whole + static_cast<double>(FMath::Max(SettleOvershootDeg, 0.f)) * FMath::Sin(UE_DOUBLE_PI * U);
	}
	OutYawDeg = static_cast<float>(FRotator::NormalizeAxis(static_cast<double>(OriginYawDeg) + Sign * Offset));
	return true;
}

int32 FGSTN_GrabSpinTrack::ResolveTurnEndFrame() const
{
	return TurnEndFrame > StartFrame && TurnEndFrame <= EndFrame ? TurnEndFrame : EndFrame;
}

bool FGSTN_GrabSpinTrack::IsWallProbeFrame(int32 TimelineFrame) const
{
	return WallHitGroup < 8 && TimelineFrame >= StartFrame && TimelineFrame <= ResolveTurnEndFrame();
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
	if (WallHitGroup != 255 && WallHitGroup >= 8)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the grab spin wall hit group %d is not below 8, so the link tick cannot mask it."), *Label, WallHitGroup));
	}
}

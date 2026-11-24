// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_HitBounce.h"

const FGSTN_TimelineTrack_HitBounce* FGSTN_HitBounceRules::FindTrack(const FGSTN_CombatTimelineData& Data, int32* OutIndex)
{
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		if (const FGSTN_TimelineTrack_HitBounce* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_HitBounce>())
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

FGSTN_HitBounceTuning FGSTN_HitBounceRules::Resolve(const FGSTN_TimelineTrack_HitBounce& Track, const FGSTN_HitBounceOverrides& Overrides)
{
	// Rounded, never truncated: a slider at 1.99 means 2
	FGSTN_HitBounceTuning Tuning;
	Tuning.MaxConsecutiveBounces = FMath::Clamp(Overrides.MaxConsecutiveBounces >= 0.f ? FMath::RoundToInt(Overrides.MaxConsecutiveBounces) : Track.MaxConsecutiveBounces, 0, 15);
	Tuning.ApexHeightCm = FMath::Max(Overrides.ApexHeightCm >= 0.f ? Overrides.ApexHeightCm : Track.ApexHeightCm, 0.f);
	Tuning.RiseFrames = FMath::Clamp(Overrides.RiseFrames >= 0.f ? FMath::RoundToInt(Overrides.RiseFrames) : Track.RiseFrames, 1, 120);
	Tuning.BackDistanceCm = Overrides.BackDistanceCm >= 0.f ? Overrides.BackDistanceCm : Track.BackDistanceCm;
	Tuning.ActionableFrames = FMath::Clamp(Overrides.ActionableFrames >= 0.f ? FMath::RoundToInt(Overrides.ActionableFrames) : Track.ActionableFrames, 1, 120);
	Tuning.bBounceOnBlock = Track.bBounceOnBlock;
	Tuning.bEnabled = !Overrides.bDisabled;
	return Tuning;
}

bool FGSTN_HitBounceRules::CanBounce(int32 Count, const FGSTN_HitBounceTuning& Tuning)
{
	return Tuning.bEnabled && Count < Tuning.MaxConsecutiveBounces;
}

bool FGSTN_HitBounceRules::IsBouncingHit(bool bRicochet, bool bBlocked, bool bParried, bool bClank, const FGSTN_HitBounceTuning& Tuning)
{
	return !bRicochet && !bParried && !bClank && (!bBlocked || Tuning.bBounceOnBlock);
}

FGSTN_HitBounceLaunch FGSTN_HitBounceRules::ComputeLaunch(const FGSTN_HitBounceTuning& Tuning, const FVector& Up, const FVector& Facing, double ModeGravity, double StepSeconds)
{
	FGSTN_HitBounceLaunch Launch;
	Launch.Frames = FMath::Max(Tuning.RiseFrames, 1);
	const double RiseSeconds = Launch.Frames * FMath::Max(StepSeconds, UE_SMALL_NUMBER);
	const FVector UpDir = Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	// The facing on the plane of up: a facing along up (never in practice) gives no back speed
	const FVector Flat = (Facing - UpDir * Facing.Dot(UpDir)).GetSafeNormal();
	const double UpSpeed = 2.0 * Tuning.ApexHeightCm / RiseSeconds;
	const double BackSpeed = Tuning.BackDistanceCm / RiseSeconds;
	Launch.Velocity = UpDir * UpSpeed - Flat * BackSpeed;
	const double BounceGravity = 2.0 * Tuning.ApexHeightCm / (RiseSeconds * RiseSeconds);
	Launch.GravityScale = ModeGravity > 1.0 ? static_cast<float>(BounceGravity / ModeGravity) : 1.f;
	return Launch;
}

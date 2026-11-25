#include "Timeline/GSTN_TimelineUtils.h"
#include "Timeline/GSTN_ChargeHold.h"
#include "Timeline/GSTN_HitBounce.h"
#include "Timeline/GSTN_HoldUntilLanded.h"
#include "Timeline/GSTN_TimelinePlayer.h"

bool UGSTN_TimelineUtils::IsInTail(const FGSTN_CombatTimelineData& Data, int32 Frame)
{
	return FGSTN_TimelinePlayer::IsInTail(Data, Frame);
}

int32 UGSTN_TimelineUtils::GetTailStartFrame(const FGSTN_CombatTimelineData& Data)
{
	return Data.GetTailStartFrame();
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::AddHitboxTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, const FGSTN_PartSelector& Part, const TArray<FGSTN_HitboxSpec>& Hitboxes)
{
	FGSTN_CombatTimelineData Result = Data;
	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = StartFrame;
	Track.EndFrame = EndFrame;
	Track.Part = Part;
	Track.Hitboxes = Hitboxes;
	// The derived type must be named or the track is sliced to the base
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::SetHoldUntilLandedTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, int32 LandFrame,
	int32 LandFromFrame, FName LoopSection, FName LandSection, int32 MaxAirFrames)
{
	FGSTN_CombatTimelineData Result = RemoveHoldUntilLandedTrack(Data);
	FGSTN_TimelineTrack_HoldUntilLanded Track;
	Track.StartFrame = StartFrame;
	Track.EndFrame = EndFrame;
	Track.LandFrame = LandFrame;
	Track.LandFromFrame = FMath::Max(LandFromFrame, 0);
	Track.LoopSection = LoopSection;
	Track.LandSection = LandSection;
	Track.MaxAirFrames = FMath::Max(MaxAirFrames, 0);
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HoldUntilLanded>(Track));
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::RemoveHoldUntilLandedTrack(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_CombatTimelineData Result = Data;
	Result.Tracks.RemoveAll([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		return Track.GetScriptStruct() == FGSTN_TimelineTrack_HoldUntilLanded::StaticStruct();
	});
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::SetChargeHoldTrack(const FGSTN_CombatTimelineData& Data, int32 HoldFrame, FGameplayTag HoldButton, FName LayoutFunction,
	int32 MaxHoldFrames, float FullChargeDamageScale, bool bLockDirection, FName LoopSection)
{
	FGSTN_CombatTimelineData Result = RemoveChargeHoldTrack(Data);
	FGSTN_TimelineTrack_ChargeHold Track;
	Track.StartFrame = FMath::Max(HoldFrame, 0);
	Track.EndFrame = Track.StartFrame + 1;
	Track.HoldButton = HoldButton;
	Track.LayoutFunction = LayoutFunction;
	Track.MaxHoldFrames = FMath::Max(MaxHoldFrames, 0);
	Track.FullChargeDamageScale = FMath::Clamp(FullChargeDamageScale, 1.f, 3.f);
	Track.bLockDirection = bLockDirection;
	Track.LoopSection = LoopSection;
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_ChargeHold>(Track));
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::RemoveChargeHoldTrack(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_CombatTimelineData Result = Data;
	Result.Tracks.RemoveAll([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		return Track.GetScriptStruct() == FGSTN_TimelineTrack_ChargeHold::StaticStruct();
	});
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::AddBodyCapsuleTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, float HalfHeight, float Radius,
	FVector Offset, EGSTN_BodyCapsuleAnchor Anchor, float MaxShiftPerFrame)
{
	FGSTN_CombatTimelineData Result = Data;
	FGSTN_TimelineTrack_BodyCapsule Track;
	Track.StartFrame = StartFrame;
	Track.EndFrame = EndFrame;
	Track.HalfHeight = FMath::Max(HalfHeight, 1.f);
	Track.Radius = FMath::Max(Radius, 0.f);
	Track.Offset = Offset;
	Track.Anchor = Anchor;
	Track.MaxShiftPerFrame = FMath::Max(MaxShiftPerFrame, 0.f);
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_BodyCapsule>(Track));
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::RemoveAttackHitboxTracks(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_CombatTimelineData Result = Data;
	Result.Tracks.RemoveAll([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		return GSTNTimelineTracks::AsAttackHitbox(Track) != nullptr;
	});
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::RemoveBodyCapsuleTracks(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_CombatTimelineData Result = Data;
	Result.Tracks.RemoveAll([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		return Track.GetScriptStruct() == FGSTN_TimelineTrack_BodyCapsule::StaticStruct();
	});
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::AddPushboxTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, bool bEnabled, float Radius,
	float HalfHeight, bool bUseOffset, FVector Offset)
{
	FGSTN_CombatTimelineData Result = Data;
	FGSTN_TimelineTrack_Pushbox Track;
	Track.StartFrame = StartFrame;
	Track.EndFrame = EndFrame;
	Track.bEnabled = bEnabled;
	Track.Radius = FMath::Max(Radius, 0.f);
	Track.HalfHeight = FMath::Max(HalfHeight, 0.f);
	Track.bUseOffset = bUseOffset;
	Track.Offset = Offset;
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Pushbox>(Track));
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::RemovePushboxTracks(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_CombatTimelineData Result = Data;
	Result.Tracks.RemoveAll([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		return Track.GetScriptStruct() == FGSTN_TimelineTrack_Pushbox::StaticStruct();
	});
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::AddLandingTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, EGSTN_LandingAction Action,
	const FGSTN_RicochetResponse& Lag, TSoftClassPtr<UObject> ContinueMove)
{
	FGSTN_CombatTimelineData Result = Data;
	FGSTN_TimelineTrack_Landing Track;
	Track.StartFrame = StartFrame;
	Track.EndFrame = EndFrame;
	Track.Action = Action;
	Track.Lag = Lag;
	Track.ContinueMove = ContinueMove;
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Landing>(Track));
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::RemoveLandingTracks(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_CombatTimelineData Result = Data;
	Result.Tracks.RemoveAll([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		return Track.GetScriptStruct() == FGSTN_TimelineTrack_Landing::StaticStruct();
	});
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::SetHitBounceTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, int32 MaxConsecutiveBounces,
	float ApexHeightCm, int32 RiseFrames, float BackDistanceCm, int32 ActionableFrames, bool bBounceOnBlock, TSoftClassPtr<UObject> BounceMove)
{
	FGSTN_CombatTimelineData Result = RemoveHitBounceTracks(Data);
	FGSTN_TimelineTrack_HitBounce Track;
	Track.StartFrame = StartFrame;
	Track.EndFrame = EndFrame;
	Track.MaxConsecutiveBounces = MaxConsecutiveBounces;
	Track.ApexHeightCm = ApexHeightCm;
	Track.RiseFrames = RiseFrames;
	Track.BackDistanceCm = BackDistanceCm;
	Track.ActionableFrames = ActionableFrames;
	Track.bBounceOnBlock = bBounceOnBlock;
	Track.BounceMove = BounceMove;
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HitBounce>(Track));
	return Result;
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::RemoveHitBounceTracks(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_CombatTimelineData Result = Data;
	Result.Tracks.RemoveAll([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		return Track.GetScriptStruct() == FGSTN_TimelineTrack_HitBounce::StaticStruct();
	});
	return Result;
}

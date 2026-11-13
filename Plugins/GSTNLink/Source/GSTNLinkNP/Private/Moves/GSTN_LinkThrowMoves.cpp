// Links, Network Prediction half: the common throw set (M11 K2).

#include "Moves/GSTN_LinkThrowMoves.h"

#include "Data/GSTN_LinkOutcomeTrack.h"
#include "GSTN_LinkTags.h"
#include "Timeline/GSTN_TimelineTypes.h"

UGSTN_GA_LinkThrow::UGSTN_GA_LinkThrow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGSTN_GA_LinkThrow::BuildThrow(int32 TotalFrames, int32 FirstActionable, int32 ThrowFrame, float Damage, float BaseKnockback, float AngleDeg)
{
	FillThrowTimeline(Timeline, TotalFrames, FirstActionable, ThrowFrame, Damage, BaseKnockback, AngleDeg);
}

void UGSTN_GA_LinkThrow::FillThrowTimeline(FGSTN_CombatTimelineData& OutTimeline, int32 TotalFrames, int32 FirstActionable, int32 ThrowFrame, float Damage,
	float BaseKnockback, float AngleDeg)
{
	OutTimeline.Tracks.Reset();
	OutTimeline.TotalFrames = TotalFrames;
	OutTimeline.FirstActionableFrame = FirstActionable;
	OutTimeline.OverrideFrames = 0;

	FGSTN_LinkOutcomeKey Key;
	Key.Frame = ThrowFrame;
	Key.Outcome = EGSTN_LinkOutcome::Throw;
	// The native tag, not a request by name: a class default is built when the module loads
	Key.ThrowHit.HitType = GSTNLinkTags::HitType_Throw;
	Key.ThrowHit.Damage = Damage;
	Key.ThrowHit.BaseKnockback = BaseKnockback;
	Key.ThrowHit.AngleDeg = AngleDeg;
	Key.ThrowHit.bParryable = false;
	Key.ThrowHit.HitstopMode = EGSTN_HitstopMode::Override;
	Key.ThrowHit.HitstopFramesOverride = 6;
	Key.ThrowHit.AttackerHitstopFramesOverride = 0;

	FGSTN_LinkOutcomeTrack Track;
	Track.StartFrame = 0;
	Track.EndFrame = TotalFrames;
	Track.Keys.Add(Key);
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_LinkOutcomeTrack>(Track));
}

UGSTN_GA_LinkThrowForward::UGSTN_GA_LinkThrowForward(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BuildThrow(30, 24, 8, 8.f, 1600.f, 40.f);
}

UGSTN_GA_LinkThrowBack::UGSTN_GA_LinkThrowBack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BuildThrow(36, 30, 12, 10.f, 1800.f, 140.f);
}

UGSTN_GA_LinkThrowUp::UGSTN_GA_LinkThrowUp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BuildThrow(32, 26, 10, 7.f, 1700.f, 88.f);
}

UGSTN_GA_LinkThrowDown::UGSTN_GA_LinkThrowDown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BuildThrow(34, 28, 14, 6.f, 900.f, 80.f);
}

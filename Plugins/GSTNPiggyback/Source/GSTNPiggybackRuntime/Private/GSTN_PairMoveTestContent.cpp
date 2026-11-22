// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_PairMoveTestContent.h"

#include "Data/GSTN_LinkOutcomeTrack.h"
#include "GSTN_CombatTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Timeline/GSTN_TimelineTypes.h"

namespace
{
	void SetPairMoveFrames(FGSTN_CombatTimelineData& Timeline, int32 Total, int32 FirstActionable)
	{
		Timeline.TotalFrames = Total;
		Timeline.FirstActionableFrame = FirstActionable;
		Timeline.OverrideFrames = 0;
	}

	void AddPartnerMoveKey(FGSTN_CombatTimelineData& Timeline, int32 Frame, int32 PairMoveId)
	{
		FGSTN_LinkOutcomeKey Key;
		Key.Frame = Frame;
		Key.Outcome = EGSTN_LinkOutcome::PartnerMove;
		Key.PartnerMoveId = PairMoveId;
		FGSTN_LinkOutcomeTrack Track;
		Track.StartFrame = 0;
		Track.EndFrame = Timeline.TotalFrames;
		Track.Keys.Add(Key);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_LinkOutcomeTrack>(Track));
	}
}

// The ids the definitions author (FillReferencePairMove): the keys must name them
static constexpr int32 GThrowRiderId = 1;
static constexpr int32 GBoostJumpId = 2;

UGSTN_GA_Test_CarrierThrowRider::UGSTN_GA_Test_CarrierThrowRider(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetPairMoveFrames(Timeline, 32, 32);
	AddPartnerMoveKey(Timeline, KeyFrame, GThrowRiderId);
}

UGSTN_GA_Test_RiderThrown::UGSTN_GA_Test_RiderThrown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetPairMoveFrames(Timeline, 40, ThrownActionableFrame);

	FGSTN_HitboxSpec Spec;
	Spec.Shape.Type = EGSTN_ShapeType::Capsule;
	Spec.Shape.Extent = FVector(45.0, 45.0, 90.0);
	Spec.Space = EGSTN_HitboxSpace::Root;
	FGSTN_HitboxKey Key;
	Key.Frame = 0;
	Spec.Keys = { Key };
	Spec.HitGroup = 0;
	Spec.MaxTargetsPerGroup = 1;
	// The rider flies fast: without the sweep a target between two frames' capsules is skipped
	Spec.bSweepBetweenFrames = true;
	Spec.Data.Damage = 6.f;
	Spec.Data.BaseKnockback = 500.f;
	Spec.Data.KnockbackGrowth = 0.f;
	Spec.Data.AngleDeg = 30.f;
	Spec.Data.YawSource = EGSTN_YawSource::HitboxVelocity;
	// Never the carrier (an ally, already outside the match rules' hit) nor another ally
	Spec.RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Ally.GetTag()).bIgnore = true;

	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = 2;
	Track.EndFrame = 40;
	Track.Part.GroupName = TEXT("Body");
	Track.Hitboxes.Add(Spec);
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
}

UGSTN_GA_Test_RiderBoostJump::UGSTN_GA_Test_RiderBoostJump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetPairMoveFrames(Timeline, 14, 10);
	AddPartnerMoveKey(Timeline, KeyFrame, GBoostJumpId);
}

UGSTN_GA_Test_CarrierBraced::UGSTN_GA_Test_CarrierBraced(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetPairMoveFrames(Timeline, 12, 10);
	FGSTN_TimelineTrack_Tags Track;
	Track.StartFrame = 0;
	Track.EndFrame = LockFrames;
	Track.Tags.AddTag(GSTNCombatTags::Lock_Movement.GetTag());
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Tags>(Track));
}

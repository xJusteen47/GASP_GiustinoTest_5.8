// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_MountAbilities.h"

#include "Data/GSTN_LinkOutcomeTrack.h"
#include "GSTN_PiggybackTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Timeline/GSTN_TimelineTypes.h"

namespace
{
	void SetFrames(FGSTN_CombatTimelineData& Timeline, int32 Total, int32 FirstActionable)
	{
		Timeline.TotalFrames = Total;
		Timeline.FirstActionableFrame = FirstActionable;
		Timeline.OverrideFrames = 0;
	}

	void AddRelease(FGSTN_CombatTimelineData& Timeline, int32 Frame)
	{
		FGSTN_LinkOutcomeKey Key;
		Key.Frame = Frame;
		Key.Outcome = EGSTN_LinkOutcome::Release;
		FGSTN_LinkOutcomeTrack Track;
		Track.StartFrame = 0;
		Track.EndFrame = Timeline.TotalFrames;
		Track.Keys.Add(Key);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_LinkOutcomeTrack>(Track));
	}
}

UGSTN_GA_Mount::UGSTN_GA_Mount(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 24, 16);

	FGSTN_HitboxSpec Spec;
	Spec.Shape.Type = EGSTN_ShapeType::Sphere;
	Spec.Shape.Extent = FVector(70.0);
	Spec.Space = EGSTN_HitboxSpace::Root;
	FGSTN_HitboxKey Key;
	Key.Frame = 0;
	Key.Offset = FVector(60.0, 0.0, 0.0);
	Spec.Keys = { Key };
	// One carrier per execution: a mount never asks two allies at once
	Spec.MaxTargetsPerGroup = 1;
	Spec.Data.HitType = GSTNPiggybackTags::HitType_Interaction_Mount;
	Spec.Data.LinkType = GSTNPiggybackTags::Type_Mount;
	Spec.Data.Damage = 0.f;
	Spec.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Spec.Data.HitstopFramesOverride = 0;
	Spec.Data.AttackerHitstopFramesOverride = 0;

	// Allies only. The native tags, not a request by name: a class default is built when the module loads, possibly before the tag tree exists
	Spec.RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Ally.GetTag()).bForceHittable = true;
	Spec.RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Enemy.GetTag()).bIgnore = true;
	Spec.RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Neutral.GetTag()).bIgnore = true;

	// One frame: a refused request must not be asked again by the same move. With a longer window the rider's client, restored to the refused
	// state by its rollback, re-requested the carrier on the next active frame and accepted optimistically again (three rollbacks for one
	// refusal in the G5 harness, mount_refused)
	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = 3;
	Track.EndFrame = 4;
	// The whole body, spelled as the group that holds every part (the timeline validator refuses an empty selector)
	Track.Part.GroupName = TEXT("Body");
	Track.Hitboxes.Add(Spec);
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
}

UGSTN_GA_Dismount::UGSTN_GA_Dismount(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 12, 8);
	AddRelease(Timeline, 1);
}

UGSTN_GA_Drop::UGSTN_GA_Drop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 14, 10);
	AddRelease(Timeline, 2);
}

// Links, Network Prediction half: the harness's link moves.

#include "Harness/GSTN_LinkTestAbilities.h"

#include "Data/GSTN_LinkHitData.h"
#include "Data/GSTN_LinkOutcomeTrack.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Timeline/GSTN_TimelineTypes.h"

namespace
{
	FGSTN_HitboxSpec MakeSphere(double Radius, const FVector& Offset)
	{
		FGSTN_HitboxSpec Spec;
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(Radius);
		Spec.Space = EGSTN_HitboxSpace::Root;
		FGSTN_HitboxKey Key;
		Key.Frame = 0;
		Key.Offset = Offset;
		Spec.Keys = { Key };
		return Spec;
	}

	void AddHitbox(FGSTN_CombatTimelineData& Timeline, int32 Start, int32 End, const FGSTN_HitboxSpec& Spec)
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.Hitboxes.Add(Spec);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	}

	void AddOutcome(FGSTN_CombatTimelineData& Timeline, const FGSTN_LinkOutcomeKey& Key)
	{
		FGSTN_LinkOutcomeTrack Track;
		Track.StartFrame = 0;
		Track.EndFrame = Timeline.TotalFrames;
		Track.Keys.Add(Key);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_LinkOutcomeTrack>(Track));
	}

	void SetFrames(FGSTN_CombatTimelineData& Timeline, int32 Total, int32 FirstActionable)
	{
		Timeline.TotalFrames = Total;
		Timeline.FirstActionableFrame = FirstActionable;
		Timeline.OverrideFrames = 0;
	}

	FInstancedStruct NoReaction()
	{
		FGSTN_LinkHitData Data;
		Data.bNoReaction = true;
		return FInstancedStruct::Make(Data);
	}
}

UGSTN_GA_LinkTestGrab::UGSTN_GA_LinkTestGrab(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 24, 20);
	FGSTN_HitboxSpec Spec = MakeSphere(60.0, FVector(90.0, 0.0, 0.0));
	Spec.Data.HitType = GSTNCombatHitTags::HitType_Grab;
	Spec.Data.LinkType = GSTNLinkTags::Type_Grab;
	Spec.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Spec.Data.HitstopFramesOverride = 0;
	Spec.Data.AttackerHitstopFramesOverride = 0;
	AddHitbox(Timeline, 4, 8, Spec);

	// The grab connects: the grabber may act on it at once (pummel, throw, release) instead of waiting for the end of the move
	FGSTN_TimelineTrack_CancelWindow Window;
	Window.StartFrame = 6;
	Window.EndFrame = 24;
	Window.Condition = EGSTN_CancelCondition::OnHit;
	// The native tags, not a request by name: a class default is built when the module loads, possibly before the tag manager has its tree
	FGameplayTagContainer Into;
	for (const FNativeGameplayTag* Tag : { &GSTNLinkTags::Command_Link_Pummel, &GSTNLinkTags::Command_Link_Release, &GSTNLinkTags::Command_Link_Throw_Forward,
		&GSTNLinkTags::Command_Link_Throw_Back, &GSTNLinkTags::Command_Link_Throw_Up, &GSTNLinkTags::Command_Link_Throw_Down })
	{
		Into.AddTag(Tag->GetTag());
	}
	Window.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(Into);
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Window));
}

UGSTN_GA_LinkTestPummel::UGSTN_GA_LinkTestPummel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 18, 14);
	FGSTN_HitboxSpec Spec = MakeSphere(130.0, FVector(40.0, 0.0, 0.0));
	Spec.Data.HitType = GSTNLinkTags::HitType_Pummel;
	Spec.Data.LinkFilter = EGSTN_LinkFilter::LinkedPartnerOnly;
	Spec.Data.Damage = 3.f;
	Spec.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Spec.Data.HitstopFramesOverride = 4;
	Spec.Data.AttackerHitstopFramesOverride = 4;
	Spec.Data.ExtensionData.Add(NoReaction());
	AddHitbox(Timeline, 3, 6, Spec);
}

UGSTN_GA_LinkTestThrow::UGSTN_GA_LinkTestThrow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 30, 24);
	FGSTN_LinkOutcomeKey Key;
	Key.Frame = 8;
	Key.Outcome = EGSTN_LinkOutcome::Throw;
	Key.ThrowHit.HitType = GSTNLinkTags::HitType_Throw;
	Key.ThrowHit.Damage = 8.f;
	Key.ThrowHit.BaseKnockback = 1600.f;
	Key.ThrowHit.AngleDeg = 40.f;
	Key.ThrowHit.bParryable = false;
	Key.ThrowHit.HitstopMode = EGSTN_HitstopMode::Override;
	Key.ThrowHit.HitstopFramesOverride = 6;
	Key.ThrowHit.AttackerHitstopFramesOverride = 0;
	AddOutcome(Timeline, Key);
}

UGSTN_GA_LinkTestRelease::UGSTN_GA_LinkTestRelease(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 10, 6);
	FGSTN_LinkOutcomeKey Key;
	Key.Frame = 1;
	Key.Outcome = EGSTN_LinkOutcome::Release;
	AddOutcome(Timeline, Key);
}

UGSTN_GA_LinkTestLatchAttack::UGSTN_GA_LinkTestLatchAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFrames(Timeline, 20, 16);
	FGSTN_HitboxSpec Spec = MakeSphere(160.0, FVector::ZeroVector);
	Spec.Data.LinkFilter = EGSTN_LinkFilter::LinkedPartnerOnly;
	Spec.Data.Damage = 5.f;
	Spec.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Spec.Data.HitstopFramesOverride = 4;
	Spec.Data.AttackerHitstopFramesOverride = 4;
	Spec.Data.ExtensionData.Add(NoReaction());
	AddHitbox(Timeline, 4, 9, Spec);
}

// Links, Network Prediction half: the grab spin (owner 2026-11-25).

#include "Moves/GSTN_LinkGrabSpinMoves.h"

#include "Data/GSTN_LinkHitData.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Timeline/GSTN_TimelineTypes.h"

namespace
{
	FGSTN_TimelineTrack_Hitbox MakeSphereTrack(int32 Start, int32 End, double Radius, const FVector& Offset, const FGSTN_HitData& Data)
	{
		FGSTN_HitboxSpec Spec;
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(Radius);
		// Root space at the frame evaluated: the sphere turns with the grabber, so it sweeps the circle the held partner draws
		Spec.Space = EGSTN_HitboxSpace::Root;
		FGSTN_HitboxKey Key;
		Key.Frame = 0;
		Key.Offset = Offset;
		Spec.Keys = { Key };
		Spec.Data = Data;

		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = Start;
		Track.EndFrame = FMath::Max(End, Start + 1);
		// The whole body, spelled as the group that holds every part (the timeline validator refuses an empty selector), like the harness moves
		Track.Part.GroupName = TEXT("Body");
		Track.Hitboxes.Add(Spec);
		return Track;
	}
}

UGSTN_GA_LinkGrabSpin::UGSTN_GA_LinkGrabSpin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGameplayTagContainer UGSTN_GA_LinkGrabSpin::ChainCommands()
{
	using namespace GSTNLinkTags;
	// The native tags, not a request by name: a class default is built when the module loads, possibly before the tag manager has its tree
	FGameplayTagContainer Into;
	for (const FNativeGameplayTag* Tag : { &Command_Link_Spin_Clockwise, &Command_Link_Spin_CounterClockwise, &Command_Link_Release, &Command_Link_Release_PutDown,
		&Command_Link_Throw_Forward, &Command_Link_Throw_Back, &Command_Link_Throw_Up, &Command_Link_Throw_Down, &Command_Link_Throw_Neutral,
		&Command_Link_Throw_CrouchForward })
	{
		Into.AddTag(Tag->GetTag());
	}
	return Into;
}

void UGSTN_GA_LinkGrabSpin::FillGrabSpinTimeline(FGSTN_CombatTimelineData& OutTimeline, const FGSTN_GrabSpinMoveSpec& Spec, EGSTN_GrabSpinSense Sense)
{
	const int32 Total = FMath::Max(Spec.TotalFrames, 4);
	OutTimeline.Tracks.Reset();
	OutTimeline.TotalFrames = Total;
	OutTimeline.FirstActionableFrame = Total;
	OutTimeline.OverrideFrames = 0;

	FGSTN_GrabSpinTrack Spin;
	Spin.StartFrame = 0;
	Spin.EndFrame = Total;
	Spin.Sense = Sense;
	Spin.Turns = Spec.Turns;
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_GrabSpinTrack>(Spin));

	// The held partner as a weapon: the fighters around, never the partner itself (it takes its own hit below)
	FGSTN_HitData Ring;
	Ring.HitType = GSTNLinkTags::HitType_GrabSpin;
	Ring.LinkFilter = EGSTN_LinkFilter::ExcludeLinkedPartner;
	Ring.Damage = Spec.RingDamage;
	Ring.BaseKnockback = Spec.RingBaseKnockback;
	Ring.KnockbackGrowth = Spec.RingKnockbackGrowth;
	Ring.AngleDeg = Spec.RingAngleDeg;
	Ring.HitstopMode = EGSTN_HitstopMode::Override;
	Ring.HitstopFramesOverride = 6;
	// No hitstop on the spinning grabber: a pause would stop the turn mid-swing (the yaw is a function of the timeline frame, so it would resume cleanly)
	Ring.AttackerHitstopFramesOverride = 0;
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(
		MakeSphereTrack(Spec.RingStartFrame, FMath::Min(Spec.RingEndFrame, Total), Spec.RingRadius, FVector(Spec.RingReach, 0.0, 0.0), Ring)));

	// The partner takes a pummel each turn: damage, no reaction (it stays held), and the turn costs grab time
	FGSTN_HitData Held;
	Held.HitType = GSTNLinkTags::HitType_Pummel;
	Held.LinkFilter = EGSTN_LinkFilter::LinkedPartnerOnly;
	Held.Damage = Spec.HeldDamage;
	Held.HitstopMode = EGSTN_HitstopMode::Override;
	Held.HitstopFramesOverride = 2;
	Held.AttackerHitstopFramesOverride = 0;
	FGSTN_LinkHitData Link;
	Link.bNoReaction = true;
	Link.GrabTimeCostFrames = Spec.GrabTimeCostFrames;
	Held.ExtensionData.Add(FInstancedStruct::Make(Link));
	const int32 HeldFrame = FMath::Clamp(Spec.HeldHitFrame, 0, Total - 1);
	// A sphere round the grabber's body that surely holds the parked partner; the filter keeps it on the partner alone
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(
		MakeSphereTrack(HeldFrame, HeldFrame + 2, 130.0, FVector(40.0, 0.0, 0.0), Held)));

	FGSTN_TimelineTrack_CancelWindow Window;
	Window.StartFrame = FMath::Clamp(Spec.ChainFromFrame, 1, Total - 1);
	Window.EndFrame = Total;
	Window.Condition = EGSTN_CancelCondition::Always;
	Window.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(ChainCommands());
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Window));
}

UGSTN_GA_LinkGrabSpinClockwise::UGSTN_GA_LinkGrabSpinClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FillGrabSpinTimeline(Timeline, FGSTN_GrabSpinMoveSpec(), EGSTN_GrabSpinSense::Clockwise);
}

UGSTN_GA_LinkGrabSpinCounterClockwise::UGSTN_GA_LinkGrabSpinCounterClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FillGrabSpinTimeline(Timeline, FGSTN_GrabSpinMoveSpec(), EGSTN_GrabSpinSense::CounterClockwise);
}

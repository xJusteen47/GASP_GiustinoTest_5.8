// Links, Network Prediction half: the grab spin (owner 2026-11-25).

#include "Moves/GSTN_LinkGrabSpinMoves.h"

#include "Data/GSTN_LinkHitData.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Timeline/GSTN_TimelineTypes.h"

namespace
{
	FGSTN_TimelineTrack_Hitbox MakeSphereTrack(int32 Start, int32 End, double Radius, const FVector& Offset, const FGSTN_HitData& Data, uint8 HitGroup)
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
		Spec.HitGroup = HitGroup;

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

FGameplayTagContainer UGSTN_GA_LinkGrabSpin::ThrowCommands()
{
	using namespace GSTNLinkTags;
	// The native tags, not a request by name: a class default is built when the module loads, possibly before the tag manager has its tree
	FGameplayTagContainer Into;
	for (const FNativeGameplayTag* Tag : { &Command_Link_Release, &Command_Link_Release_PutDown, &Command_Link_Throw_Forward, &Command_Link_Throw_Back,
		&Command_Link_Throw_Up, &Command_Link_Throw_Down, &Command_Link_Throw_Neutral, &Command_Link_Throw_CrouchForward })
	{
		Into.AddTag(Tag->GetTag());
	}
	return Into;
}

FGameplayTagContainer UGSTN_GA_LinkGrabSpin::ChainCommands()
{
	FGameplayTagContainer Into;
	Into.AddTag(GSTNLinkTags::Command_Link_Spin_Clockwise.GetTag());
	Into.AddTag(GSTNLinkTags::Command_Link_Spin_CounterClockwise.GetTag());
	return Into;
}

void UGSTN_GA_LinkGrabSpin::FillGrabSpinTimeline(FGSTN_CombatTimelineData& OutTimeline, const FGSTN_GrabSpinMoveSpec& Spec, EGSTN_GrabSpinSense Sense)
{
	const int32 Turns = FMath::Max(Spec.TurnFrames, 4);
	const int32 Total = Turns + FMath::Max(Spec.SettleFrames, 0);
	OutTimeline.Tracks.Reset();
	OutTimeline.TotalFrames = Total;
	OutTimeline.FirstActionableFrame = Turns;
	OutTimeline.OverrideFrames = 0;
	// The settle (owner 2026-11-25, "solo estetico"): the frames after the turns are a tail that anything ends, the stick included
	OutTimeline.Tail.bHasTail = Total > Turns;
	OutTimeline.Tail.StartFrame = INDEX_NONE;
	OutTimeline.Tail.BlendOutFrames = 6;
	// The spin keeps the facing it starts with: the stick is mid-rotation when the move comes out, so the default start aim (snap to the stick) and
	// a reverse would jerk the held partner to wherever the thumb happens to be; the turn is the track's alone
	OutTimeline.AimPolicy = EGSTN_AimPolicy::KeepFacing;
	OutTimeline.MaxSteerDegrees = 0.f;
	OutTimeline.Reverse.Frames = 0;

	FGSTN_GrabSpinTrack Spin;
	Spin.StartFrame = 0;
	Spin.EndFrame = Total;
	Spin.TurnEndFrame = Turns;
	Spin.Sense = Sense;
	Spin.Turns = Spec.Turns;
	Spin.SettleOvershootDeg = Spec.SettleOvershootDeg;
	Spin.WallHitGroup = Spec.WallDamage > 0.f ? GSTNGrabSpinGroups::Wall : 255;
	Spin.WallProbeReach = Spec.WallProbeReach;
	Spin.WallProbeRadius = Spec.WallProbeRadius;
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
		MakeSphereTrack(Spec.RingStartFrame, FMath::Min(Spec.RingEndFrame, Turns), Spec.RingRadius, FVector(Spec.RingReach, 0.0, 0.0), Ring, GSTNGrabSpinGroups::Ring)));

	// The partner takes a pummel each move: damage, no reaction (it stays held), and the move costs grab time
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
	const int32 HeldFrame = FMath::Clamp(Spec.HeldHitFrame, 0, Turns - 1);
	// A sphere round the grabber's body that surely holds the parked partner; the filter keeps it on the partner alone
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(
		MakeSphereTrack(HeldFrame, HeldFrame + 2, 130.0, FVector(40.0, 0.0, 0.0), Held, GSTNGrabSpinGroups::Held)));

	// The wall slam (owner 2026-11-25: "sbattere al muro dovrebbe far danno al nemico da lanciare con un po' di hitstop"): live over the turns, masked
	// by the link tick on every frame the wall probe is clear, so it lands on the first frame the partner meets a wall, once per move
	if (Spec.WallDamage > 0.f)
	{
		FGSTN_HitData Wall;
		Wall.HitType = GSTNLinkTags::HitType_GrabSpinWall;
		Wall.LinkFilter = EGSTN_LinkFilter::LinkedPartnerOnly;
		Wall.Damage = Spec.WallDamage;
		Wall.bParryable = false;
		Wall.HitstopMode = EGSTN_HitstopMode::Override;
		Wall.HitstopFramesOverride = FMath::Max(Spec.WallHitstopFrames, 0);
		Wall.AttackerHitstopFramesOverride = FMath::Max(Spec.WallAttackerHitstopFrames, 0);
		FGSTN_LinkHitData WallLink;
		WallLink.bNoReaction = true;
		Wall.ExtensionData.Add(FInstancedStruct::Make(WallLink));
		OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(
			MakeSphereTrack(1, Turns, 130.0, FVector(40.0, 0.0, 0.0), Wall, GSTNGrabSpinGroups::Wall)));
	}

	FGSTN_TimelineTrack_CancelWindow Throws;
	Throws.StartFrame = FMath::Clamp(Spec.ThrowFromFrame, 1, Turns - 1);
	Throws.EndFrame = Turns;
	Throws.Condition = EGSTN_CancelCondition::Always;
	Throws.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(ThrowCommands());
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Throws));

	FGSTN_TimelineTrack_CancelWindow Chain;
	Chain.StartFrame = FMath::Clamp(Spec.ChainFromFrame, 1, Turns - 1);
	Chain.EndFrame = Turns;
	Chain.Condition = EGSTN_CancelCondition::Always;
	Chain.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(ChainCommands());
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Chain));
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

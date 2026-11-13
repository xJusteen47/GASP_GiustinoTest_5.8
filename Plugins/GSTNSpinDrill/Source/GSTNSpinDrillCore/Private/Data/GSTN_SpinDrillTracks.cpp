// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_SpinDrillTracks.h"

#include "Data/GSTN_SpinTimelines.h"
#include "GSTN_SpinDrillTags.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Layout/GSTN_ButtonLayout.h"

FGameplayTag FGSTN_BoreTrack::ResolveHoldButton() const
{
	return LayoutFunction.IsNone() ? HoldButton : UGSTN_ButtonLayout::GetButton(LayoutFunction, HoldButton);
}

const FGSTN_SpinTrack* FGSTN_SpinTrack::Find(const FGSTN_CombatTimelineData& Data)
{
	const UScriptStruct* SpinStruct = FGSTN_SpinTrack::StaticStruct();
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
	{
		const UScriptStruct* Struct = Track.GetScriptStruct();
		if (Struct && Struct->IsChildOf(SpinStruct))
		{
			return &Track.Get<FGSTN_SpinTrack>();
		}
	}
	return nullptr;
}

const FGSTN_SpinCancelTrack* FGSTN_SpinCancelTrack::Find(const FGSTN_CombatTimelineData& Data)
{
	const UScriptStruct* CancelStruct = FGSTN_SpinCancelTrack::StaticStruct();
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
	{
		const UScriptStruct* Struct = Track.GetScriptStruct();
		if (Struct && Struct->IsChildOf(CancelStruct))
		{
			return &Track.Get<FGSTN_SpinCancelTrack>();
		}
	}
	return nullptr;
}

const FGSTN_DiveTrack* FGSTN_DiveTrack::Find(const FGSTN_CombatTimelineData& Data)
{
	const UScriptStruct* DiveStruct = FGSTN_DiveTrack::StaticStruct();
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
	{
		const UScriptStruct* Struct = Track.GetScriptStruct();
		if (Struct && Struct->IsChildOf(DiveStruct))
		{
			return &Track.Get<FGSTN_DiveTrack>();
		}
	}
	return nullptr;
}

const FGSTN_BoreTrack* FGSTN_BoreTrack::Find(const FGSTN_CombatTimelineData& Data)
{
	const UScriptStruct* BoreStruct = FGSTN_BoreTrack::StaticStruct();
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
	{
		const UScriptStruct* Struct = Track.GetScriptStruct();
		if (Struct && Struct->IsChildOf(BoreStruct))
		{
			return &Track.Get<FGSTN_BoreTrack>();
		}
	}
	return nullptr;
}

FGSTN_CombatTimelineData FGSTN_SpinTimelines::Make(const FGSTN_SpinTimelineParams& Params, EGSTN_SpinSense Sense, bool bAirSpin)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = FMath::Max(1, Params.TotalFrames);
	Data.FirstActionableFrame = FMath::Clamp(Params.FirstActionableFrame, 0, Data.TotalFrames);
	Data.OverrideFrames = 0;

	FGSTN_SpinTrack Spin;
	Spin.StartFrame = 0;
	Spin.EndFrame = Data.TotalFrames;
	Spin.Sense = Sense;
	Spin.bAirSpin = bAirSpin;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_SpinTrack>(Spin));

	FGSTN_HitboxSpec Ring;
	Ring.Shape.Type = EGSTN_ShapeType::Sphere;
	Ring.Shape.Extent = FVector(Params.RingRadius);
	Ring.Space = EGSTN_HitboxSpace::Root;
	FGSTN_HitboxKey Key;
	Key.Frame = 0;
	Key.Offset = FVector(0.0, 0.0, Params.RingHeight);
	Ring.Keys = { Key };
	Ring.Data.Damage = Params.Damage;
	Ring.Data.BaseKnockback = Params.BaseKnockback;
	Ring.Data.AngleDeg = Params.KnockbackAngleDeg;

	FGSTN_TimelineTrack_Hitbox Hitbox;
	Hitbox.StartFrame = FMath::Clamp(Params.RingStartFrame, 0, Data.TotalFrames - 1);
	Hitbox.EndFrame = FMath::Clamp(Params.RingEndFrame, Hitbox.StartFrame + 1, Data.TotalFrames);
	Hitbox.Part.GroupName = Params.RingGroup;
	Hitbox.Hitboxes.Add(Ring);
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));

	// The native tag, not a request by name: a class default is built when the module loads, possibly before the tag manager has its tree
	FGSTN_TimelineTrack_CancelWindow Jump;
	Jump.StartFrame = 0;
	Jump.EndFrame = Data.TotalFrames;
	Jump.Condition = EGSTN_CancelCondition::Always;
	Jump.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Movement_Jump.GetTag()));
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Jump));

	// D5: the opposite rotation, from CancelFromFrame on, starts the cancel (its binding requires the spin's sense tag)
	if (Params.CancelFromFrame >= 0 && Params.CancelFromFrame < Data.TotalFrames)
	{
		FGSTN_TimelineTrack_CancelWindow IntoOpposite;
		IntoOpposite.StartFrame = Params.CancelFromFrame;
		IntoOpposite.EndFrame = Data.TotalFrames;
		IntoOpposite.Condition = EGSTN_CancelCondition::Always;
		IntoOpposite.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(SpinCommand(Opposite(Sense))));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(IntoOpposite));
	}
	// D8: crouch pressed during the spin, from DiveFromFrame on, starts the dive (its binding requires Combat.State.Spinning)
	if (Params.DiveFromFrame >= 0 && Params.DiveFromFrame < Data.TotalFrames)
	{
		FGSTN_TimelineTrack_CancelWindow IntoDive;
		IntoDive.StartFrame = Params.DiveFromFrame;
		IntoDive.EndFrame = Data.TotalFrames;
		IntoDive.Condition = EGSTN_CancelCondition::Always;
		IntoDive.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNSpinDrillTags::Command_Movement_Drill.GetTag()));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(IntoDive));
	}
	return Data;
}

FGSTN_CombatTimelineData FGSTN_SpinTimelines::MakeDive(const FGSTN_DiveTimelineParams& Params)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = FMath::Max(2, Params.MaxFrames);
	Data.FirstActionableFrame = Data.TotalFrames;
	Data.OverrideFrames = 0;

	FGSTN_DiveTrack Dive;
	Dive.StartFrame = 0;
	Dive.EndFrame = Data.TotalFrames;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_DiveTrack>(Dive));

	FGSTN_HitboxSpec Tip;
	Tip.Shape.Type = EGSTN_ShapeType::Sphere;
	Tip.Shape.Extent = FVector(Params.TipRadius);
	Tip.Space = EGSTN_HitboxSpace::Root;
	Tip.bSweepBetweenFrames = true;
	FGSTN_HitboxKey Key;
	Key.Frame = 0;
	Key.Offset = FVector(0.0, 0.0, -Params.TipDepth);
	Tip.Keys = { Key };
	Tip.Data.HitType = GSTNSpinDrillTags::HitType_Drill.GetTag();
	Tip.Data.Damage = Params.Damage;
	Tip.Data.BaseKnockback = Params.BaseKnockback;
	Tip.Data.AngleDeg = Params.KnockbackAngleDeg;
	// D9: the tip asks for the drill latch. GSTNLink's request step decides on the target side (UGSTN_LinkRule_Drill); a request does no damage,
	// and a refusal does not land, which the drill reads as the bounce of 7.6
	Tip.Data.LinkType = GSTNSpinDrillTags::Link_Type_Drill.GetTag();

	FGSTN_TimelineTrack_Hitbox Hitbox;
	Hitbox.StartFrame = 0;
	Hitbox.EndFrame = Data.TotalFrames;
	Hitbox.Part.GroupName = Params.TipGroup;
	Hitbox.Hitboxes.Add(Tip);
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));
	return Data;
}

FGSTN_CombatTimelineData FGSTN_SpinTimelines::MakeBore(const FGSTN_BoreTimelineParams& Params)
{
	FGSTN_CombatTimelineData Data;
	const int32 BoreFrame = 1;
	Data.TotalFrames = BoreFrame + 1 + FMath::Max(1, Params.RecoveryFrames);
	Data.FirstActionableFrame = Data.TotalFrames;
	Data.OverrideFrames = 0;

	FGSTN_BoreTrack Bore;
	Bore.StartFrame = 0;
	Bore.EndFrame = BoreFrame + 1;
	Bore.BoreFrame = BoreFrame;
	// IO-2: the drill's button (the dive holds the button that started it), read by the hook from the active layout
	Bore.HoldButton = GSTNCombatButtonTags::Button_Crouch.GetTag();
	Bore.LayoutFunction = TEXT("Spin.Drill");
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_BoreTrack>(Bore));

	FGSTN_HitboxSpec Tip;
	Tip.Shape.Type = EGSTN_ShapeType::Sphere;
	Tip.Shape.Extent = FVector(Params.TipRadius);
	Tip.Space = EGSTN_HitboxSpace::Root;
	FGSTN_HitboxKey Key;
	Key.Frame = 0;
	Key.Offset = FVector(0.0, 0.0, -Params.TipDepth);
	Tip.Keys = { Key };
	Tip.Data.HitType = GSTNSpinDrillTags::HitType_Drill.GetTag();
	Tip.Data.Damage = Params.Damage;
	Tip.Data.BaseKnockback = Params.BaseKnockback;
	Tip.Data.AngleDeg = Params.KnockbackAngleDeg;
	// No hitstun: the 6-frame re-hit would renew the MinHitstunFrames floor for the whole bore and pin the target in place
	Tip.Data.HitstunMultiplier = 0.f;
	// No hitstop either: the re-hit every few frames would freeze the victim (and the walking target of a moving latch) for most of the bore
	Tip.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Tip.Data.HitstopFramesOverride = 0;
	Tip.Data.AttackerHitstopFramesOverride = 0;
	// Seam S1: the held frame re-hits the latched target every RehitIntervalFrames (it counts stalled frames). No link type: the latch holds
	Tip.RehitIntervalFrames = FMath::Max(1, Params.RehitIntervalFrames);

	FGSTN_TimelineTrack_Hitbox Hitbox;
	Hitbox.StartFrame = BoreFrame;
	Hitbox.EndFrame = BoreFrame + 1;
	Hitbox.Part.GroupName = Params.TipGroup;
	Hitbox.Hitboxes.Add(Tip);
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));
	return Data;
}

FGSTN_CombatTimelineData FGSTN_SpinTimelines::MakeCancel(const FGSTN_SpinCancelTimelineParams& Params, EGSTN_SpinSense Sense)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = FMath::Max(1, Params.RecoveryFrames);
	Data.FirstActionableFrame = Data.TotalFrames;
	Data.OverrideFrames = 0;

	FGSTN_SpinCancelTrack Cancel;
	Cancel.StartFrame = 0;
	Cancel.EndFrame = Data.TotalFrames;
	Cancel.Sense = Sense;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_SpinCancelTrack>(Cancel));

	FGSTN_TimelineTrack_CancelWindow Jump;
	Jump.StartFrame = 0;
	Jump.EndFrame = Data.TotalFrames;
	Jump.Condition = EGSTN_CancelCondition::Always;
	Jump.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Movement_Jump.GetTag()));
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Jump));
	return Data;
}

FGameplayTag FGSTN_SpinTimelines::SpinCommand(EGSTN_SpinSense Sense)
{
	return (Sense == EGSTN_SpinSense::Clockwise ? GSTNSpinDrillTags::Command_Movement_Spin_Clockwise : GSTNSpinDrillTags::Command_Movement_Spin_CounterClockwise).GetTag();
}

FGSTN_SpinTimelineParams FGSTN_SpinTimelines::GroundParams()
{
	return FGSTN_SpinTimelineParams();
}

FGSTN_SpinTimelineParams FGSTN_SpinTimelines::AirParams()
{
	FGSTN_SpinTimelineParams Params;
	Params.TotalFrames = 36;
	Params.FirstActionableFrame = 20;
	Params.RingEndFrame = 20;
	return Params;
}

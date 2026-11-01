// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_SpinDrillTracks.h"

#include "Data/GSTN_SpinTimelines.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Input/GSTN_CombatCommandTags.h"

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
	return Data;
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

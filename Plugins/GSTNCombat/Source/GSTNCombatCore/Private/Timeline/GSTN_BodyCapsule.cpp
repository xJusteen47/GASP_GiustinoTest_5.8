// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_BodyCapsule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_BodyCapsule)

bool FGSTN_BodyCapsuleShape::NearlyEquals(const FGSTN_BodyCapsuleShape& Other) const
{
	constexpr float Tolerance = 0.01f;
	return FMath::IsNearlyEqual(HalfHeight, Other.HalfHeight, Tolerance) && FMath::IsNearlyEqual(Radius, Other.Radius, Tolerance)
		&& Offset.Equals(Other.Offset, Tolerance);
}

const FGSTN_TimelineTrack_BodyCapsule* FGSTN_BodyCapsule::FindTrackAt(const FGSTN_CombatTimelineData& Data, int32 Frame)
{
	if (Frame == INDEX_NONE)
	{
		return nullptr;
	}
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Entry : Data.Tracks)
	{
		const FGSTN_TimelineTrack_BodyCapsule* Track = Entry.GetPtr<FGSTN_TimelineTrack_BodyCapsule>();
		if (Track && Track->Contains(Frame))
		{
			return Track;
		}
	}
	return nullptr;
}

FGSTN_BodyCapsuleTarget FGSTN_BodyCapsule::ResolveTarget(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Standing)
{
	FGSTN_BodyCapsuleTarget Target;
	Target.Shape = Standing;
	const FGSTN_TimelineTrack_BodyCapsule* Track = Data ? FindTrackAt(*Data, TimelineFrame) : nullptr;
	if (!Track)
	{
		return Target;
	}
	Target.Shape.Radius = Track->Radius > 0.f ? Track->Radius : Standing.Radius;
	// Unreal clamps the half height to the radius: the shape the capsule really takes, so the step's bottom keeping is exact
	Target.Shape.HalfHeight = FMath::Max(Track->HalfHeight, Target.Shape.Radius);
	Target.Shape.Offset = Track->Offset;
	Target.Anchor = Track->Anchor;
	Target.MaxShiftPerFrame = Track->Anchor == EGSTN_BodyCapsuleAnchor::Body ? Track->MaxShiftPerFrame : 0.f;
	return Target;
}

FGSTN_BodyCapsuleTarget FGSTN_BodyCapsule::ResolveTarget(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Standing, const FGSTN_BodyCapsuleShape* Idle)
{
	if (!Idle || (Data && FindTrackAt(*Data, TimelineFrame)))
	{
		return ResolveTarget(Data, TimelineFrame, Standing);
	}
	FGSTN_BodyCapsuleTarget Target;
	Target.Shape = *Idle;
	Target.Shape.HalfHeight = FMath::Max(Idle->HalfHeight, Idle->Radius);
	Target.Anchor = EGSTN_BodyCapsuleAnchor::Body;
	Target.MaxShiftPerFrame = 0.f;
	return Target;
}

FGSTN_BodyCapsuleStep FGSTN_BodyCapsule::Step(const FGSTN_BodyCapsuleShape& Applied, const FGSTN_BodyCapsuleTarget& Target)
{
	FGSTN_BodyCapsuleStep Out;
	Out.Applied = Target.Shape;
	if (Target.Anchor == EGSTN_BodyCapsuleAnchor::Capsule)
	{
		// Bottom kept: centre moves by the half height change along up. Mesh root = actor - offset follows on its own
		Out.ActorShiftLocal = FVector(0.0, 0.0, Target.Shape.HalfHeight - Applied.HalfHeight);
		return Out;
	}

	FVector Delta = Target.Shape.Offset - Applied.Offset;
	if (Target.MaxShiftPerFrame > 0.f && Delta.Size() > Target.MaxShiftPerFrame)
	{
		Delta = Delta.GetSafeNormal() * Target.MaxShiftPerFrame;
	}
	Out.Applied.Offset = Applied.Offset + Delta;
	// The mesh root (actor - offset) stays: the actor moves exactly as much as the offset
	Out.ActorShiftLocal = Delta;
	return Out;
}

FGSTN_BodyCapsuleTarget FGSTN_BodyCapsule::GroundedRestore(const FGSTN_BodyCapsuleTarget& Target, const FGSTN_BodyCapsuleShape& Standing, bool bGrounded)
{
	FGSTN_BodyCapsuleTarget Out = Target;
	if (bGrounded && Target.Shape.NearlyEquals(Standing))
	{
		Out.Anchor = EGSTN_BodyCapsuleAnchor::Capsule;
		Out.MaxShiftPerFrame = 0.f;
	}
	return Out;
}

#include "Hits/GSTN_HitReachUtils.h"

namespace
{
	FTransform KeyTransform(const FGSTN_HitboxKey& Key)
	{
		return FTransform(Key.Rotation.Quaternion(), Key.Offset, Key.Scale);
	}

	/** Per shape-local axis, how much of the reach stretch it takes: 1 + (R - 1) * |component of the reach axis on that local axis| */
	FVector ExtentFactor(const FQuat& ShapeToSpace, const FVector& Axis, double Scale)
	{
		const FVector Local = ShapeToSpace.UnrotateVector(Axis);
		return FVector(
			1.0 + (Scale - 1.0) * FMath::Abs(Local.X),
			1.0 + (Scale - 1.0) * FMath::Abs(Local.Y),
			1.0 + (Scale - 1.0) * FMath::Abs(Local.Z));
	}

	/** Point with its coordinate along Axis (a unit basis vector) multiplied by Scale */
	FVector ScaleAlong(const FVector& Point, const FVector& Axis, double Scale)
	{
		return Point + Axis * (FVector::DotProduct(Point, Axis) * (Scale - 1.0));
	}
}

FVector UGSTN_HitReachUtils::AxisVector(EGSTN_ReachAxis Axis)
{
	switch (Axis)
	{
	case EGSTN_ReachAxis::X: return FVector::XAxisVector;
	case EGSTN_ReachAxis::Y: return FVector::YAxisVector;
	case EGSTN_ReachAxis::Z: return FVector::ZAxisVector;
	default: return FVector::ZeroVector;
	}
}

bool UGSTN_HitReachUtils::IsReachScalable(const FGSTN_HitboxSpec& Spec)
{
	return Spec.ReachAxis != EGSTN_ReachAxis::None && Spec.Space != EGSTN_HitboxSpace::Projectile;
}

FVector UGSTN_HitReachUtils::GetCentreInSpace(const FGSTN_HitboxSpec& Spec, int32 KeyIndex)
{
	const FVector LocalCentre = Spec.Shape.Local.GetTranslation();
	if (!Spec.Keys.IsValidIndex(KeyIndex))
	{
		return LocalCentre;
	}
	// The carrier is Shape.Local * Key * Space (HitShapeUtils), so the centre in space is the key applied to the local translation
	return KeyTransform(Spec.Keys[KeyIndex]).TransformPosition(LocalCentre);
}

FGSTN_HitboxSpec UGSTN_HitReachUtils::ScaleHitboxReach(const FGSTN_HitboxSpec& Spec, float ReachScale)
{
	FGSTN_HitboxSpec Result = Spec;
	const double Scale = FMath::Clamp(static_cast<double>(ReachScale), static_cast<double>(MinReachScale), static_cast<double>(MaxReachScale));
	if (!IsReachScalable(Spec) || Scale == 1.0)
	{
		return Result;
	}
	const FVector Axis = AxisVector(Spec.ReachAxis);
	const FQuat LocalRotation = Spec.Shape.Local.GetRotation();

	if (Result.Keys.IsEmpty())
	{
		// No keys: the Local placement is already in the hitbox's space
		Result.Shape.Local.SetTranslation(ScaleAlong(Spec.Shape.Local.GetTranslation(), Axis, Scale));
		if (Spec.bScaleExtentWithReach)
		{
			Result.Shape.Extent = Spec.Shape.Extent * ExtentFactor(LocalRotation, Axis, Scale);
		}
		return Result;
	}

	// Keys: each key may turn the shape, so the stretch goes into each key's scale (the composed scale multiplies per shape-local
	// axis, the same index as the extent), then the offset is moved so the centre lands where the reach rule puts it
	for (int32 KeyIndex = 0; KeyIndex < Result.Keys.Num(); ++KeyIndex)
	{
		FGSTN_HitboxKey& Key = Result.Keys[KeyIndex];
		const FVector Target = ScaleAlong(GetCentreInSpace(Spec, KeyIndex), Axis, Scale);
		if (Spec.bScaleExtentWithReach)
		{
			const FQuat ShapeToSpace = Key.Rotation.Quaternion() * LocalRotation;
			Key.Scale = Key.Scale * ExtentFactor(ShapeToSpace, Axis, Scale);
		}
		const FVector Centre = KeyTransform(Key).TransformPosition(Spec.Shape.Local.GetTranslation());
		Key.Offset += Target - Centre;
	}
	return Result;
}

int32 UGSTN_HitReachUtils::ApplyReachScale(FGSTN_CombatTimelineData& Data, float ReachScale)
{
	int32 Scaled = 0;
	for (TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		FGSTN_TimelineTrack_Hitbox* Track = Instance.GetMutablePtr<FGSTN_TimelineTrack_Hitbox>();
		if (!Track)
		{
			continue;
		}
		for (FGSTN_HitboxSpec& Spec : Track->Hitboxes)
		{
			if (IsReachScalable(Spec) && ReachScale != 1.f)
			{
				Spec = ScaleHitboxReach(Spec, ReachScale);
				++Scaled;
			}
		}
	}
	return Scaled;
}

FGSTN_CombatTimelineData UGSTN_HitReachUtils::ScaleTimelineReach(const FGSTN_CombatTimelineData& Data, float ReachScale, int32& OutScaledHitboxes)
{
	FGSTN_CombatTimelineData Result = Data;
	OutScaledHitboxes = ApplyReachScale(Result, ReachScale);
	return Result;
}

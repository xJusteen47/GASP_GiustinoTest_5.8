// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Placement/GSTN_Placer_Ring.h"

FVector UGSTN_Placer_Ring::RingDirection(const FGSTN_ExitQuery& Query, int32 Index)
{
	const FVector Up = Query.Up.GetSafeNormal();
	FVector Base = Query.PreferredDirection;
	if (Base.IsNearlyZero())
	{
		Base = Query.PartnerTransform.GetUnitAxis(EAxis::X);
	}
	Base = FVector::VectorPlaneProject(Base, Up);
	if (!Base.Normalize())
	{
		// The preferred direction runs along the up: any fixed direction in the plane will do, as long as it is the same everywhere
		Base = FVector::VectorPlaneProject(FVector::XAxisVector, Up);
		if (!Base.Normalize())
		{
			Base = FVector::VectorPlaneProject(FVector::YAxisVector, Up).GetSafeNormal();
		}
	}
	return FQuat(Up, FMath::DegreesToRadians(45.0 * Index)).RotateVector(Base);
}

FGSTN_ExitResult UGSTN_Placer_Ring::Place_Implementation(const FGSTN_ExitQuery& Query) const
{
	const FVector Up = Query.Up.GetSafeNormal();
	const FVector PartnerLocation = Query.PartnerTransform.GetLocation();

	for (int32 Index = 0; Index < DirectionCount; ++Index)
	{
		const FVector Direction = RingDirection(Query, Index);
		const FVector Location = PartnerLocation + Direction * RadiusCm + Up * HeightOffsetCm;
		// The entity lands facing the entity it left
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(-Direction, Up).ToQuat();
		if (IsFree(Query, Location, Rotation))
		{
			FGSTN_ExitResult Result;
			Result.Transform = FTransform(Rotation, Location);
			Result.Velocity = Direction * OutwardSpeedCm;
			Result.bValid = true;
			Result.CandidateIndex = Index;
			return Result;
		}
	}

	// Straight above the partner, a capsule height clear of it
	const FVector AboveLocation = PartnerLocation + Up * (Query.CapsuleHalfHeight * 2.0 + AboveClearanceCm);
	const FQuat AboveRotation = Query.PartnerTransform.GetRotation();
	if (IsFree(Query, AboveLocation, AboveRotation))
	{
		FGSTN_ExitResult Result;
		Result.Transform = FTransform(AboveRotation, AboveLocation);
		Result.bValid = true;
		Result.CandidateIndex = AboveIndex;
		return Result;
	}

	return MakeFallback(Query, FallbackLiftCm);
}

// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Placement/GSTN_LinkExitPlacer.h"

#include "Placement/GSTN_ExitCollisionQuery.h"

FGSTN_ExitResult UGSTN_LinkExitPlacer::Place_Implementation(const FGSTN_ExitQuery& Query) const
{
	return MakeFallback(Query, Query.CapsuleHalfHeight * 2.0);
}

bool UGSTN_LinkExitPlacer::IsFree(const FGSTN_ExitQuery& Query, const FVector& Location, const FQuat& Rotation)
{
	return Query.Collision == nullptr || Query.Collision->IsCapsuleFree(Location, Rotation, Query.CapsuleRadius, Query.CapsuleHalfHeight);
}

FGSTN_ExitResult UGSTN_LinkExitPlacer::MakeFallback(const FGSTN_ExitQuery& Query, double Lift)
{
	FGSTN_ExitResult Result;
	Result.Transform = Query.PartnerTransform;
	Result.Transform.AddToTranslation(Query.Up.GetSafeNormal() * Lift);
	Result.Velocity = FVector::ZeroVector;
	Result.bValid = false;
	Result.CandidateIndex = INDEX_NONE;
	return Result;
}

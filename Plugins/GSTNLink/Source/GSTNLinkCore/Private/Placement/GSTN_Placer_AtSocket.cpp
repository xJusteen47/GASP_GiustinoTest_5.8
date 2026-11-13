// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Placement/GSTN_Placer_AtSocket.h"

FGSTN_ExitResult UGSTN_Placer_AtSocket::Place_Implementation(const FGSTN_ExitQuery& Query) const
{
	const FVector Up = Query.Up.GetSafeNormal();
	const FQuat Rotation = Query.SocketTransform.GetRotation();

	for (int32 Nudge = 0; Nudge <= MaxNudges; ++Nudge)
	{
		const FVector Location = Query.SocketTransform.GetLocation() + Up * (NudgeCm * Nudge);
		if (IsFree(Query, Location, Rotation))
		{
			FGSTN_ExitResult Result;
			Result.Transform = FTransform(Rotation, Location);
			Result.Velocity = Query.KnockbackVelocity;
			Result.bValid = true;
			Result.CandidateIndex = Nudge;
			return Result;
		}
	}

	FGSTN_ExitResult Result = MakeFallback(Query, FallbackLiftCm);
	Result.Velocity = Query.KnockbackVelocity;
	return Result;
}

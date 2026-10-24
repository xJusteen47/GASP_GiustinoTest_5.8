// Pure combat logic (no GAS, no Network Prediction).

#include "Jump/GSTN_GravityPhases.h"

FVector FGSTN_GravityPhases::Resolve(FGSTN_GravityBase& Tracker, const FVector& CurrentGravity, float Scale, bool& OutWrite)
{
	// Not what this wrote last: the pawn's own gravity (first call, or changed by something else), which is the new base
	if (!Tracker.bValid || !CurrentGravity.Equals(Tracker.LastWritten, 1e-3))
	{
		Tracker.Base = CurrentGravity;
		Tracker.bValid = true;
		Tracker.LastWritten = CurrentGravity;
	}

	const FVector Wanted = Tracker.Base * static_cast<double>(Scale);
	OutWrite = !FMath::IsNearlyEqual(Scale, 1.f) || !CurrentGravity.Equals(Tracker.Base, 1e-3);
	if (OutWrite)
	{
		Tracker.LastWritten = Wanted;
	}
	return OutWrite ? Wanted : CurrentGravity;
}

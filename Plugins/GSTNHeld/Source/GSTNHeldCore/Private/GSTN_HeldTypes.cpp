// Held objects, pure core (no GAS, no Network Prediction).

#include "GSTN_HeldTypes.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNHeld);

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNHeldCore);

FGSTN_HeldObjectTuning::FGSTN_HeldObjectTuning()
{
	// TEST VALUES: a light prop thrown at 15 m/s under earth gravity, about two seconds of flight, one hit
	Throw.Speed = 1500.f;
	Throw.GravityAccel = FVector(0.0, 0.0, -980.0);
	Throw.LifetimeFrames = 120;
	Throw.Shape.Type = EGSTN_ShapeType::Sphere;
	Throw.Shape.Extent = FVector(20.0);
	Throw.Hit.Damage = 10.f;
	Throw.Hit.BaseKnockback = 40.f;
	Throw.Hit.AngleDeg = 30.f;
	Throw.MaxHits = 1;
}

void FGSTN_HeldRules::ResolvePickups(TConstArrayView<FGSTN_PickupRequest> Requests, TConstArrayView<FGSTN_PickupCandidate> Candidates,
	TArray<FGSTN_PickupAssignment>& OutAssignments)
{
	OutAssignments.Reset();
	TArray<FGSTN_PickupRequest> Order(Requests.GetData(), Requests.Num());
	Order.StableSort([](const FGSTN_PickupRequest& A, const FGSTN_PickupRequest& B)
	{
		return A.Frame != B.Frame ? A.Frame < B.Frame : A.HolderId < B.HolderId;
	});

	TArray<bool> Taken;
	Taken.Init(false, Candidates.Num());
	TSet<int32> Served;
	for (const FGSTN_PickupRequest& Request : Order)
	{
		if (Served.Contains(Request.HolderId))
		{
			continue;
		}
		Served.Add(Request.HolderId);

		int32 Best = INDEX_NONE;
		double BestDistSq = 0.0;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FGSTN_PickupCandidate& Candidate = Candidates[Index];
			if (Taken[Index] || !Candidate.bAvailable)
			{
				continue;
			}
			const double DistSq = FVector::DistSquared(Request.Location, Candidate.Location);
			if (DistSq > FMath::Square(static_cast<double>(Candidate.Radius)))
			{
				continue;
			}
			if (Best == INDEX_NONE || DistSq < BestDistSq || (DistSq == BestDistSq && Candidate.ObjectId < Candidates[Best].ObjectId))
			{
				Best = Index;
				BestDistSq = DistSq;
			}
		}
		if (Best != INDEX_NONE)
		{
			Taken[Best] = true;
			OutAssignments.Add({ Request.HolderId, Candidates[Best].ObjectId });
		}
	}
}

FTransform FGSTN_HeldRules::HoldTransform(const FTransform& Holder, const FVector& HoldOffset)
{
	return FTransform(Holder.GetRotation(), Holder.TransformPosition(HoldOffset));
}

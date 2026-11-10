// Held objects, pure core (no GAS, no Network Prediction).

#include "GSTN_HeldTypes.h"

#include "GSTN_HeldTags.h"

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
	SizeTag = GSTNHeldTags::Held_Size_Light;
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

		auto InReach = [&Request](const FGSTN_PickupCandidate& Candidate, double& OutDistSq)
		{
			OutDistSq = FVector::DistSquared(Request.Location, Candidate.Location);
			return OutDistSq <= FMath::Square(static_cast<double>(Candidate.Radius));
		};
		// M10 G6: an object of a class that needs capacity is out of this holder's reach when it is too heavy for it
		auto CanLift = [&Request](const FGSTN_PickupCandidate& Candidate)
		{
			return !Candidate.bRequiresCapacity || Candidate.WeightKg <= Request.CarryCapacityKg;
		};
		int32 Best = INDEX_NONE;
		double BestDistSq = 0.0;
		// The focused object first (M10 slice 5), when it is still free and in reach
		if (Request.TargetObjectId != INDEX_NONE)
		{
			for (int32 Index = 0; Index < Candidates.Num() && Best == INDEX_NONE; ++Index)
			{
				const FGSTN_PickupCandidate& Candidate = Candidates[Index];
				double DistSq = 0.0;
				if (Candidate.ObjectId == Request.TargetObjectId && !Taken[Index] && Candidate.bAvailable && CanLift(Candidate) && InReach(Candidate, DistSq))
				{
					Best = Index;
				}
			}
		}
		// No focus, or the focused object is not free and the settings allow it: the nearest free object in reach (equal distance: lower object id)
		if (Best == INDEX_NONE && (Request.TargetObjectId == INDEX_NONE || Request.bFallBackToNearest))
		{
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				const FGSTN_PickupCandidate& Candidate = Candidates[Index];
				double DistSq = 0.0;
				if (Taken[Index] || !Candidate.bAvailable || !CanLift(Candidate) || !InReach(Candidate, DistSq))
				{
					continue;
				}
				if (Best == INDEX_NONE || DistSq < BestDistSq || (DistSq == BestDistSq && Candidate.ObjectId < Candidates[Best].ObjectId))
				{
					Best = Index;
					BestDistSq = DistSq;
				}
			}
		}
		if (Best != INDEX_NONE)
		{
			Taken[Best] = true;
			OutAssignments.Add({ Request.HolderId, Candidates[Best].ObjectId });
		}
	}
}

int32 FGSTN_HeldRules::FindPickupTarget(const FVector& From, TConstArrayView<FGSTN_PickupCandidate> Candidates)
{
	const FGSTN_PickupRequest Request{ 0, 0, From };
	TArray<FGSTN_PickupAssignment> Assignments;
	ResolvePickups(MakeArrayView(&Request, 1), Candidates, Assignments);
	return Assignments.IsEmpty() ? INDEX_NONE : Assignments[0].ObjectId;
}

FTransform FGSTN_HeldRules::HoldTransform(const FTransform& Holder, const FVector& HoldOffset)
{
	return FTransform(Holder.GetRotation(), Holder.TransformPosition(HoldOffset));
}

bool FGSTN_HeldRules::BreaksOnState(const FGameplayTagContainer& HolderTags, const FGSTN_HeldBreakRules& Rules)
{
	return !Rules.DropWhenHolderHasAny.IsEmpty() && HolderTags.HasAny(Rules.DropWhenHolderHasAny);
}

bool FGSTN_HeldRules::BreaksOnEvent(const FGameplayTag& EventType, const FGSTN_HeldBreakRules& Rules)
{
	return EventType.IsValid() && EventType.MatchesAny(Rules.DropOnHolderEvents);
}

EGSTN_HeldThrowAim FGSTN_HeldRules::ChooseThrowAim(int32 HeldFrames, bool bPrecisionAim, const FGSTN_HeldThrowInput& Input)
{
	if (IsAiming(HeldFrames, Input) || (bPrecisionAim && Input.bCameraAimWhenReticleOn))
	{
		return EGSTN_HeldThrowAim::Camera;
	}
	return EGSTN_HeldThrowAim::Stick;
}

FVector FGSTN_HeldRules::DropPoint(const FTransform& Holder, const FGSTN_HeldBreakRules& Rules)
{
	return Holder.GetLocation() + Holder.GetUnitAxis(EAxis::X) * Rules.DropForwardCm;
}

uint8 FGSTN_HeldLoad::EncodePayload(int32 SizeIndex, float LoadRatio)
{
	const int32 Size = FMath::Clamp(SizeIndex, 0, MaxSizeClasses - 1);
	const int32 Ratio = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(LoadRatio, 0.f, 1.f) * RatioSteps), 0, RatioSteps);
	return static_cast<uint8>((Size << 6) | Ratio);
}

void FGSTN_HeldLoad::DecodePayload(uint8 Payload, int32& OutSizeIndex, float& OutLoadRatio)
{
	OutSizeIndex = Payload >> 6;
	OutLoadRatio = static_cast<float>(Payload & 0x3F) / RatioSteps;
}

float FGSTN_HeldLoad::LoadRatio(float WeightKg, float CapacityKg, float DefaultCapacityKg)
{
	const float Capacity = EffectiveCapacity(CapacityKg, DefaultCapacityKg);
	if (Capacity <= 0.f)
	{
		return WeightKg > 0.f ? 1.f : 0.f;
	}
	return FMath::Clamp(WeightKg / Capacity, 0.f, 1.f);
}

bool FGSTN_HeldLoad::CanLift(const FGSTN_HeldSizeClass* SizeClass, float WeightKg, float EffectiveCapacityKg)
{
	return !SizeClass || !SizeClass->bRequiresCapacity || WeightKg <= EffectiveCapacityKg;
}

float FGSTN_HeldLoad::SpeedScale(const FGSTN_HeldSizeClass* SizeClass, float LoadRatio)
{
	return SizeClass ? FMath::Clamp(1.f - FMath::Clamp(LoadRatio, 0.f, 1.f) * SizeClass->SpeedLossAtFullLoad, 0.f, 1.f) : 1.f;
}

float FGSTN_HeldLoad::JumpScale(const FGSTN_HeldSizeClass* SizeClass, float LoadRatio)
{
	return SizeClass ? FMath::Clamp(1.f - FMath::Clamp(LoadRatio, 0.f, 1.f) * SizeClass->JumpLossAtFullLoad, 0.f, 1.f) : 1.f;
}

EGSTN_HeldImpactBehaviour FGSTN_HeldRules::ResolveImpact(const FGSTN_HeldObjectTuning& Tuning, bool bWorld, const FVector& Velocity, const FVector& Normal,
	int32 BouncesSoFar, FVector& OutVelocity)
{
	OutVelocity = FVector::ZeroVector;
	switch (Tuning.ImpactBehaviour)
	{
	case EGSTN_HeldImpactBehaviour::Break:
		return EGSTN_HeldImpactBehaviour::Break;
	case EGSTN_HeldImpactBehaviour::Stick:
		return bWorld ? EGSTN_HeldImpactBehaviour::Stick : EGSTN_HeldImpactBehaviour::Land;
	case EGSTN_HeldImpactBehaviour::Bounce:
	{
		const FVector N = Normal.GetSafeNormal();
		if (!bWorld || N.IsNearlyZero() || BouncesSoFar >= Tuning.MaxBounces)
		{
			return EGSTN_HeldImpactBehaviour::Land;
		}
		const FVector Rebound = (Velocity - 2.0 * FVector::DotProduct(Velocity, N) * N) * Tuning.BounceRestitution;
		if (Rebound.Size() < Tuning.MinBounceSpeed || FVector::DotProduct(Rebound, N) <= 0.0)
		{
			return EGSTN_HeldImpactBehaviour::Land;
		}
		OutVelocity = Rebound;
		return EGSTN_HeldImpactBehaviour::Bounce;
	}
	default:
		return EGSTN_HeldImpactBehaviour::Land;
	}
}

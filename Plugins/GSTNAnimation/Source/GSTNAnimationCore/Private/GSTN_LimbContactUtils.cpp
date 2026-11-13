// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_LimbContactUtils.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_LimbContactUtils)

namespace GSTNLimbContact
{
	/** Under this length a direction is treated as unknown */
	constexpr float MinDirectionLength = 1.e-4f;
}

FGSTN_LimbContactResult UGSTN_LimbContactUtils::ComputeContact(const FVector& Effector, const FVector& ChainRoot, float ChainLength, const FGSTN_LimbSurface& Surface, const FGSTN_LimbContactSettings& Settings)
{
	FGSTN_LimbContactResult Result;
	Result.TargetLocation = Effector;

	const FVector Normal = Surface.Normal.GetSafeNormal();
	if (!Surface.bValid || Normal.IsNearlyZero())
	{
		return Result;
	}

	const float EffectorDistance = FVector::DotProduct(Effector - Surface.Point, Normal);
	Result.Gap = EffectorDistance - Settings.ContactOffset;
	Result.TargetLocation = Effector - Normal * Result.Gap;

	if (Settings.bRequireReach)
	{
		const float RootDistance = FVector::DotProduct(ChainRoot - Surface.Point, Normal);
		if (RootDistance <= EffectorDistance)
		{
			return Result;
		}
	}

	if (ChainLength > 0.f && FVector::Dist(ChainRoot, Result.TargetLocation) > ChainLength * Settings.MaxReachRatio)
	{
		return Result;
	}

	// A limb already inside the surface is always pushed out onto it, whatever the distances
	const float Attach = FMath::Max(Settings.AttachDistance, 0.f);
	const float Release = FMath::Max(Settings.ReleaseDistance, Attach);
	if (Result.Gap <= Attach)
	{
		Result.TargetAlpha = 1.f;
	}
	else if (Result.Gap < Release)
	{
		const float T = (Result.Gap - Attach) / (Release - Attach);
		Result.TargetAlpha = 1.f - FMath::SmoothStep(0.f, 1.f, T);
	}

	return Result;
}

float UGSTN_LimbContactUtils::StepContactAlpha(float Current, float Target, const FGSTN_LimbContactSettings& Settings, float DeltaSeconds)
{
	const float ClampedTarget = FMath::Clamp(Target, 0.f, 1.f);
	const float Speed = ClampedTarget >= Current ? Settings.BlendInSpeed : Settings.BlendOutSpeed;
	if (Speed <= 0.f)
	{
		return ClampedTarget;
	}

	const float MaxStep = Speed * FMath::Max(DeltaSeconds, 0.f);
	return FMath::Clamp(Current + FMath::Clamp(ClampedTarget - Current, -MaxStep, MaxStep), 0.f, 1.f);
}

FQuat UGSTN_LimbContactUtils::AlignAxisToSurface(const FQuat& Rotation, const FVector& LocalAxis, const FVector& SurfaceNormal, float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (ClampedAlpha <= 0.f || LocalAxis.Size() < GSTNLimbContact::MinDirectionLength || SurfaceNormal.Size() < GSTNLimbContact::MinDirectionLength)
	{
		return Rotation;
	}

	const FVector CurrentAxis = Rotation.RotateVector(LocalAxis.GetSafeNormal());
	const FQuat Delta = FQuat::FindBetweenNormals(CurrentAxis, -SurfaceNormal.GetSafeNormal());
	const FQuat Aligned = Delta * Rotation;
	return FQuat::Slerp(Rotation, Aligned, ClampedAlpha).GetNormalized();
}

FGSTN_LimbSurface UGSTN_LimbContactUtils::SurfaceFromTransform(const FTransform& Transform)
{
	FGSTN_LimbSurface Surface;
	Surface.bValid = true;
	Surface.Point = Transform.GetLocation();
	Surface.Normal = Transform.GetUnitAxis(EAxis::X);
	return Surface;
}

FGSTN_LimbSurface UGSTN_LimbContactUtils::InverseTransformSurface(const FGSTN_LimbSurface& Surface, const FTransform& SpaceTransform)
{
	FGSTN_LimbSurface Local = Surface;
	Local.Point = SpaceTransform.InverseTransformPosition(Surface.Point);
	Local.Normal = SpaceTransform.InverseTransformVectorNoScale(Surface.Normal).GetSafeNormal();
	return Local;
}

FGSTN_LimbSurface UGSTN_LimbContactUtils::ProbeSurface(const UWorld* World, const FVector& Origin, const FVector& ExpectedNormal, const FGSTN_LimbSurface& Fallback,
	const FGSTN_LimbProbeSettings& Settings, const FCollisionQueryParams& QueryParams)
{
	const FVector Normal = ExpectedNormal.GetSafeNormal();
	if (!World || Normal.IsNearlyZero() || Settings.ProbeDistance <= 0.f)
	{
		return Fallback;
	}

	FHitResult Hit;
	const FVector End = Origin - Normal * Settings.ProbeDistance;
	const bool bHit = World->SweepSingleByChannel(Hit, Origin, End, FQuat::Identity, Settings.TraceChannel,
		FCollisionShape::MakeSphere(Settings.ProbeRadius), QueryParams);

	// A sweep that starts inside geometry has no usable normal
	if (!bHit || Hit.bStartPenetrating)
	{
		return Fallback;
	}

	const float MinDot = FMath::Cos(FMath::DegreesToRadians(Settings.MaxNormalAngle));
	if (FVector::DotProduct(Hit.ImpactNormal, Normal) < MinDot)
	{
		return Fallback;
	}

	FGSTN_LimbSurface Surface;
	Surface.bValid = true;
	Surface.Point = Hit.ImpactPoint;
	Surface.Normal = Hit.ImpactNormal;
	return Surface;
}

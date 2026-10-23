// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceUtils.h"
#include "WallMovement/GSTN_WallMovementDebug.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallQueryUtils)

namespace GSTNWallQuery
{
	/** Collision setup shared by every wall query: the updated primitive's channel and responses, ignoring its owner */
	struct FQueryContext
	{
		const UWorld* World = nullptr;
		const USceneComponent* UpdatedComponent = nullptr;
		const UPrimitiveComponent* UpdatedPrimitive = nullptr;
		ECollisionChannel Channel = ECC_Pawn;
		FCollisionQueryParams QueryParams;
		FCollisionResponseParams ResponseParams;

		/** Takes SCENE_QUERY_STAT(Name), which expands to the trace tag and the stat id */
		FQueryContext(const FMovingComponentSet& MovingComps, const FName TraceTag, const TStatId& StatId)
			: UpdatedComponent(MovingComps.UpdatedComponent.Get())
			, UpdatedPrimitive(MovingComps.UpdatedPrimitive.Get())
			, QueryParams(TraceTag, StatId, false, UpdatedComponent ? UpdatedComponent->GetOwner() : nullptr)
		{
			World = UpdatedComponent ? UpdatedComponent->GetWorld() : nullptr;
			if (UpdatedPrimitive)
			{
				Channel = UpdatedPrimitive->GetCollisionObjectType();
				UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParams);
			}
		}
	};

	/** Weighted blend of probe samples into a single wall normal and contact point */
	struct FProbeAccumulator
	{
		FVector NormalSum = FVector::ZeroVector;
		FVector PointSum = FVector::ZeroVector;
		float WeightSum = 0.f;
		float PointWeightSum = 0.f;

		void AddSample(const FVector& Normal, const FVector& Point, float Weight, bool bContributesToContact)
		{
			NormalSum += Normal * Weight;
			WeightSum += Weight;
			if (bContributesToContact)
			{
				PointSum += Point * Weight;
				PointWeightSum += Weight;
			}
		}

		bool HasContact() const { return PointWeightSum > 0.f; }
	};

	/**
	 * Along-wall probes (spatial filter): sphere sweeps toward the wall from the capsule center, top and bottom.
	 * A sphere's Hit.Normal points from the contact to the sphere center, so on edges it is "rounded" and changes
	 * continuously, unlike a line trace's face normal. The strongest sample names the wall component in OutResult
	 */
	static void AccumulateAlongWallSamples(const FQueryContext& Ctx, const FVector& Center, const FVector& Up, const FVector& WallNormal, const FVector& GravityUp,
		float Radius, float HalfHeight, const FCollisionShape& ProbeShape, const FGSTN_WallCheckSettings& Settings, FProbeAccumulator& Accumulator, FGSTN_WallProbeResult& OutResult)
	{
		const float ProbeRadius = ProbeShape.GetSphereRadius();
		const float CosMaxDeviation = FMath::Cos(FMath::DegreesToRadians(Settings.MaxProbeDeviationAngle));
		const float ProbeReach = (Radius - ProbeRadius) + Settings.WallGap + Settings.WallProbeDistance;
		const float VerticalOffset = FMath::Max(HalfHeight - Radius, 0.f) * Settings.ProbeVerticalSpread;

		struct FProbeOrigin { FVector Offset; float Weight; };
		const FProbeOrigin Origins[] =
		{
			{ FVector::ZeroVector,     1.0f },
			{ Up * VerticalOffset,     0.5f },
			{ -Up * VerticalOffset,    0.5f },
		};

		float BestSampleWeight = 0.f;
		for (const FProbeOrigin& Origin : Origins)
		{
			const FVector Start = Center + Origin.Offset;
			const FVector End = Start - WallNormal * ProbeReach;

			FHitResult Hit;
			const bool bProbeHit = Ctx.World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Ctx.Channel, ProbeShape, Ctx.QueryParams, Ctx.ResponseParams);
			[[maybe_unused]] const int32 ProbeIdx = UE_PTRDIFF_TO_INT32(&Origin - Origins);
			GSTN_WALL_LOG(2, TEXT("   probe%d hit=%d pen=%d N=%s IN=%s P=%s t=%.2f actor=%s"),
				ProbeIdx, bProbeHit ? 1 : 0, Hit.bStartPenetrating ? 1 : 0, *GSTNWallDebug::Vec(Hit.Normal), *GSTNWallDebug::Vec(Hit.ImpactNormal), *GSTNWallDebug::Vec(Hit.ImpactPoint),
				Hit.Time, *GetNameSafe(Hit.GetActor()));
			if (!bProbeHit || Hit.bStartPenetrating)
			{
				continue;
			}

			const float Alignment = FVector::DotProduct(Hit.Normal, WallNormal);
			if (Alignment < CosMaxDeviation || !UGSTN_WallQueryUtils::IsHitOnValidWall(Ctx.World, Hit, GravityUp, Settings, Ctx.Channel, Ctx.QueryParams, Ctx.ResponseParams))
			{
				GSTN_WALL_LOG(2, TEXT("   probe%d rejected (align=%.2f)"), ProbeIdx, Alignment);
				continue;
			}

			// Closer hits and hits agreeing with the current wall dominate. Agreement must reach zero exactly at the rejection
			// threshold: the probe direction follows the filtered normal, so a sample with a sizeable weight right at the cutoff
			// appears/disappears as the normal moves, snapping it back and forth (a self-sustained oscillation, even at rest)
			const float Proximity = 1.f - 0.5f * Hit.Time;
			const float Agreement = FMath::Square(FMath::Clamp((Alignment - CosMaxDeviation) / FMath::Max(1.f - CosMaxDeviation, UE_KINDA_SMALL_NUMBER), 0.f, 1.f));
			const float SampleWeight = Origin.Weight * Proximity * Agreement;
			Accumulator.AddSample(Hit.Normal, Hit.ImpactPoint, SampleWeight, true);

			// The strongest along-wall sample names the wall component: the movement base candidate
			if (SampleWeight > BestSampleWeight)
			{
				BestSampleWeight = SampleWeight;
				OutResult.Component = Hit.GetComponent();
				OutResult.BoneName = Hit.BoneName;
			}

#if ENABLE_DRAW_DEBUG
			if (Settings.bDrawDebug && IsInGameThread())
			{
				DrawDebugLine(Ctx.World, Start, Hit.Location, FColor::Orange, false, -1.f, 0, 0.5f);
				DrawDebugPoint(Ctx.World, Hit.ImpactPoint, 6.f, FColor::Orange, false, -1.f);
			}
#endif
		}
	}

	/**
	 * Lookahead probe (concave corners ahead): sweeps along the movement direction. Its weight ramps up as the obstacle
	 * gets closer, so the capsule starts turning into an inside corner before touching it, instead of hitting it and snapping
	 */
	static void AccumulateLookaheadSample(const FMovingComponentSet& MovingComps, const FQueryContext& Ctx, const FVector& Center, const FQuat& Orientation,
		const FVector& WallNormal, const FVector& Velocity, const FVector& GravityUp, float Radius, float HalfHeight, const FCollisionShape& ProbeShape,
		const FGSTN_WallCheckSettings& Settings, bool bHasAlongWallContact, FProbeAccumulator& Accumulator)
	{
		const FVector MoveDir = FVector::VectorPlaneProject(Velocity, WallNormal).GetSafeNormal();
		if (MoveDir.IsNearlyZero())
		{
			return;
		}

		const FVector Up = Orientation.GetUpVector();
		const float ProbeRadius = ProbeShape.GetSphereRadius();
		const float CosMaxConcave = FMath::Cos(FMath::DegreesToRadians(Settings.MaxConcaveAngle));
		const float ExtentAlongMove = Radius + FMath::Max(HalfHeight - Radius, 0.f) * FMath::Abs(FVector::DotProduct(MoveDir, Up));
		const float LookaheadDistance = FMath::Max(Settings.MinLookaheadDistance, Velocity.Size() * Settings.LookaheadTime);
		const float ContactDistance = ExtentAlongMove - ProbeRadius; // probe travel at which the capsule would touch
		const FVector End = Center + MoveDir * (ContactDistance + LookaheadDistance);

		FHitResult Hit;
		if (Ctx.World->SweepSingleByChannel(Hit, Center, End, FQuat::Identity, Ctx.Channel, ProbeShape, Ctx.QueryParams, Ctx.ResponseParams)
			&& !Hit.bStartPenetrating
			&& FVector::DotProduct(Hit.Normal, MoveDir) < -0.1f
			&& FVector::DotProduct(Hit.Normal, WallNormal) >= CosMaxConcave
			&& UGSTN_WallQueryUtils::IsHitOnValidWall(Ctx.World, Hit, GravityUp, Settings, Ctx.Channel, Ctx.QueryParams, Ctx.ResponseParams)
			&& UGSTN_WallQueryUtils::CanFitOnWall(MovingComps, Orientation, Hit.Normal, Hit.ImpactPoint, Radius, HalfHeight, Settings.WallGap))
		{
			const float Closeness = 1.f - FMath::Clamp((Hit.Distance - ContactDistance) / LookaheadDistance, 0.f, 1.f);
			Accumulator.AddSample(Hit.Normal, Hit.ImpactPoint, 1.5f * FMath::Square(Closeness), !bHasAlongWallContact);

#if ENABLE_DRAW_DEBUG
			if (Settings.bDrawDebug && IsInGameThread())
			{
				DrawDebugLine(Ctx.World, Center, Hit.Location, FColor::Magenta, false, -1.f, 0, 0.5f);
			}
#endif
		}
	}
}

bool UGSTN_WallQueryUtils::IsValidWallNormal(const FVector& Normal, const FVector& GravityUp, float MinSurfaceAngle, float MaxSurfaceAngle)
{
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Normal, GravityUp), -1.f, 1.f)));
	return AngleDeg >= MinSurfaceAngle && AngleDeg <= MaxSurfaceAngle;
}

bool UGSTN_WallQueryUtils::PassesSurfaceFilter(const FHitResult& Hit, const FGSTN_WallSurfaceFilter& Filter)
{
	// Empty query: today's behaviour, and the surface is not resolved at all (no cost for modes that do not use the filter)
	if (Filter.IsEmpty())
	{
		return true;
	}

	// Static configuration only (components, settings, providers answering from replicated state): the same answer in a
	// re-simulated frame, so there is nothing to cache or to roll back
	const FGSTN_SurfaceView Surface = UGSTN_SurfaceUtils::Resolve(Hit);
	if (Surface.Source == EGSTN_SurfaceSource::Default)
	{
		return Filter.bUnknownSurfacesPass;
	}
	return Surface.MatchesBehaviours(Filter.RequiredBehaviours);
}

bool UGSTN_WallQueryUtils::IsHitOnValidWall(const UWorld* World, const FHitResult& Hit, const FVector& GravityUp, const FGSTN_WallCheckSettings& Settings,
	ECollisionChannel Channel, const FCollisionQueryParams& QueryParams, const FCollisionResponseParams& ResponseParams)
{
	if (!IsValidWallNormal(Hit.Normal, GravityUp, Settings.MinSurfaceAngle, Settings.MaxSurfaceAngle)
		|| !IsValidWallNormal(Hit.ImpactNormal, GravityUp, Settings.MinSurfaceAngle, Settings.MaxSurfaceAngle))
	{
		return false;
	}

	// Angle is fine: does the surface allow this kind of wall? Done before the edge trace below, which is the costly part.
	// An edge hit is judged by the component it touched, not by the neighbouring face the trace may find
	if (!PassesSurfaceFilter(Hit, Settings.SurfaceFilter))
	{
		return false;
	}

	// Face contact: the sweep normal is the face normal, nothing else to check
	constexpr float EdgeCosThreshold = 0.9994f; // ~2 degrees
	if (FVector::DotProduct(Hit.Normal, Hit.ImpactNormal) >= EdgeCosThreshold || !World)
	{
		return true;
	}

	// Edge contact: ImpactNormal is only one of the two faces. Step just past the edge, into the reported face, which lands
	// on the plane of the other face, and line trace it from outside along the rounded normal to read its real normal
	constexpr float StepPastEdge = 2.f;
	constexpr float TraceHalfLength = 10.f;
	const FVector Target = Hit.ImpactPoint - Hit.ImpactNormal * StepPastEdge;

	FHitResult FaceHit;
	if (World->LineTraceSingleByChannel(FaceHit, Target + Hit.Normal * TraceHalfLength, Target - Hit.Normal * TraceHalfLength,
		Channel, QueryParams, ResponseParams) && !FaceHit.bStartPenetrating)
	{
		return IsValidWallNormal(FaceHit.ImpactNormal, GravityUp, Settings.MinSurfaceAngle, Settings.MaxSurfaceAngle);
	}

	// Other face not found (e.g. a vertex): trust the reported face, which is a valid wall
	return true;
}

bool UGSTN_WallQueryUtils::CanFitOnWall(const FMovingComponentSet& MovingComps, const FQuat& CurrentQuat, const FVector& NewNormal,
	const FVector& ContactPoint, float Radius, float HalfHeight, float WallGap)
{
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	const UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	if (!UpdatedComponent || !UpdatedPrimitive)
	{
		return false;
	}

	FVector Up = FVector::VectorPlaneProject(CurrentQuat.GetUpVector(), NewNormal).GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		Up = FVector::VectorPlaneProject(CurrentQuat.GetForwardVector(), NewNormal).GetSafeNormal();
	}
	const FQuat TestQuat = FRotationMatrix::MakeFromXZ(-NewNormal, Up).ToQuat();

	FVector TestLocation = UpdatedComponent->GetComponentLocation();
	TestLocation += NewNormal * ((Radius + WallGap) - FVector::DotProduct(TestLocation - ContactPoint, NewNormal));

	constexpr float FitTestShrink = 1.f;
	const FCollisionShape TestShape = FCollisionShape::MakeCapsule(FMath::Max(Radius - FitTestShrink, 1.f), FMath::Max(HalfHeight - FitTestShrink, 1.f));
	return !UMovementUtils::OverlapTest(UpdatedComponent, UpdatedPrimitive, TestLocation, TestQuat,
		UpdatedPrimitive->GetCollisionObjectType(), TestShape, UpdatedComponent->GetOwner());
}

bool UGSTN_WallQueryUtils::IsConcaveCornerHit(const FMovingComponentSet& MovingComps, const FHitResult& Hit, const FVector& MoveDelta, const FQuat& Orientation,
	const FVector& WallNormal, const FVector& GravityUp, float MinCornerAngle, const FGSTN_WallCheckSettings& Settings)
{
	if (FVector::DotProduct(MoveDelta, Hit.Normal) >= 0.f)
	{
		return false;
	}

	const float CornerAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Hit.Normal, WallNormal), -1.f, 1.f)));
	if (CornerAngle <= MinCornerAngle || CornerAngle > Settings.MaxConcaveAngle)
	{
		return false;
	}

	const GSTNWallQuery::FQueryContext Ctx(MovingComps, SCENE_QUERY_STAT(GSTN_WallEdgeCheck));
	if (!IsHitOnValidWall(Ctx.World, Hit, GravityUp, Settings, Ctx.Channel, Ctx.QueryParams, Ctx.ResponseParams))
	{
		return false;
	}

	float Radius, HalfHeight;
	GetCapsuleSize(Ctx.UpdatedPrimitive, Radius, HalfHeight);
	return CanFitOnWall(MovingComps, Orientation, Hit.Normal, Hit.ImpactPoint, Radius, HalfHeight, Settings.WallGap);
}

bool UGSTN_WallQueryUtils::IsFloorHit(const FHitResult& Hit, const FVector& MoveDelta, const FVector& GravityUp, float MinSurfaceAngle)
{
	return FVector::DotProduct(Hit.ImpactNormal, GravityUp) >= FMath::Cos(FMath::DegreesToRadians(MinSurfaceAngle))
		&& FVector::DotProduct(MoveDelta, GravityUp) < 0.f;
}

bool UGSTN_WallQueryUtils::FindWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& Velocity,
	const FGSTN_WallCornerHit& CornerHit, const FVector& GravityUp, const FGSTN_WallCheckSettings& Settings, FGSTN_WallProbeResult& OutResult)
{
	const GSTNWallQuery::FQueryContext Ctx(MovingComps, SCENE_QUERY_STAT(GSTN_WallProbe));
	if (!Ctx.World || !Ctx.UpdatedPrimitive)
	{
		return false;
	}

	float Radius, HalfHeight;
	GetCapsuleSize(Ctx.UpdatedPrimitive, Radius, HalfHeight);
	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(Radius * Settings.ProbeRadiusScale);
	const FVector Center = Ctx.UpdatedComponent->GetComponentLocation();

	OutResult.Component = nullptr;
	OutResult.BoneName = NAME_None;

	GSTNWallQuery::FProbeAccumulator Accumulator;
	GSTNWallQuery::AccumulateAlongWallSamples(Ctx, Center, Orientation.GetUpVector(), WallNormal, GravityUp, Radius, HalfHeight, ProbeShape, Settings, Accumulator, OutResult);

	const bool bHasAlongWallContact = Accumulator.HasContact();
	GSTNWallQuery::AccumulateLookaheadSample(MovingComps, Ctx, Center, Orientation, WallNormal, Velocity, GravityUp, Radius, HalfHeight, ProbeShape,
		Settings, bHasAlongWallContact, Accumulator);

	// Blocking hit from the move itself: we are physically in the inside corner
	if (CornerHit.IsValid())
	{
		Accumulator.AddSample(CornerHit.Normal, CornerHit.Point, 2.f, !bHasAlongWallContact);
	}

	if (Accumulator.WeightSum <= UE_KINDA_SMALL_NUMBER || Accumulator.NormalSum.IsNearlyZero())
	{
		return false;
	}

	OutResult.Normal = Accumulator.NormalSum.GetSafeNormal();
	OutResult.ContactPoint = Accumulator.HasContact() ? Accumulator.PointSum / Accumulator.PointWeightSum : Center - OutResult.Normal * (Radius + Settings.WallGap);
	OutResult.bValid = IsValidWallNormal(OutResult.Normal, GravityUp, Settings.MinSurfaceAngle, Settings.MaxSurfaceAngle);
	return OutResult.bValid;
}

bool UGSTN_WallQueryUtils::FindWallAroundConvexEdge(const FMovingComponentSet& MovingComps, const FVector& WallNormal, const FVector& MoveDelta,
	const FVector& GravityUp, const FGSTN_WallCheckSettings& Settings, FGSTN_WallProbeResult& OutResult)
{
	const GSTNWallQuery::FQueryContext Ctx(MovingComps, SCENE_QUERY_STAT(GSTN_WallWrapProbe));
	if (!Ctx.World || !Ctx.UpdatedPrimitive)
	{
		return false;
	}

	float Radius, HalfHeight;
	GetCapsuleSize(Ctx.UpdatedPrimitive, Radius, HalfHeight);
	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(Radius * Settings.ProbeRadiusScale);

	const FVector Center = Ctx.UpdatedComponent->GetComponentLocation();
	const FVector MoveDir = MoveDelta.GetSafeNormal();
	const float CosMaxConvex = FMath::Cos(FMath::DegreesToRadians(Settings.MaxConvexAngle));

	auto Accept = [&](const FHitResult& Hit) -> bool
	{
		if (Hit.bStartPenetrating || FVector::DotProduct(Hit.Normal, WallNormal) < CosMaxConvex
			|| !IsHitOnValidWall(Ctx.World, Hit, GravityUp, Settings, Ctx.Channel, Ctx.QueryParams, Ctx.ResponseParams))
		{
			return false;
		}
		OutResult.Normal = Hit.Normal;
		OutResult.ContactPoint = Hit.ImpactPoint;
		OutResult.Component = Hit.GetComponent();
		OutResult.BoneName = Hit.BoneName;
		OutResult.bValid = true;
		return true;
	};

	// Leg 1: dive "behind" the old wall plane, past where its surface used to be
	const FVector DiveEnd = Center - WallNormal * (Radius + Settings.WallGap + Radius);
	FHitResult Hit;
	if (Ctx.World->SweepSingleByChannel(Hit, Center, DiveEnd, FQuat::Identity, Ctx.Channel, ProbeShape, Ctx.QueryParams, Ctx.ResponseParams))
	{
		return Accept(Hit);
	}

	// Leg 2: sweep back against the movement direction to find the face around the edge.
	// The rounded sphere normal at the edge lets the orientation roll over it continuously.
	const FVector BackEnd = DiveEnd - MoveDir * (MoveDelta.Size() + 2.f * Radius);
	if (Ctx.World->SweepSingleByChannel(Hit, DiveEnd, BackEnd, FQuat::Identity, Ctx.Channel, ProbeShape, Ctx.QueryParams, Ctx.ResponseParams))
	{
#if ENABLE_DRAW_DEBUG
		if (Settings.bDrawDebug && IsInGameThread())
		{
			DrawDebugLine(Ctx.World, Center, DiveEnd, FColor::Purple, false, -1.f, 0, 0.5f);
			DrawDebugLine(Ctx.World, DiveEnd, Hit.Location, FColor::Purple, false, -1.f, 0, 0.5f);
		}
#endif
		return Accept(Hit);
	}

	return false;
}

void UGSTN_WallQueryUtils::GetCapsuleSize(const UPrimitiveComponent* Primitive, float& OutRadius, float& OutHalfHeight)
{
	const FCollisionShape Shape = Primitive ? Primitive->GetCollisionShape() : FCollisionShape();
	if (Shape.IsCapsule())
	{
		OutRadius = Shape.GetCapsuleRadius();
		OutHalfHeight = Shape.GetCapsuleHalfHeight();
	}
	else if (Shape.IsSphere())
	{
		OutRadius = OutHalfHeight = Shape.GetSphereRadius();
	}
	else
	{
		const FVector Extent = Shape.GetExtent();
		OutRadius = FMath::Max(FMath::Min(Extent.X, Extent.Y), 1.f);
		OutHalfHeight = FMath::Max(Extent.Z, OutRadius);
	}
}

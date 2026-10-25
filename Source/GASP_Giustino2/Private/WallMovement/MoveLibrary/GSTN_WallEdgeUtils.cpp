// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallEdgeUtils.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"

namespace GSTNWallEdge
{
	/** Offsets below this with a smooth angle are a plain flat wall, not a feature */
	constexpr float FlatOffsetTolerance = 0.1f;
	/** Slack on the offset a smooth slope explains over the sampled distance (cm) */
	constexpr float SlopeOffsetTolerance = 1.f;
	/** A face this much past MaxSmoothAngle is still smooth: a facet authored at exactly the limit must not flip to a corner on float noise (deg) */
	constexpr float SmoothAngleTolerance = 0.25f;
	/** Velocities along the plane below this have no direction worth looking at (cm/s) */
	constexpr float MinProbeSpeed = 5.f;
	/** How far behind the wall plane the far-face ray runs, so it travels inside the thickness of the wall that just ended (cm) */
	constexpr float FarFaceDepth = 4.f;
	/** The far-face ray runs this much past the look, so an edge exactly at the leading edge is crossed by the ray and not met at its very end (cm) */
	constexpr float FarFaceOvershoot = 6.f;
	/** Without a far face the distance to an ending wall is guessed as this fraction of the look distance */
	constexpr float UnknownEdgeFraction = 0.5f;
	/** A turn of more than this many times MaxSmoothAngle within one look is a corner, not a curve worth the extra traces (a chain of 15 degree steps that sharp is a rounded corner) */
	constexpr float MaxChainAngleFactor = 3.f;
	/** Samples along the look that tell a curve from a crease (the surface under the capsule, then this many steps up to the end) */
	constexpr int32 ChainSamples = 5;
	/** A capsule within this of its seat on the contact is taken as seated (cm) */
	constexpr float SeatTolerance = 1.5f;
	/** How far into the blocking feature the bump probe steps along the movement (cm) */
	constexpr float BumpProbeInset = 1.f;

	/** Collision setup of the probes: the capsule's channel and responses, owner ignored, pawns optionally ignored */
	struct FProbeContext
	{
		const UWorld* World = nullptr;
		const USceneComponent* UpdatedComponent = nullptr;
		const UPrimitiveComponent* UpdatedPrimitive = nullptr;
		ECollisionChannel Channel = ECC_Pawn;
		FCollisionQueryParams QueryParams;
		FCollisionResponseParams ResponseParams;

		FProbeContext(const FMovingComponentSet& MovingComps, const FGSTN_WallEdgeSettings& Edges, const FName TraceTag, const TStatId& StatId)
			: UpdatedComponent(MovingComps.UpdatedComponent.Get())
			, UpdatedPrimitive(MovingComps.UpdatedPrimitive.Get())
			, QueryParams(TraceTag, StatId, false, UpdatedComponent ? UpdatedComponent->GetOwner() : nullptr)
		{
			World = UpdatedComponent ? UpdatedComponent->GetWorld() : nullptr;
			if (UpdatedPrimitive)
			{
				Channel = UpdatedPrimitive->GetCollisionObjectType();
				UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParams);
				if (Edges.bIgnorePawns)
				{
					// Other characters are not wall geometry: their capsules differ between a client and the server (finding 15)
					ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
				}
			}
		}

		bool IsValid() const { return World && UpdatedPrimitive; }

		bool Trace(FHitResult& OutHit, const FVector& Start, const FVector& End) const
		{
			GSTN_WALL_QUERY_SCOPE();
			return World->LineTraceSingleByChannel(OutHit, Start, End, Channel, QueryParams, ResponseParams);
		}

		bool IsValidWall(const FHitResult& Hit, const FVector& GravityUp, const FGSTN_WallCheckSettings& Check) const
		{
			return UGSTN_WallQueryUtils::IsHitOnValidWall(World, Hit, GravityUp, Check, Channel, QueryParams, ResponseParams);
		}
	};

	/** True when AngleDeg is within the smooth limit (plus the float noise allowance) */
	inline bool IsSmooth(const FGSTN_WallEdgeSettings& Edges, float AngleDeg)
	{
		return AngleDeg <= Edges.MaxSmoothAngle + SmoothAngleTolerance;
	}

	/**
	 * A surface that turns away (or toward) the character in steps of at most MaxSmoothAngle is a curve, a chain of small slopes, not a corner. One angle
	 * cannot tell them apart (a crease and an arc both read "20 degrees at 100 cm"), several can: the angle of the surface under the capsule and at points up to the
	 * end of the look. A crease puts all of its angle in one of the steps between them, an arc spreads it. The angles are measured from the proxy plane, which
	 * lags a curve by a few degrees, so only the steps count, not the angles themselves
	 */
	inline bool IsChainOfSmallTurns(const FGSTN_WallEdgeSettings& Edges, float AngleNear, float AngleFar)
	{
		return IsSmooth(Edges, FMath::Abs(AngleFar - AngleNear));
	}

	/** The angle between the wall normal and the surface found by a trace straight down toward the wall at Column (a point on the wall plane): false when nothing valid is there */
	inline bool SampleAngleAt(const FProbeContext& Ctx, const FGSTN_WallCheckSettings& Check, const FVector& Column, const FVector& WallNormal, const FVector& GravityUp,
		float Lift, float Depth, float& OutAngle)
	{
		const FVector Origin = Column + WallNormal * Lift;
		FHitResult Hit;
		if (!Ctx.Trace(Hit, Origin, Origin - WallNormal * Depth) || Hit.bStartPenetrating || !Ctx.IsValidWall(Hit, GravityUp, Check))
		{
			return false;
		}
		OutAngle = UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(Hit.ImpactNormal, WallNormal);
		return true;
	}
}

float UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(const FVector& A, const FVector& B)
{
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.f, 1.f)));
}

float UGSTN_WallEdgeUtils::LookaheadDistance(const FGSTN_WallEdgeSettings& Settings, float Speed)
{
	return FMath::Max(Settings.MinLookahead, FMath::Max(Speed, 0.f) * Settings.LookaheadTime);
}

EGSTN_WallEdgeKind UGSTN_WallEdgeUtils::ClassifyAhead(const FGSTN_WallEdgeSettings& Settings, float MaxEdgeAngleDeg, bool bHitAhead, float Offset, float AngleDeg, bool bFarFaceFound, float AheadDistance)
{
	if (bHitAhead)
	{
		const bool bSmooth = AngleDeg <= Settings.MaxSmoothAngle + GSTNWallEdge::SmoothAngleTolerance;
		const float AbsOffset = FMath::Abs(Offset);
		if (AbsOffset <= Settings.MaxStepHeight)
		{
			// Step-up has precedence over the angle: continuous within step height
			if (!bSmooth)
			{
				return EGSTN_WallEdgeKind::Bump;
			}
			if (AbsOffset <= GSTNWallEdge::FlatOffsetTolerance)
			{
				return EGSTN_WallEdgeKind::None;
			}
			// A smooth face is a slope when its offset is what that slope makes over the sampled distance; more than that is a step
			// whose face happens to be parallel to the wall (a ledge, a rib)
			const bool bSlopeConsistent = AheadDistance < 0.f
				|| AbsOffset <= AheadDistance * FMath::Tan(FMath::DegreesToRadians(AngleDeg)) + GSTNWallEdge::SlopeOffsetTolerance;
			return bSlopeConsistent ? EGSTN_WallEdgeKind::Slope : EGSTN_WallEdgeKind::Bump;
		}
		if (bSmooth)
		{
			return EGSTN_WallEdgeKind::Slope;
		}
		if (AngleDeg > MaxEdgeAngleDeg)
		{
			return EGSTN_WallEdgeKind::Blocked;
		}
		// Taller than the step: the surface found rises toward the character (inside corner) or falls away from it (outside edge)
		return Offset > 0.f ? EGSTN_WallEdgeKind::Convex : EGSTN_WallEdgeKind::Concave;
	}

	// The wall is gone ahead. Without a far face it simply ends: an outside edge
	if (!bFarFaceFound)
	{
		return EGSTN_WallEdgeKind::Convex;
	}
	if (AngleDeg <= Settings.MaxSmoothAngle + GSTNWallEdge::SmoothAngleTolerance)
	{
		return EGSTN_WallEdgeKind::Slope;
	}
	return AngleDeg <= MaxEdgeAngleDeg ? EGSTN_WallEdgeKind::Convex : EGSTN_WallEdgeKind::Blocked;
}

float UGSTN_WallEdgeUtils::EdgeSpeedCap(const FGSTN_WallEdgeSettings& Settings, EGSTN_WallEdgeKind Kind, float AngleDeg, float TerminalSpeed)
{
	float MinScale;
	if (Kind == EGSTN_WallEdgeKind::Concave && Settings.bSlowOnConcave)
	{
		MinScale = Settings.ConcaveMinScale;
	}
	else if (Kind == EGSTN_WallEdgeKind::Convex && Settings.bSlowOnConvex)
	{
		MinScale = Settings.ConvexMinScale;
	}
	else
	{
		return TNumericLimits<float>::Max();
	}

	const float Span = FMath::Max(90.f - Settings.MaxSmoothAngle, UE_KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp((AngleDeg - Settings.MaxSmoothAngle) / Span, 0.f, 1.f);
	return FMath::Max(TerminalSpeed, 0.f) * FMath::Lerp(1.f, MinScale, Alpha);
}

float UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(const FGSTN_WallEdgeSettings& Settings, float CornerSpeed, float Distance)
{
	return FMath::Sqrt(FMath::Square(FMath::Max(CornerSpeed, 0.f)) + 2.f * FMath::Max(Settings.CornerBraking, 0.f) * FMath::Max(Distance, 0.f));
}

float UGSTN_WallEdgeUtils::BumpHeightOf(const FGSTN_WallEdgeSettings& Settings, float Offset)
{
	return FMath::Clamp(-Offset, 0.f, Settings.MaxStepHeight);
}

float UGSTN_WallEdgeUtils::StepLiftFor(const FGSTN_WallEdgeSettings& Settings, float BumpHeight, float WallGap)
{
	const float Lift = FMath::Max(BumpHeight - FMath::Max(WallGap, 0.f), 0.f) + Settings.StepMargin;
	return FMath::Clamp(Lift, 0.f, Settings.MaxStepHeight + Settings.StepMargin);
}

float UGSTN_WallEdgeUtils::CapsuleSupportAlong(float Radius, float HalfHeight, const FQuat& CapsuleQuat, const FVector& Dir)
{
	return FMath::Max(HalfHeight - Radius, 0.f) * FMath::Abs(FVector::DotProduct(Dir, CapsuleQuat.GetUpVector())) + Radius;
}

FGSTN_WallAheadResult UGSTN_WallEdgeUtils::ProbeAhead(const FMovingComponentSet& MovingComps, const FGSTN_WallEdgeSettings& Edges, const FGSTN_WallCheckSettings& Check,
	const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, const FVector* KnownPlanePoint)
{
	FGSTN_WallAheadResult Result;
	const GSTNWallEdge::FProbeContext Ctx(MovingComps, Edges, SCENE_QUERY_STAT(GSTN_WallAhead));
	const FVector OnPlane = FVector::VectorPlaneProject(Velocity, WallNormal);
	const float Speed = OnPlane.Size();
	if (!Ctx.IsValid() || Speed < GSTNWallEdge::MinProbeSpeed)
	{
		return Result;
	}

	const FVector Dir = OnPlane / Speed;
	float Radius, HalfHeight;
	UGSTN_WallQueryUtils::GetCapsuleSize(Ctx.UpdatedPrimitive, Radius, HalfHeight);
	const FQuat Quat = Ctx.UpdatedComponent->GetComponentQuat();

	// The wall plane under the capsule and the leading edge of the capsule along the movement. The plane is where the capsule would be seated
	// (WallGap), unless a point on the real surface is known: the capsule is not seated yet on the first ticks after a grab (the entry sweep
	// reaches several cm, the pull-in is rate limited), and measuring the wall ahead from a plane that is not there reads the gap error as a
	// feature (a "bump" or "slope" on a flat wall)
	const float SupportOnNormal = CapsuleSupportAlong(Radius, HalfHeight, Quat, WallNormal);
	const float Lead = CapsuleSupportAlong(Radius, HalfHeight, Quat, Dir);
	const FVector CapsuleLocation = Ctx.UpdatedComponent->GetComponentLocation();
	float SeatDistance = SupportOnNormal + Check.WallGap;
	if (KnownPlanePoint)
	{
		// Only a plausible point (one the sweeps could have found: a stale point of an old wall must not move the plane), and only when the capsule
		// is really off its seat. A seated capsule keeps the plane of the seat: the contact of the last tick is up to a tick of travel behind, which
		// on a facet or a curve is a different depth than the plane under the capsule now
		const float Known = FVector::DotProduct(CapsuleLocation - *KnownPlanePoint, WallNormal);
		if (Known >= SupportOnNormal * 0.5f && Known <= SeatDistance + Check.WallProbeDistance && FMath::Abs(Known - SeatDistance) > GSTNWallEdge::SeatTolerance)
		{
			SeatDistance = Known;
		}
	}
	const FVector Seat = CapsuleLocation - WallNormal * SeatDistance;
	const FVector Front = Seat + Dir * Lead;
	const float Look = LookaheadDistance(Edges, Speed);
	const float Lift = Edges.MaxStepHeight + Edges.StepMargin;

	Result.Distance = Look;

	// 1. Down toward the wall from the end point, lifted off the plane. It reaches RecessDepth below the plane plus what a smooth slope that began at the capsule
	// has receded by the end point: a 15 degree facet is 30 cm behind the plane 110 cm along, and that is a slope to follow, not the end of the wall
	const FVector End = Front + Dir * Look;
	const FVector Origin = End + WallNormal * Lift;
	const float SlopeReach = (Lead + Look) * FMath::Tan(FMath::DegreesToRadians(Edges.MaxSmoothAngle + GSTNWallEdge::SmoothAngleTolerance));
	const float TraceDepth = Lift + Edges.RecessDepth + SlopeReach;
	FHitResult Hit;
	const bool bHit = Ctx.Trace(Hit, Origin, Origin - WallNormal * TraceDepth);

	if (bHit && Hit.bStartPenetrating)
	{
		// 2. Inside geometry that is taller than the step: an inside corner (or a floor/ceiling, which no wall turn can follow)
		const FVector RayStart = Front + WallNormal * Lift;
		FHitResult Face;
		if (!Ctx.Trace(Face, RayStart, RayStart + Dir * Look) || Face.bStartPenetrating)
		{
			return Result;
		}
		Result.Normal = Face.ImpactNormal;
		Result.Point = Face.ImpactPoint;
		Result.Component = Face.GetComponent();
		Result.Angle = AngleBetweenNormalsDeg(Face.ImpactNormal, WallNormal);
		Result.Offset = -FMath::Max(Lift, Edges.MaxStepHeight + 1.f);
		Result.Distance = Face.Distance;
		Result.Kind = Ctx.IsValidWall(Face, GravityUp, Check)
			? ClassifyAhead(Edges, Check.MaxConcaveAngle, true, Result.Offset, Result.Angle, false, Face.Distance)
			: EGSTN_WallEdgeKind::Blocked;
		if (Result.Kind == EGSTN_WallEdgeKind::Concave && Result.Angle <= Edges.MaxSmoothAngle * GSTNWallEdge::MaxChainAngleFactor)
		{
			// Curved or creased? The surface under the capsule, and one nearer than the face found: a ray lifted half as high, from the capsule itself (not from its
			// leading edge, which may be inside the rising surface already), meets the surface nearer, at a smaller angle if it is an arc, at the same one if it is a crease
			float AngleUnder;
			const FVector LowStart = Seat + WallNormal * (Lift * 0.5f);
			FHitResult Near;
			if (GSTNWallEdge::SampleAngleAt(Ctx, Check, Seat, WallNormal, GravityUp, Lift, TraceDepth, AngleUnder)
				&& Ctx.Trace(Near, LowStart, LowStart + Dir * (Lead + Look)) && !Near.bStartPenetrating && Near.Distance + 1.f < Lead + Face.Distance && Ctx.IsValidWall(Near, GravityUp, Check)
				&& GSTNWallEdge::IsChainOfSmallTurns(Edges, AngleUnder, AngleBetweenNormalsDeg(Near.ImpactNormal, WallNormal))
				&& GSTNWallEdge::IsChainOfSmallTurns(Edges, AngleBetweenNormalsDeg(Near.ImpactNormal, WallNormal), Result.Angle))
			{
				Result.Kind = EGSTN_WallEdgeKind::Slope;
			}
		}
		return Result;
	}

	if (bHit && Ctx.IsValidWall(Hit, GravityUp, Check))
	{
		Result.Normal = Hit.ImpactNormal;
		Result.Point = Hit.ImpactPoint;
		Result.Component = Hit.GetComponent();
		Result.Angle = AngleBetweenNormalsDeg(Hit.ImpactNormal, WallNormal);
		Result.Offset = Hit.Distance - Lift;
		Result.Kind = ClassifyAhead(Edges, Result.Offset <= 0.f ? Check.MaxConcaveAngle : Check.MaxConvexAngle, true, Result.Offset, Result.Angle, false, Look);
		if ((Result.Kind == EGSTN_WallEdgeKind::Concave || Result.Kind == EGSTN_WallEdgeKind::Convex) && Result.Angle > UE_KINDA_SMALL_NUMBER)
		{
			// Where the corner is: the surface found is a ramp that started at the plane, Offset / tan(Angle) before the end point
			const float Run = FMath::Abs(Result.Offset) / FMath::Tan(FMath::DegreesToRadians(Result.Angle));
			Result.Distance = FMath::Clamp(Look - Run, 0.f, Look);

			// Curved or creased? The surface under the capsule and at a few points from there to the end of the look: an arc turns a little between each, a crease
			// all of it in one. Five steps, so that a wall made of facets sees at most one of its (small) creases in each
			float Previous = 0.f;
			bool bChain = Result.Angle <= Edges.MaxSmoothAngle * GSTNWallEdge::MaxChainAngleFactor
				&& GSTNWallEdge::SampleAngleAt(Ctx, Check, Seat, WallNormal, GravityUp, Lift, TraceDepth, Previous);
			for (int32 Step = 1; Step < GSTNWallEdge::ChainSamples && bChain; ++Step)
			{
				float Angle;
				bChain = GSTNWallEdge::SampleAngleAt(Ctx, Check, Seat + Dir * ((Lead + Look) * Step / GSTNWallEdge::ChainSamples), WallNormal, GravityUp, Lift, TraceDepth, Angle)
					&& GSTNWallEdge::IsChainOfSmallTurns(Edges, Previous, Angle);
				Previous = Angle;
			}
			if (bChain && GSTNWallEdge::IsChainOfSmallTurns(Edges, Previous, Result.Angle))
			{
				Result.Kind = EGSTN_WallEdgeKind::Slope;
			}
		}
		return Result;
	}

	// 3. The wall ends (nothing there, or a surface the wall filter refuses): is there a face around the edge, behind the plane?
	const FVector FarStart = End - WallNormal * GSTNWallEdge::FarFaceDepth;
	FHitResult Far;
	if (Ctx.Trace(Far, FarStart, FarStart - Dir * (Look + GSTNWallEdge::FarFaceOvershoot)))
	{
		if (Far.bStartPenetrating)
		{
			// Still inside the wall's own thickness at the end of the look: it did not end, nothing to react to
			return Result;
		}
		if (Ctx.IsValidWall(Far, GravityUp, Check))
		{
			Result.Normal = Far.ImpactNormal;
			Result.Point = Far.ImpactPoint;
			Result.Component = Far.GetComponent();
			Result.Angle = AngleBetweenNormalsDeg(Far.ImpactNormal, WallNormal);
			Result.Distance = FMath::Clamp(Look - Far.Distance, 0.f, Look);
			Result.Kind = ClassifyAhead(Edges, Check.MaxConvexAngle, false, 0.f, Result.Angle, true);
			return Result;
		}
	}

	// No face to turn onto: the wall just stops. A full 90 degree edge at an unknown place in the look
	Result.Angle = 90.f;
	Result.Distance = Look * GSTNWallEdge::UnknownEdgeFraction;
	Result.Kind = ClassifyAhead(Edges, Check.MaxConvexAngle, false, 0.f, Result.Angle, false);
	return Result;
}

FGSTN_WallAheadResult UGSTN_WallEdgeUtils::ProbeBumpAt(const FMovingComponentSet& MovingComps, const FGSTN_WallEdgeSettings& Edges, const FGSTN_WallCheckSettings& Check,
	const FVector& WallNormal, const FVector& GravityUp, const FVector& HitPoint, const FVector& Dir)
{
	FGSTN_WallAheadResult Result;
	const GSTNWallEdge::FProbeContext Ctx(MovingComps, Edges, SCENE_QUERY_STAT(GSTN_WallBump));
	if (!Ctx.IsValid())
	{
		return Result;
	}

	float Radius, HalfHeight;
	UGSTN_WallQueryUtils::GetCapsuleSize(Ctx.UpdatedPrimitive, Radius, HalfHeight);
	const FQuat Quat = Ctx.UpdatedComponent->GetComponentQuat();

	// Same plane as ProbeAhead (the capsule seated at WallGap), lifted by the step height; the trace column is the blocking point moved a
	// little into the feature, so it lands on the feature's own top and not on the air beside it
	const float PlaneDepth = FVector::DotProduct(Ctx.UpdatedComponent->GetComponentLocation(), WallNormal)
		- CapsuleSupportAlong(Radius, HalfHeight, Quat, WallNormal) - Check.WallGap;
	const float Lift = Edges.MaxStepHeight + Edges.StepMargin;
	FVector Origin = HitPoint + Dir * GSTNWallEdge::BumpProbeInset;
	Origin += WallNormal * ((PlaneDepth + Lift) - FVector::DotProduct(Origin, WallNormal));

	FHitResult Hit;
	if (!Ctx.Trace(Hit, Origin, Origin - WallNormal * (Lift + Edges.RecessDepth)) || Hit.bStartPenetrating || !Ctx.IsValidWall(Hit, GravityUp, Check))
	{
		return Result;
	}

	Result.Normal = Hit.ImpactNormal;
	Result.Point = Hit.ImpactPoint;
	Result.Component = Hit.GetComponent();
	Result.Angle = AngleBetweenNormalsDeg(Hit.ImpactNormal, WallNormal);
	Result.Offset = Hit.Distance - Lift;
	Result.Distance = 0.f;
	Result.Kind = ClassifyAhead(Edges, Result.Offset <= 0.f ? Check.MaxConcaveAngle : Check.MaxConvexAngle, true, Result.Offset, Result.Angle, false, GSTNWallEdge::BumpProbeInset);
	return Result;
}

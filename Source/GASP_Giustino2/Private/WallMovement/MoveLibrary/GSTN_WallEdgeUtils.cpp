// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallEdgeUtils.h"

namespace GSTNWallEdge
{
	/** Offsets below this with a smooth angle are a plain flat wall, not a feature */
	constexpr float FlatOffsetTolerance = 0.1f;
}

float UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(const FVector& A, const FVector& B)
{
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.f, 1.f)));
}

float UGSTN_WallEdgeUtils::LookaheadDistance(const FGSTN_WallEdgeSettings& Settings, float Speed)
{
	return FMath::Max(Settings.MinLookahead, FMath::Max(Speed, 0.f) * Settings.LookaheadTime);
}

EGSTN_WallEdgeKind UGSTN_WallEdgeUtils::ClassifyAhead(const FGSTN_WallEdgeSettings& Settings, float MaxEdgeAngleDeg, bool bHitAhead, float Offset, float AngleDeg, bool bFarFaceFound)
{
	if (bHitAhead)
	{
		const bool bSmooth = AngleDeg <= Settings.MaxSmoothAngle;
		if (FMath::Abs(Offset) <= Settings.MaxStepHeight)
		{
			// Step-up has precedence over the angle: continuous within step height. A smooth, flush surface is just the wall
			if (bSmooth)
			{
				return FMath::Abs(Offset) <= GSTNWallEdge::FlatOffsetTolerance ? EGSTN_WallEdgeKind::None : EGSTN_WallEdgeKind::Slope;
			}
			return EGSTN_WallEdgeKind::Bump;
		}
		if (bSmooth)
		{
			return EGSTN_WallEdgeKind::Slope;
		}
		return AngleDeg <= MaxEdgeAngleDeg ? EGSTN_WallEdgeKind::Concave : EGSTN_WallEdgeKind::Blocked;
	}

	// The wall is gone ahead. Without a far face it simply ends: an outside edge
	if (!bFarFaceFound)
	{
		return EGSTN_WallEdgeKind::Convex;
	}
	if (AngleDeg <= Settings.MaxSmoothAngle)
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

// Projectile, pure part (no GAS, no Network Prediction, no Mover).

#include "Movement/GSTN_BallisticMode.h"

#include "Hits/GSTN_HitShapeUtils.h"
#include "Serialization/Archive.h"

namespace
{
	// 1/100 cm: far below what is visible or hittable; packed components stay 3-4 bytes for positions in a level
	constexpr double GSTNBallisticScale = 100.0;

	int64 ToUnits(double Value)
	{
		return FMath::RoundToInt64(Value * GSTNBallisticScale);
	}

	// The one decode both Quantize and NetSerialize use, so the host's and the clients' doubles are the same bits
	double FromUnits(int64 Units)
	{
		return static_cast<double>(Units) / GSTNBallisticScale;
	}

	FVector QuantizeVector(const FVector& V)
	{
		return FVector(FromUnits(ToUnits(V.X)), FromUnits(ToUnits(V.Y)), FromUnits(ToUnits(V.Z)));
	}

	uint64 ZigZag(int64 Value)
	{
		return (static_cast<uint64>(Value) << 1) ^ static_cast<uint64>(Value >> 63);
	}

	int64 UnZigZag(uint64 Value)
	{
		return static_cast<int64>(Value >> 1) ^ -static_cast<int64>(Value & 1);
	}

	void SerializeUnits(FArchive& Ar, double& Value)
	{
		uint64 Packed = Ar.IsSaving() ? ZigZag(ToUnits(Value)) : 0;
		Ar.SerializeIntPacked64(Packed);
		if (Ar.IsLoading())
		{
			Value = FromUnits(UnZigZag(Packed));
		}
	}

	void SerializeUnits(FArchive& Ar, FVector& V)
	{
		SerializeUnits(Ar, V.X);
		SerializeUnits(Ar, V.Y);
		SerializeUnits(Ar, V.Z);
	}
}

bool FGSTN_BallisticSegment::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint64 Frame = Ar.IsSaving() ? ZigZag(StartFrame) : 0;
	Ar.SerializeIntPacked64(Frame);
	SerializeUnits(Ar, Start);
	SerializeUnits(Ar, Velocity);
	SerializeUnits(Ar, GravityAccel);
	uint32 StepMs = Ar.IsSaving() ? static_cast<uint32>(FMath::Max<int64>(FMath::RoundToInt64(StepSeconds * 1000.0), 1)) : 0;
	Ar.SerializeIntPacked(StepMs);
	if (Ar.IsLoading())
	{
		StartFrame = static_cast<int32>(UnZigZag(Frame));
		StepSeconds = static_cast<double>(StepMs) / 1000.0;
	}
	bOutSuccess = !Ar.IsError();
	return true;
}

FGSTN_BallisticSegment FGSTN_BallisticMode::Quantize(const FGSTN_BallisticSegment& Segment)
{
	FGSTN_BallisticSegment Out = Segment;
	Out.Start = QuantizeVector(Segment.Start);
	Out.Velocity = QuantizeVector(Segment.Velocity);
	Out.GravityAccel = QuantizeVector(Segment.GravityAccel);
	Out.StepSeconds = static_cast<double>(FMath::Max<int64>(FMath::RoundToInt64(Segment.StepSeconds * 1000.0), 1)) / 1000.0;
	return Out;
}

FGSTN_BallisticSegment FGSTN_BallisticMode::MakeSegment(const FGSTN_ProjectileParams& Params, int32 StartFrame, double StepSeconds)
{
	FGSTN_BallisticSegment Segment;
	Segment.StartFrame = StartFrame;
	Segment.Start = Params.Origin;
	Segment.Velocity = Params.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector) * static_cast<double>(Params.Speed);
	Segment.GravityAccel = Params.GravityAccel;
	Segment.StepSeconds = StepSeconds;
	return Segment;
}

FVector FGSTN_BallisticMode::PositionAt(const FGSTN_BallisticSegment& Segment, int32 Frame)
{
	const double T = static_cast<double>(FMath::Max(Frame - Segment.StartFrame, 0)) * Segment.StepSeconds;
	return Segment.Start + Segment.Velocity * T + Segment.GravityAccel * (0.5 * T * T);
}

FVector FGSTN_BallisticMode::VelocityAt(const FGSTN_BallisticSegment& Segment, int32 Frame)
{
	const double T = static_cast<double>(FMath::Max(Frame - Segment.StartFrame, 0)) * Segment.StepSeconds;
	return Segment.Velocity + Segment.GravityAccel * T;
}

FTransform FGSTN_BallisticMode::CarrierAt(const FGSTN_BallisticSegment& Segment, int32 Frame)
{
	const FVector Launch = Segment.Velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Forward = VelocityAt(Segment, Frame).GetSafeNormal(UE_SMALL_NUMBER, Launch);
	return FTransform(FRotationMatrix::MakeFromX(Forward).ToQuat(), PositionAt(Segment, Frame));
}

bool FGSTN_BallisticMode::SweepStep(const FGSTN_BallisticSegment& Segment, const FGSTN_HitShape& Shape, int32 Frame, const FGSTN_HitShape& Target, const FTransform& TargetWorld)
{
	return UGSTN_HitShapeUtils::SweptOverlap(Shape, CarrierAt(Segment, Frame - 1), CarrierAt(Segment, Frame), Target, TargetWorld);
}

int32 FGSTN_BallisticMode::FindFirstContactFrame(const FGSTN_BallisticSegment& Segment, const FGSTN_HitShape& Shape, const FGSTN_HitShape& Target, const FTransform& TargetWorld, int32 LastFrame)
{
	for (int32 Frame = Segment.StartFrame + 1; Frame <= LastFrame; ++Frame)
	{
		if (SweepStep(Segment, Shape, Frame, Target, TargetWorld))
		{
			return Frame;
		}
	}
	return INDEX_NONE;
}

bool FGSTN_BallisticMode::SolveLaunchDirection(const FVector& Origin, const FVector& Target, double Speed, const FVector& GravityAccel, bool bHighArc, FVector& OutDirection)
{
	const FVector Delta = Target - Origin;
	const double G = GravityAccel.Size();
	if (Speed <= UE_SMALL_NUMBER || Delta.IsNearlyZero())
	{
		return false;
	}
	if (G <= UE_SMALL_NUMBER)
	{
		OutDirection = Delta.GetSafeNormal();
		return true;
	}

	// Work in the plane that holds gravity and the target: Up against gravity, X the distance across it, Y the height along Up
	const FVector Up = -GravityAccel / G;
	const double Y = Delta | Up;
	const FVector Across = Delta - Up * Y;
	const double X = Across.Size();
	const double V2 = Speed * Speed;
	if (X <= UE_KINDA_SMALL_NUMBER)
	{
		// Straight up or down: down always arrives, up only if the speed reaches the height
		if (Y > 0.0 && V2 < 2.0 * G * Y)
		{
			return false;
		}
		OutDirection = Y >= 0.0 ? Up : -Up;
		return true;
	}

	const double Discriminant = V2 * V2 - G * (G * X * X + 2.0 * Y * V2);
	if (Discriminant < 0.0)
	{
		return false;
	}
	const double TanTheta = (V2 + (bHighArc ? 1.0 : -1.0) * FMath::Sqrt(Discriminant)) / (G * X);
	const double Theta = FMath::Atan(TanTheta);
	OutDirection = (Across / X) * FMath::Cos(Theta) + Up * FMath::Sin(Theta);
	return true;
}

double FGSTN_ProjectileGhostReconcile::PositionError(const FGSTN_BallisticSegment& Ghost, const FGSTN_BallisticSegment& Authority, int32 Frame)
{
	return FVector::Dist(FGSTN_BallisticMode::PositionAt(Ghost, Frame), FGSTN_BallisticMode::PositionAt(Authority, Frame));
}

FGSTN_ProjectileGhostReconcile::EResult FGSTN_ProjectileGhostReconcile::Decide(const FGSTN_BallisticSegment& Ghost, const FGSTN_BallisticSegment& Authority,
	int32 Frame, int32 MaxFrameDelta, double ToleranceCm, double& OutErrorCm)
{
	OutErrorCm = PositionError(Ghost, Authority, Frame);
	if (FMath::Abs(Ghost.StartFrame - Authority.StartFrame) > FMath::Max(MaxFrameDelta, 0))
	{
		return EResult::NoMatch;
	}
	return OutErrorCm <= ToleranceCm ? EResult::Adopt : EResult::Snap;
}

FVector FGSTN_ProjectileGhostReconcile::BlendOffset(const FVector& InitialOffset, int32 AdoptFrame, int32 BlendFrames, int32 Frame)
{
	if (BlendFrames <= 0 || Frame >= AdoptFrame + BlendFrames)
	{
		return FVector::ZeroVector;
	}
	const double Alpha = static_cast<double>(FMath::Max(Frame - AdoptFrame, 0)) / static_cast<double>(BlendFrames);
	return InitialOffset * (1.0 - Alpha);
}

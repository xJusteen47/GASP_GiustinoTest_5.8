// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#include "Math/GSTN_DrillMath.h"

#include "Curves/CurveFloat.h"
#include "Jump/GSTN_JumpTypes.h"

namespace GSTNDrillMath
{
	constexpr double AxisScale = 32767.0;

	int16 QuantizeComponent(double Value, bool& bInOutInRange)
	{
		const int64 Rounded = FMath::FloorToInt64(Value + 0.5);
		if (Rounded < MIN_int16 || Rounded > MAX_int16)
		{
			bInOutInRange = false;
		}
		return static_cast<int16>(FMath::Clamp<int64>(Rounded, MIN_int16, MAX_int16));
	}

	/** Sign with +1 for zero, so the octahedral fold is defined on the axes */
	double SignNotZero(double Value)
	{
		return Value >= 0.0 ? 1.0 : -1.0;
	}

	int16 QuantizeUnit(double Value)
	{
		const double Clamped = FMath::Clamp(Value, -1.0, 1.0);
		return static_cast<int16>(FMath::FloorToInt64(Clamped * AxisScale + 0.5));
	}
}

bool UGSTN_DrillMath::QuantizeAnchor(const FVector& LocalCm, FGSTN_DrillAnchorQ& OutAnchor)
{
	bool bInRange = true;
	OutAnchor.X = GSTNDrillMath::QuantizeComponent(LocalCm.X, bInRange);
	OutAnchor.Y = GSTNDrillMath::QuantizeComponent(LocalCm.Y, bInRange);
	OutAnchor.Z = GSTNDrillMath::QuantizeComponent(LocalCm.Z, bInRange);
	return bInRange;
}

FVector UGSTN_DrillMath::DequantizeAnchor(const FGSTN_DrillAnchorQ& Anchor)
{
	return FVector(static_cast<double>(Anchor.X), static_cast<double>(Anchor.Y), static_cast<double>(Anchor.Z));
}

FGSTN_DrillAxisQ UGSTN_DrillMath::EncodeAxis(const FVector& Axis)
{
	const double L1 = FMath::Abs(Axis.X) + FMath::Abs(Axis.Y) + FMath::Abs(Axis.Z);
	FGSTN_DrillAxisQ Result;
	if (L1 <= UE_DOUBLE_SMALL_NUMBER)
	{
		return Result;
	}

	double X = Axis.X / L1;
	double Y = Axis.Y / L1;
	if (Axis.Z < 0.0)
	{
		// Lower hemisphere folded over the diagonals of the square
		const double FoldedX = (1.0 - FMath::Abs(Y)) * GSTNDrillMath::SignNotZero(X);
		const double FoldedY = (1.0 - FMath::Abs(X)) * GSTNDrillMath::SignNotZero(Y);
		X = FoldedX;
		Y = FoldedY;
	}
	Result.X = GSTNDrillMath::QuantizeUnit(X);
	Result.Y = GSTNDrillMath::QuantizeUnit(Y);
	return Result;
}

FVector UGSTN_DrillMath::DecodeAxis(const FGSTN_DrillAxisQ& Axis)
{
	double X = Axis.X / GSTNDrillMath::AxisScale;
	double Y = Axis.Y / GSTNDrillMath::AxisScale;
	const double Z = 1.0 - FMath::Abs(X) - FMath::Abs(Y);
	if (Z < 0.0)
	{
		const double UnfoldedX = (1.0 - FMath::Abs(Y)) * GSTNDrillMath::SignNotZero(X);
		const double UnfoldedY = (1.0 - FMath::Abs(X)) * GSTNDrillMath::SignNotZero(Y);
		X = UnfoldedX;
		Y = UnfoldedY;
	}
	// Never zero: |X| + |Y| + |Z| = 1 after the unfold
	return FVector(X, Y, Z).GetUnsafeNormal();
}

int32 UGSTN_DrillMath::ResolveSeatFrames(int32 SeatFrames, float SeatFramesScale)
{
	return FMath::Max(0, FMath::RoundToInt(static_cast<float>(SeatFrames) * SeatFramesScale));
}

int32 UGSTN_DrillMath::ResolveMaxBoreFrames(int32 TuningMaxBoreFrames, int32 TargetOverride)
{
	return FMath::Max(1, TargetOverride > 0 ? TargetOverride : TuningMaxBoreFrames);
}

bool UGSTN_DrillMath::IsJumpBoostInWindow(int32 Now, int32 LastJumpPressFrame, bool bJumpHeld, int32 WindowFrames)
{
	if (bJumpHeld)
	{
		return true;
	}
	if (LastJumpPressFrame == INDEX_NONE || LastJumpPressFrame > Now)
	{
		return false;
	}
	return Now - LastJumpPressFrame <= FMath::Max(0, WindowFrames);
}

EGSTN_DrillEndResult UGSTN_DrillMath::ResolveEnd(const FGSTN_DrillEndInputs& Inputs)
{
	if (Inputs.bDrillerInHitstun || Inputs.bDrillerKnockedOut)
	{
		return EGSTN_DrillEndResult::DetachHitstun;
	}
	if (Inputs.bTargetGone || Inputs.bTargetNotDrillable)
	{
		return Inputs.bContinueThroughDestroyed ? EGSTN_DrillEndResult::KeepPlunging : EGSTN_DrillEndResult::Launch;
	}
	if (!Inputs.bCrouchHeld || Inputs.FramesBored >= Inputs.MaxBoreFrames)
	{
		return Inputs.bJumpBoost ? EGSTN_DrillEndResult::Launch : EGSTN_DrillEndResult::DetachPop;
	}
	return EGSTN_DrillEndResult::Continue;
}

bool UGSTN_DrillMath::ShouldRefreshAirSpin(int32 RefreshOnMask, int32 EventMask)
{
	return (RefreshOnMask & EventMask) != 0;
}

int32 UGSTN_DrillMath::AirSpinsUsedAfterEvents(int32 AirSpinsUsed, int32 RefreshOnMask, int32 EventMask)
{
	return ShouldRefreshAirSpin(RefreshOnMask, EventMask) ? 0 : AirSpinsUsed;
}

bool UGSTN_DrillMath::CanAirSpin(int32 AirSpinsUsed, int32 UsesPerAirtime)
{
	return AirSpinsUsed < UsesPerAirtime;
}

int32 UGSTN_DrillMath::RefreshEventsForEnd(EGSTN_DrillEndResult Result, bool bLaunchRefreshesAirSpin)
{
	return Result == EGSTN_DrillEndResult::Launch && bLaunchRefreshesAirSpin ? static_cast<int32>(EGSTN_AirSpinRefresh::Launch) : 0;
}

int32 UGSTN_DrillMath::RefreshEventsFromJumpPhase(uint8 PreviousPhase, uint8 CurrentPhase)
{
	const uint8 Grounded = static_cast<uint8>(EGSTN_JumpPhase::None);
	const uint8 Launched = static_cast<uint8>(EGSTN_JumpPhase::Launched);
	int32 Events = 0;
	if (CurrentPhase == Grounded)
	{
		Events |= static_cast<int32>(EGSTN_AirSpinRefresh::Land);
	}
	// The edge only: a launch refreshes once, so an air spin used while still rising from it stays spent
	if (CurrentPhase == Launched && PreviousPhase != Launched)
	{
		Events |= static_cast<int32>(EGSTN_AirSpinRefresh::Trampoline);
	}
	return Events;
}

FVector UGSTN_DrillMath::ComputeHopVelocity(const FVector& Prior, const FVector& Up, float HopSpeed, bool bCancelFall)
{
	const FVector UpDir = Up.GetSafeNormal();
	if (UpDir.IsZero())
	{
		return Prior;
	}
	const double Along = Prior.Dot(UpDir);
	const FVector Across = Prior - UpDir * Along;
	const double Hop = FMath::Max(0.f, HopSpeed);
	const double NewAlong = bCancelFall ? FMath::Max(Along, Hop) : Along + Hop;
	return Across + UpDir * NewAlong;
}

FVector UGSTN_DrillMath::ComputeLaunchVelocity(EGSTN_DrillLaunchDirection Direction, const FVector& SurfaceNormal, const FVector& Gravity, float Speed)
{
	const FVector Normal = SurfaceNormal.GetSafeNormal();
	const FVector GravityUp = -Gravity.GetSafeNormal();
	const FVector& Preferred = Direction == EGSTN_DrillLaunchDirection::SurfaceNormal ? Normal : GravityUp;
	const FVector& Fallback = Direction == EGSTN_DrillLaunchDirection::SurfaceNormal ? GravityUp : Normal;
	return (Preferred.IsZero() ? Fallback : Preferred) * Speed;
}

FVector UGSTN_DrillMath::ComputeDiveVelocity(const FVector& DiveDirection, const FVector& SteerInput, float PlungeSpeed, float SteerFraction)
{
	const FVector Along = DiveDirection.GetSafeNormal();
	if (Along.IsZero())
	{
		return FVector::ZeroVector;
	}
	const FVector Lateral = (SteerInput - Along * FVector::DotProduct(SteerInput, Along)).GetClampedToMaxSize(1.0);
	return Along * PlungeSpeed + Lateral * (PlungeSpeed * SteerFraction);
}

FGSTN_DrillWobble UGSTN_DrillMath::ComputeWobble(int32 FramesSinceLatch, int32 SeatFrames, float MaxWobbleDegrees, float PrecessionHz,
	float FixedTickHz, const TArray<float>& AmplitudeSamples)
{
	FGSTN_DrillWobble Result;
	const int32 Frames = FMath::Max(0, FramesSinceLatch);
	if (FixedTickHz > 0.f)
	{
		Result.PhaseDegrees = FMath::Fmod(360.f * PrecessionHz * static_cast<float>(Frames) / FixedTickHz, 360.f);
	}
	if (SeatFrames <= 0 || Frames >= SeatFrames)
	{
		return Result;
	}

	const float Alpha = static_cast<float>(Frames) / static_cast<float>(SeatFrames);
	float Normalised = 1.f - Alpha;
	if (AmplitudeSamples.Num() == 1)
	{
		Normalised = AmplitudeSamples[0];
	}
	else if (AmplitudeSamples.Num() > 1)
	{
		const float Position = Alpha * static_cast<float>(AmplitudeSamples.Num() - 1);
		const int32 Index = FMath::Min(FMath::FloorToInt(Position), AmplitudeSamples.Num() - 2);
		Normalised = FMath::Lerp(AmplitudeSamples[Index], AmplitudeSamples[Index + 1], Position - static_cast<float>(Index));
	}
	Result.AmplitudeDegrees = MaxWobbleDegrees * FMath::Clamp(Normalised, 0.f, 1.f);
	return Result;
}

void UGSTN_DrillMath::SampleWobbleCurve(const UCurveFloat* Curve, int32 Count, TArray<float>& OutSamples)
{
	OutSamples.Reset();
	if (Curve == nullptr)
	{
		return;
	}
	const int32 Samples = FMath::Max(2, Count);
	OutSamples.Reserve(Samples);
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		OutSamples.Add(Curve->GetFloatValue(static_cast<float>(Index) / static_cast<float>(Samples - 1)));
	}
}

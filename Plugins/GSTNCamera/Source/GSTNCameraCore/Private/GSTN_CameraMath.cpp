#include "GSTN_CameraMath.h"

namespace GSTNCameraMathPrivate
{
	constexpr float Tiny = 1.0e-6f;

	static FVector SafeUp(const FVector& Up)
	{
		const FVector Normal = Up.GetSafeNormal();
		return Normal.IsNearlyZero() ? FVector::UpVector : Normal;
	}

	/** A unit vector perpendicular to V, deterministic */
	static FVector AnyPerpendicular(const FVector& V)
	{
		const FVector Axis = FMath::Abs(V.X) < 0.9 ? FVector::XAxisVector : FVector::YAxisVector;
		return FVector::CrossProduct(V, Axis).GetSafeNormal();
	}
}

using namespace GSTNCameraMathPrivate;

float FGSTN_CameraMath::ShortestAngleDelta(float FromDeg, float ToDeg)
{
	float Delta = FMath::Fmod(ToDeg - FromDeg, 360.f);
	if (Delta > 180.f)
	{
		Delta -= 360.f;
	}
	else if (Delta <= -180.f)
	{
		Delta += 360.f;
	}
	return Delta;
}

float FGSTN_CameraMath::StepAngleToward(float CurrentDeg, float TargetDeg, float MaxDeltaDeg)
{
	const float Delta = ShortestAngleDelta(CurrentDeg, TargetDeg);
	return CurrentDeg + FMath::Clamp(Delta, -FMath::Abs(MaxDeltaDeg), FMath::Abs(MaxDeltaDeg));
}

FVector FGSTN_CameraMath::ProjectOnUpPlane(const FVector& Vector, const FVector& Up)
{
	const FVector U = SafeUp(Up);
	const FVector Flat = Vector - U * FVector::DotProduct(Vector, U);
	return Flat.SizeSquared() < Tiny ? FVector::ZeroVector : Flat.GetSafeNormal();
}

FVector FGSTN_CameraMath::ReferenceForward(const FVector& Up)
{
	const FVector Reference = ProjectOnUpPlane(FVector::XAxisVector, Up);
	return Reference.IsNearlyZero() ? ProjectOnUpPlane(FVector::YAxisVector, Up) : Reference;
}

FGSTN_CameraFrame FGSTN_CameraMath::MakeFrame(const FVector& Up, const FVector& PreferredForward)
{
	FGSTN_CameraFrame Frame;
	Frame.Up = SafeUp(Up);
	const FVector Flat = ProjectOnUpPlane(PreferredForward, Frame.Up);
	Frame.RefForward = Flat.IsNearlyZero() ? ReferenceForward(Frame.Up) : Flat;
	return Frame;
}

FVector FGSTN_CameraMath::HeadingToDirection(const FGSTN_CameraFrame& Frame, float HeadingDeg)
{
	return Frame.RefForward.RotateAngleAxis(HeadingDeg, SafeUp(Frame.Up)).GetSafeNormal();
}

float FGSTN_CameraMath::DirectionToHeading(const FGSTN_CameraFrame& Frame, const FVector& Direction)
{
	const FVector Up = SafeUp(Frame.Up);
	const FVector Flat = ProjectOnUpPlane(Direction, Up);
	if (Flat.IsNearlyZero())
	{
		return 0.f;
	}
	const FVector Side = FVector::CrossProduct(Up, Frame.RefForward);
	return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Flat, Side), FVector::DotProduct(Flat, Frame.RefForward)));
}

float FGSTN_CameraMath::DirectionToPitch(const FVector& Up, const FVector& Direction)
{
	return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FVector::DotProduct(Direction.GetSafeNormal(), SafeUp(Up)), -1.0, 1.0)));
}

FVector FGSTN_CameraMath::StepUp(const FVector& CurrentUp, const FVector& TargetUp, float MaxDegPerSecond, float DeltaSeconds)
{
	const FVector Current = SafeUp(CurrentUp);
	const FVector Target = SafeUp(TargetUp);
	const double Cos = FMath::Clamp(FVector::DotProduct(Current, Target), -1.0, 1.0);
	const double AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Cos));
	if (AngleDeg < 1.0e-4)
	{
		return Target;
	}
	const double Step = FMath::Max(0.f, MaxDegPerSecond) * FMath::Max(0.f, DeltaSeconds);
	if (Step >= AngleDeg)
	{
		return Target;
	}
	// Opposite vectors have no shortest axis: any perpendicular is deterministic and continuous from then on, because the
	// next step is no longer opposite
	FVector Axis = FVector::CrossProduct(Current, Target);
	Axis = Axis.SizeSquared() < 1.0e-12 ? AnyPerpendicular(Current) : Axis.GetSafeNormal();
	return Current.RotateAngleAxis(Step, Axis).GetSafeNormal();
}

FVector FGSTN_CameraMath::TransportForward(const FVector& OldUp, const FVector& NewUp, const FVector& Forward)
{
	const FVector From = SafeUp(OldUp);
	const FVector To = SafeUp(NewUp);
	FVector Carried = Forward;
	const double Cos = FVector::DotProduct(From, To);
	if (Cos < -0.999999)
	{
		// A 180 degree flip in one call: rotate about a deterministic axis perpendicular to Up, like StepUp's first step would
		Carried = Forward.RotateAngleAxis(180.f, AnyPerpendicular(From));
	}
	else
	{
		Carried = FQuat::FindBetweenNormals(From, To).RotateVector(Forward);
	}
	const FVector Flat = ProjectOnUpPlane(Carried, To);
	return Flat.IsNearlyZero() ? ReferenceForward(To) : Flat;
}

float FGSTN_CameraMath::BlendAlpha(EGSTN_CameraBlendFunction Function, float Progress, float Exponent)
{
	const float T = FMath::Clamp(Progress, 0.f, 1.f);
	const float E = FMath::Max(1.f, Exponent);
	switch (Function)
	{
	case EGSTN_CameraBlendFunction::EaseIn:
		return FMath::Pow(T, E);
	case EGSTN_CameraBlendFunction::EaseOut:
		return 1.f - FMath::Pow(1.f - T, E);
	case EGSTN_CameraBlendFunction::EaseInOut:
		return T < 0.5f ? 0.5f * FMath::Pow(2.f * T, E) : 1.f - 0.5f * FMath::Pow(2.f * (1.f - T), E);
	default:
		return T;
	}
}

FGSTN_CameraView FGSTN_CameraMath::BlendView(const FGSTN_CameraView& A, const FGSTN_CameraView& B, float Alpha)
{
	const float W = FMath::Clamp(Alpha, 0.f, 1.f);
	FGSTN_CameraView Out;
	Out.Pivot = FMath::Lerp(A.Pivot, B.Pivot, static_cast<double>(W));
	Out.HeadingDeg = A.HeadingDeg + ShortestAngleDelta(A.HeadingDeg, B.HeadingDeg) * W;
	Out.PitchDeg = FMath::Lerp(A.PitchDeg, B.PitchDeg, W);
	Out.ArmCm = FMath::Lerp(A.ArmCm, B.ArmCm, W);
	Out.SideOffsetCm = FMath::Lerp(A.SideOffsetCm, B.SideOffsetCm, W);
	Out.UpOffsetCm = FMath::Lerp(A.UpOffsetCm, B.UpOffsetCm, W);
	Out.FovDeg = FMath::Lerp(A.FovDeg, B.FovDeg, W);
	return Out;
}

FGSTN_CameraPose FGSTN_CameraMath::ComputePose(const FGSTN_CameraFrame& Frame, const FGSTN_CameraView& View)
{
	const FVector Up = SafeUp(Frame.Up);
	const FVector Heading = HeadingToDirection(Frame, View.HeadingDeg);
	const FVector Right = FVector::CrossProduct(Up, Heading).GetSafeNormal();
	const double PitchRad = FMath::DegreesToRadians(FMath::Clamp(View.PitchDeg, -89.9f, 89.9f));
	const double CosP = FMath::Cos(PitchRad);
	const double SinP = FMath::Sin(PitchRad);

	FGSTN_CameraPose Pose;
	Pose.Forward = (Heading * CosP + Up * SinP).GetSafeNormal();
	Pose.Up = (Up * CosP - Heading * SinP).GetSafeNormal();
	const FVector EffectivePivot = View.Pivot + Right * View.SideOffsetCm + Up * View.UpOffsetCm;
	Pose.Location = EffectivePivot - Pose.Forward * View.ArmCm;
	return Pose;
}

FGSTN_CameraView FGSTN_CameraMath::ViewLookingAt(const FGSTN_CameraFrame& Frame, const FVector& CameraLocation, const FVector& Target, float FovDeg)
{
	const FVector ToTarget = Target - CameraLocation;
	const double Distance = ToTarget.Size();
	FGSTN_CameraView View;
	View.Pivot = Target;
	View.FovDeg = FovDeg;
	View.ArmCm = static_cast<float>(Distance);
	if (Distance > 1.0e-3)
	{
		const FVector Direction = ToTarget / Distance;
		View.PitchDeg = DirectionToPitch(Frame.Up, Direction);
		View.HeadingDeg = DirectionToHeading(Frame, Direction);
	}
	return View;
}

FGSTN_CameraOrbitState FGSTN_CameraMath::ClampOrbitAroundDesired(const FGSTN_CameraOrbitState& Orbit, float DesiredYawDeg, float DesiredPitchDeg, const FGSTN_CameraOrbitLimits& Limits)
{
	FGSTN_CameraOrbitState Out = Orbit;
	const float YawOffset = FMath::Clamp(ShortestAngleDelta(DesiredYawDeg, Orbit.YawDeg), -Limits.YawLimitDeg, Limits.YawLimitDeg);
	Out.YawDeg = DesiredYawDeg + YawOffset;
	const float PitchMin = FMath::Min(Limits.PitchMinOffsetDeg, Limits.PitchMaxOffsetDeg);
	const float PitchMax = FMath::Max(Limits.PitchMinOffsetDeg, Limits.PitchMaxOffsetDeg);
	Out.PitchDeg = DesiredPitchDeg + FMath::Clamp(Orbit.PitchDeg - DesiredPitchDeg, PitchMin, PitchMax);
	const float Scale = FMath::Max(1.f, Limits.DistanceScaleLimit);
	Out.DistanceScale = FMath::Clamp(Orbit.DistanceScale, 1.f / Scale, Scale);
	return Out;
}

bool FGSTN_CameraMath::StepRecenter(FGSTN_CameraOrbitState& InOutOrbit, float DesiredYawDeg, float DesiredPitchDeg, float SecondsSinceInput, float DelaySeconds, float DegPerSecond, float DeltaSeconds)
{
	const bool bOnTarget = FMath::IsNearlyZero(ShortestAngleDelta(InOutOrbit.YawDeg, DesiredYawDeg), 0.01f)
		&& FMath::IsNearlyEqual(InOutOrbit.PitchDeg, DesiredPitchDeg, 0.01f)
		&& FMath::IsNearlyEqual(InOutOrbit.DistanceScale, 1.f, 0.001f);
	if (bOnTarget)
	{
		return true;
	}
	if (DelaySeconds <= 0.f || SecondsSinceInput < DelaySeconds)
	{
		return false;
	}
	const float Step = FMath::Max(0.f, DegPerSecond) * FMath::Max(0.f, DeltaSeconds);
	InOutOrbit.YawDeg = StepAngleToward(InOutOrbit.YawDeg, DesiredYawDeg, Step);
	InOutOrbit.PitchDeg += FMath::Clamp(DesiredPitchDeg - InOutOrbit.PitchDeg, -Step, Step);
	// Zoom: 1 scale unit counts as 100 degrees so it settles with the angles
	InOutOrbit.DistanceScale += FMath::Clamp(1.f - InOutOrbit.DistanceScale, -Step * 0.01f, Step * 0.01f);
	return false;
}

bool FGSTN_CameraMath::ProjectOnPolyline(const TArray<FVector>& Points, const FVector& Point, FVector& OutPoint, float& OutDistanceAlongCm, int32* OutSegment)
{
	if (Points.Num() < 2)
	{
		return false;
	}
	double BestDistSq = TNumericLimits<double>::Max();
	double Along = 0.0;
	double BestAlong = 0.0;
	int32 BestSegment = 0;
	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
	{
		const FVector& Start = Points[Index];
		const FVector Segment = Points[Index + 1] - Start;
		const double LengthSq = Segment.SizeSquared();
		const double T = LengthSq > 1.0e-9 ? FMath::Clamp(FVector::DotProduct(Point - Start, Segment) / LengthSq, 0.0, 1.0) : 0.0;
		const FVector Candidate = Start + Segment * T;
		const double DistSq = FVector::DistSquared(Candidate, Point);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutPoint = Candidate;
			BestAlong = Along + FMath::Sqrt(LengthSq) * T;
			BestSegment = Index;
		}
		Along += FMath::Sqrt(LengthSq);
	}
	OutDistanceAlongCm = static_cast<float>(BestAlong);
	if (OutSegment)
	{
		*OutSegment = BestSegment;
	}
	return true;
}

int32 FGSTN_CameraMath::ChooseSection(const TArray<FGSTN_CameraSectionCandidate>& Candidates)
{
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (Best == INDEX_NONE)
		{
			Best = Index;
			continue;
		}
		const FGSTN_CameraSectionCandidate& A = Candidates[Index];
		const FGSTN_CameraSectionCandidate& B = Candidates[Best];
		bool bBetter;
		if (A.Priority != B.Priority)
		{
			bBetter = A.Priority > B.Priority;
		}
		else if (!FMath::IsNearlyEqual(A.EnterTime, B.EnterTime))
		{
			bBetter = A.EnterTime > B.EnterTime;
		}
		else if (!FMath::IsNearlyEqual(A.VolumeSize, B.VolumeSize))
		{
			bBetter = A.VolumeSize < B.VolumeSize;
		}
		else
		{
			bBetter = A.Id < B.Id;
		}
		if (bBetter)
		{
			Best = Index;
		}
	}
	return Best;
}

int32 FGSTN_CameraMath::StepRuleChoice(FGSTN_CameraRuleChoiceState& State, const TArray<FGSTN_CameraRuleEval>& Rules, float DeltaSeconds, float MinDwellSeconds)
{
	constexpr int32 NoPending = -2;
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		if (Rules[Index].bMatches && (Best == INDEX_NONE || Rules[Index].Priority > Rules[Best].Priority))
		{
			Best = Index;
		}
	}

	if (!Rules.IsValidIndex(State.CurrentRule))
	{
		State.CurrentRule = Best;
		State.DwellSeconds = 0.f;
		State.PendingRule = NoPending;
		State.PendingSeconds = 0.f;
		return State.CurrentRule;
	}

	State.DwellSeconds += DeltaSeconds;
	if (Best == State.CurrentRule)
	{
		State.PendingRule = NoPending;
		State.PendingSeconds = 0.f;
		return State.CurrentRule;
	}

	if (State.PendingRule != Best)
	{
		State.PendingRule = Best;
		State.PendingSeconds = 0.f;
	}
	State.PendingSeconds += DeltaSeconds;

	// Leaving a rule that still matches means a better one appeared: the newcomer's enter delay. Leaving one that stopped matching: its exit delay
	const FGSTN_CameraRuleEval& Current = Rules[State.CurrentRule];
	const float Delay = Current.bMatches ? (Rules.IsValidIndex(Best) ? Rules[Best].EnterDelaySeconds : 0.f) : Current.ExitDelaySeconds;
	if (State.PendingSeconds >= Delay && State.DwellSeconds >= MinDwellSeconds)
	{
		State.CurrentRule = Best;
		State.DwellSeconds = 0.f;
		State.PendingRule = NoPending;
		State.PendingSeconds = 0.f;
	}
	return State.CurrentRule;
}

FVector FGSTN_CameraMath::ConvergedAimDirection(const FVector& ViewOrigin, const FVector& ViewForward, const FVector& AimOrigin, bool bHit, const FVector& HitPoint, float ConvergeMinCm, float ConvergeMaxCm)
{
	const FVector Forward = ViewForward.GetSafeNormal();
	if (!bHit)
	{
		return Forward;
	}
	const double Along = FVector::DotProduct(HitPoint - ViewOrigin, Forward);
	if (Along > ConvergeMaxCm)
	{
		return Forward;
	}
	// The trace starts at the minimum distance, so a nearer hit is clamped there: keeps the aim line from crossing the view axis behind the character
	const FVector Target = Along < ConvergeMinCm ? ViewOrigin + Forward * ConvergeMinCm : HitPoint;
	const FVector Direction = (Target - AimOrigin).GetSafeNormal();
	return Direction.IsNearlyZero() ? Forward : Direction;
}

bool FGSTN_CameraMath::ShouldRaiseFrameChange(const FVector& Up, const FVector& FromForward, const FVector& ToForward, float MinYawDeltaDeg, bool bManualOrbit, float* OutDeltaDeg)
{
	if (OutDeltaDeg)
	{
		*OutDeltaDeg = 0.f;
	}
	const FVector From = ProjectOnUpPlane(FromForward, Up);
	const FVector To = ProjectOnUpPlane(ToForward, Up);
	if (From.IsNearlyZero() || To.IsNearlyZero())
	{
		return false;
	}
	const float Delta = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(From, To), -1.0, 1.0)));
	if (OutDeltaDeg)
	{
		*OutDeltaDeg = Delta;
	}
	return !bManualOrbit && Delta >= MinYawDeltaDeg;
}

float FGSTN_CameraMath::StepGroundHeight(float GroundHeight, float CurrentHeight, bool bGrounded, float JumpDeadZoneUpCm, float FallFollowCm)
{
	if (bGrounded)
	{
		return CurrentHeight;
	}
	if (CurrentHeight < GroundHeight - FallFollowCm)
	{
		return CurrentHeight + FallFollowCm;
	}
	if (CurrentHeight > GroundHeight + JumpDeadZoneUpCm)
	{
		return CurrentHeight - JumpDeadZoneUpCm;
	}
	return GroundHeight;
}

bool FGSTN_CameraMath::DuelDirection(const FVector& A, const FVector& B, const FVector& Up, const FVector& CurrentForward, bool bAOnLeft, float MinAxisCm, FVector& OutDirection)
{
	const FVector U = SafeUp(Up);
	const FVector Axis = (B - A) - U * FVector::DotProduct(B - A, U);
	if (Axis.Size() < FMath::Max(MinAxisCm, 1.0e-3f))
	{
		OutDirection = CurrentForward;
		return false;
	}
	// Looking along Up x Axis puts B on the left of the screen (A on the right); the opposite puts A on the left
	const FVector Perp = FVector::CrossProduct(U, Axis).GetSafeNormal();
	const FVector Flat = ProjectOnUpPlane(CurrentForward, U);
	if (Flat.IsNearlyZero())
	{
		OutDirection = bAOnLeft ? -Perp : Perp;
	}
	else
	{
		OutDirection = FVector::DotProduct(Perp, Flat) >= 0.0 ? Perp : -Perp;
	}
	return true;
}

float FGSTN_CameraMath::DuelArm(float SeparationCm, float VerticalExtentCm, float MarginCm, float HorizontalFovDeg, float AspectRatio, float MinArmCm, float MaxArmCm)
{
	const float HalfTan = FMath::Max(0.01f, FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(HorizontalFovDeg, 5.f, 170.f) * 0.5f)));
	const float HalfVerticalTan = HalfTan / FMath::Max(0.1f, AspectRatio);
	const float ForWidth = (SeparationCm * 0.5f + MarginCm) / HalfTan;
	const float ForHeight = (VerticalExtentCm * 0.5f + MarginCm) / HalfVerticalTan;
	return FMath::Clamp(FMath::Max(ForWidth, ForHeight), FMath::Min(MinArmCm, MaxArmCm), FMath::Max(MinArmCm, MaxArmCm));
}

bool FGSTN_CameraMath::StepDuelCamera(FGSTN_DuelCameraState& State, const FGSTN_CameraFrame& Frame, const FVector& A, const FVector& B, bool bAOnLeft, bool bBothGrounded,
	float MinSeparationCm, float SideSwapDelaySeconds, float SideSwapMaxDelaySeconds, float MaxYawRateDegPerSecond, float DeltaSeconds)
{
	const FVector U = SafeUp(Frame.Up);
	const FVector Axis = (B - A) - U * FVector::DotProduct(B - A, U);
	State.bFrozen = Axis.Size() < FMath::Max(MinSeparationCm, 1.0e-3f);
	if (!State.bFrozen)
	{
		const FVector Line = Axis.GetSafeNormal();
		// Looking along Up x Line puts B on the left of the screen (A on the right); the opposite puts A on the left
		const FVector Perp = FVector::CrossProduct(U, Line).GetSafeNormal();
		const double Wanted = bAOnLeft ? -1.0 : 1.0;
		if (!State.bInitialised)
		{
			State.Held = Perp * Wanted;
			State.HeadingDeg = DirectionToHeading(Frame, State.Held);
			State.bInitialised = true;
		}
		else
		{
			// The line may turn (or reverse when a fighter passes the other); the camera stays on the side it was on
			const FVector Flat = ProjectOnUpPlane(State.Held, U);
			const double Sign = (Flat.IsNearlyZero() ? Wanted : (FVector::DotProduct(Perp, Flat) >= 0.0 ? 1.0 : -1.0));
			bool bSwap = false;
			if (Sign != Wanted)
			{
				State.CrossedSeconds += DeltaSeconds;
				State.StableCrossedSeconds = bBothGrounded ? State.StableCrossedSeconds + DeltaSeconds : 0.f;
				bSwap = State.StableCrossedSeconds >= SideSwapDelaySeconds || State.CrossedSeconds >= SideSwapMaxDelaySeconds;
			}
			else
			{
				State.CrossedSeconds = 0.f;
				State.StableCrossedSeconds = 0.f;
			}
			if (bSwap)
			{
				State.CrossedSeconds = 0.f;
				State.StableCrossedSeconds = 0.f;
			}
			State.Held = Perp * (bSwap ? Wanted : Sign);
		}
	}
	if (!State.bInitialised)
	{
		return false;
	}
	State.HeadingDeg = StepAngleToward(State.HeadingDeg, DirectionToHeading(Frame, State.Held), MaxYawRateDegPerSecond * DeltaSeconds);
	return true;
}

float FGSTN_CameraMath::StepShoulderBlend(float Current, EGSTN_ShoulderSide TargetSide, float BlendSeconds, float DeltaSeconds)
{
	const float Target = TargetSide == EGSTN_ShoulderSide::Right ? 1.f : -1.f;
	if (BlendSeconds <= UE_SMALL_NUMBER)
	{
		return Target;
	}
	const float Step = 2.f * DeltaSeconds / BlendSeconds;
	return Current < Target ? FMath::Min(Current + Step, Target) : FMath::Max(Current - Step, Target);
}

EGSTN_CameraGroupFraming FGSTN_CameraMath::ChooseGroupFraming(int32 OpponentCount)
{
	return OpponentCount <= 0 ? EGSTN_CameraGroupFraming::None : (OpponentCount == 1 ? EGSTN_CameraGroupFraming::Duel : EGSTN_CameraGroupFraming::Arena);
}

float FGSTN_CameraMath::ArenaPitchForArm(const FGSTN_CameraArenaTuning& Tuning, float ArmCm)
{
	const float MinArm = FMath::Min(Tuning.MinArmCm, Tuning.MaxArmCm);
	const float MaxArm = FMath::Max(Tuning.MinArmCm, Tuning.MaxArmCm);
	const float Span = FMath::Max(MaxArm - MinArm, 1.0e-3f);
	return Tuning.PitchDeg - Tuning.MaxExtraPitchDeg * FMath::Clamp((ArmCm - MinArm) / Span, 0.f, 1.f);
}

float FGSTN_CameraMath::StepArenaZoom(float CurrentArmCm, float WantedArmCm, float ZoomInSeconds, float ZoomOutSeconds, float DeltaSeconds)
{
	const float Seconds = WantedArmCm > CurrentArmCm ? ZoomOutSeconds : ZoomInSeconds;
	if (Seconds <= UE_SMALL_NUMBER)
	{
		return WantedArmCm;
	}
	return CurrentArmCm + (WantedArmCm - CurrentArmCm) * (1.f - FMath::Exp(-FMath::Max(DeltaSeconds, 0.f) / Seconds));
}

FGSTN_ArenaFraming FGSTN_CameraMath::ArenaFrameAll(const TArray<FVector>& Fighters, const FGSTN_CameraFrame& Frame, float HeadingDeg, const FGSTN_CameraArenaTuning& Tuning, float AspectRatio, const FBox& Bounds, const FVector* FocusOverride)
{
	FGSTN_ArenaFraming Out;
	if (Fighters.IsEmpty())
	{
		return Out;
	}
	Out.bValid = true;
	Out.HeadingDeg = HeadingDeg;

	// Weighted centroid: the first fighter is the local pawn
	FVector Sum = FVector::ZeroVector;
	double TotalWeight = 0.0;
	for (int32 Index = 0; Index < Fighters.Num(); ++Index)
	{
		const double Weight = Index == 0 ? FMath::Max(Tuning.LocalPlayerWeight, 0.01f) : 1.0;
		Sum += Fighters[Index] * Weight;
		TotalWeight += Weight;
	}
	Out.Focus = Sum / TotalWeight;
	if (Bounds.IsValid)
	{
		Out.Focus = FVector(FMath::Clamp(Out.Focus.X, Bounds.Min.X, Bounds.Max.X), FMath::Clamp(Out.Focus.Y, Bounds.Min.Y, Bounds.Max.Y), FMath::Clamp(Out.Focus.Z, Bounds.Min.Z, Bounds.Max.Z));
	}
	const FVector FitFocus = FocusOverride ? *FocusOverride : Out.Focus;

	const float HalfTan = FMath::Max(0.01f, FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Tuning.FovDeg, 5.f, 170.f) * 0.5f)));
	const float HalfVerticalTan = HalfTan / FMath::Max(0.1f, AspectRatio);
	const float Inner = 1.f - FMath::Clamp(Tuning.ScreenMargin, 0.f, 0.8f);
	const float MinArm = FMath::Min(Tuning.MinArmCm, Tuning.MaxArmCm);
	const float MaxArm = FMath::Max(Tuning.MinArmCm, Tuning.MaxArmCm);

	// Exact test with the real pose (pitch and depths matter): the arm grows until every fighter sphere is inside the shrunk frustum
	auto Fits = [&](float Arm)
	{
		FGSTN_CameraView View;
		View.Pivot = FitFocus;
		View.HeadingDeg = HeadingDeg;
		View.PitchDeg = ArenaPitchForArm(Tuning, Arm);
		View.ArmCm = Arm;
		const FGSTN_CameraPose Pose = ComputePose(Frame, View);
		const FVector Right = FVector::CrossProduct(Pose.Up, Pose.Forward).GetSafeNormal();
		for (const FVector& Point : Fighters)
		{
			const FVector Local = Point - Pose.Location;
			const double Depth = FVector::DotProduct(Local, Pose.Forward);
			if (Depth <= 1.0)
			{
				return false;
			}
			if (FMath::Abs(FVector::DotProduct(Local, Right)) + Tuning.FighterRadiusCm > Inner * HalfTan * Depth
				|| FMath::Abs(FVector::DotProduct(Local, Pose.Up)) + Tuning.FighterRadiusCm > Inner * HalfVerticalTan * Depth)
			{
				return false;
			}
		}
		return true;
	};
	float Arm = MinArm;
	for (int32 Iteration = 0; Iteration < 120 && Arm < MaxArm && !Fits(Arm); ++Iteration)
	{
		Arm = FMath::Min(Arm * 1.04f + 5.f, MaxArm);
	}
	Out.ArmCm = Arm;
	Out.PitchDeg = ArenaPitchForArm(Tuning, Arm);
	return Out;
}

EGSTN_CameraOcclusionAction FGSTN_CameraMath::DecideOcclusion(EGSTN_CameraOcclusionPolicy Policy, bool bFadeable, float BlockerDistanceFromCameraCm, float NearClipSafeCm)
{
	switch (Policy)
	{
	case EGSTN_CameraOcclusionPolicy::PullIn:
		return EGSTN_CameraOcclusionAction::PullIn;
	case EGSTN_CameraOcclusionPolicy::Fade:
		return bFadeable ? EGSTN_CameraOcclusionAction::Fade : EGSTN_CameraOcclusionAction::None;
	default:
		if (bFadeable)
		{
			return EGSTN_CameraOcclusionAction::Fade;
		}
		return BlockerDistanceFromCameraCm <= NearClipSafeCm ? EGSTN_CameraOcclusionAction::PullIn : EGSTN_CameraOcclusionAction::None;
	}
}

// ---- FGSTN_CameraModeStack ----

int32 FGSTN_CameraModeStack::IndexOf(int32 Key) const
{
	return Entries.IndexOfByPredicate([Key](const FGSTN_CameraStackEntry& Entry) { return Entry.Key == Key; });
}

void FGSTN_CameraModeStack::Request(int32 Key, const FGSTN_CameraBlendTuning& Blend, int32 MaxDepth, TArray<int32>* OutRemoved)
{
	if (Entries.IsEmpty())
	{
		FGSTN_CameraStackEntry& First = Entries.AddDefaulted_GetRef();
		First.Key = Key;
		First.Progress = 1.f;
		First.Blend = Blend;
		return;
	}
	if (TopKey() == Key)
	{
		return;
	}
	const int32 Existing = IndexOf(Key);
	if (Existing != INDEX_NONE)
	{
		// Promote keeping the progress; the blend becomes the one of the new request
		FGSTN_CameraStackEntry Entry = Entries[Existing];
		Entry.Blend = Blend;
		Entries.RemoveAt(Existing);
		Entries.Add(Entry);
	}
	else
	{
		FGSTN_CameraStackEntry& Added = Entries.AddDefaulted_GetRef();
		Added.Key = Key;
		Added.Progress = 0.f;
		Added.Blend = Blend;
	}
	while (Entries.Num() > FMath::Max(1, MaxDepth))
	{
		if (OutRemoved)
		{
			OutRemoved->Add(Entries[0].Key);
		}
		Entries.RemoveAt(0);
	}
}

void FGSTN_CameraModeStack::Advance(float DeltaSeconds, TArray<int32>* OutRemoved)
{
	if (Entries.IsEmpty())
	{
		return;
	}
	FGSTN_CameraStackEntry& Top = Entries.Last();
	Top.Progress = Top.Blend.BlendSeconds <= UE_SMALL_NUMBER ? 1.f : FMath::Min(1.f, Top.Progress + DeltaSeconds / Top.Blend.BlendSeconds);
	if (Top.Progress >= 1.f)
	{
		while (Entries.Num() > 1)
		{
			if (OutRemoved)
			{
				OutRemoved->Add(Entries[0].Key);
			}
			Entries.RemoveAt(0);
		}
	}
}

float FGSTN_CameraModeStack::WeightOf(int32 Index) const
{
	if (!Entries.IsValidIndex(Index))
	{
		return 0.f;
	}
	if (Index == 0)
	{
		return 1.f;
	}
	const FGSTN_CameraStackEntry& Entry = Entries[Index];
	return FGSTN_CameraMath::BlendAlpha(Entry.Blend.Function, Entry.Progress, Entry.Blend.Exponent);
}

void FGSTN_CameraModeStack::Cut(TArray<int32>* OutRemoved)
{
	while (Entries.Num() > 1)
	{
		if (OutRemoved)
		{
			OutRemoved->Add(Entries[0].Key);
		}
		Entries.RemoveAt(0);
	}
	if (!Entries.IsEmpty())
	{
		Entries[0].Progress = 1.f;
	}
}

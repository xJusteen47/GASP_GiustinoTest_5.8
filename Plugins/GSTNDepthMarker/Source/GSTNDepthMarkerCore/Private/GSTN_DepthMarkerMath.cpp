// Depth marker: pure target selection maths (depth-marker.md §2.3-§2.7, §3).

#include "GSTN_DepthMarkerMath.h"

#include "Algo/Sort.h"

namespace GSTNDepthMarkerPrivate
{
	/** Angle in degrees between two unit vectors */
	static float AngleDegrees(const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(A, B)), -1.f, 1.f)));
	}

	/** A surface target from a plane (point and normal) and the subject's column. The plane's local-space copy rides along */
	static FGSTN_DepthTarget MakeSurfaceTarget(const FVector& Feet, const FVector& Up, const FVector& PlanePoint, const FVector& PlaneNormalIn,
		int64 Key, const FTransform& ComponentTransform, const FGSTN_DepthSelectorParams& Params, EGSTN_DepthMarkerState State, float EdgeAlpha)
	{
		FVector Normal = PlaneNormalIn.GetSafeNormal();
		if (FVector::DotProduct(Normal, Up) < 0.0)
		{
			Normal = -Normal;
		}

		FVector Point;
		float S = 0.f;
		if (!FGSTN_DepthMarkerMath::ProjectOntoPlane(Feet, Up, PlanePoint, Normal, Point, S))
		{
			Point = PlanePoint;
			S = -static_cast<float>(FVector::DotProduct(Feet - PlanePoint, Up));
		}

		FGSTN_DepthTarget Target;
		Target.Kind = EGSTN_DepthTargetKind::Surface;
		Target.State = State;
		Target.Point = Point;
		Target.Normal = Normal;
		Target.PresentNormal = FGSTN_DepthMarkerMath::ClampTilt(Normal, Up, Params.MaxTiltDegrees);
		Target.Height = -S;
		Target.SurfaceKey = Key;
		Target.LocalPoint = ComponentTransform.InverseTransformPosition(Point);
		Target.LocalNormal = ComponentTransform.InverseTransformVectorNoScale(Normal);
		Target.EdgeAlpha = EdgeAlpha;
		Target.SlopeRadiusScale = FGSTN_DepthMarkerMath::SlopeRadiusScale(Normal, Up);
		Target.bSteep = FVector::DotProduct(Normal, Up) < Params.MinNormalDot;
		return Target;
	}

	/** Remembers a candidate as the selected surface */
	static void Remember(FGSTN_DepthSelectionState& State, const FGSTN_DepthCandidate& C, const FGSTN_DepthTarget& Target)
	{
		State.bHasPrevious = true;
		State.PreviousKey = C.SurfaceKey;
		State.PreviousPoint = Target.Point;
		State.PreviousNormal = Target.Normal;
		State.PreviousHeight = Target.Height;
		State.PreviousComponentTransform = C.ComponentTransform;
	}
}

void FGSTN_DepthMarkerMath::FilterCandidates(const TArray<FGSTN_DepthCandidate>& In, const FVector& Up, const FGSTN_DepthSelectorParams& Params,
	const FGameplayTagQuery& TargetBehaviours, TArray<FGSTN_DepthCandidate>& Out)
{
	Out.Reset();
	const FVector UpN = Up.GetSafeNormal();
	for (const FGSTN_DepthCandidate& C : In)
	{
		if (!C.bBlocking || C.bStartPenetrating || C.bExcluded)
		{
			continue;
		}
		// A thin platform just passed upward sits slightly above the feet: not a target beyond the tolerance
		if (C.Height < -Params.PenetrationTolerance)
		{
			continue;
		}
		// Undersides and vertical faces
		if (FVector::DotProduct(C.Normal.GetSafeNormal(), UpN) <= 0.0)
		{
			continue;
		}
		if (!TargetBehaviours.IsEmpty())
		{
			if (!C.bSurfaceKnown)
			{
				if (!Params.bUnknownSurfacesPass)
				{
					continue;
				}
			}
			else if (!TargetBehaviours.Matches(C.BehaviourTags))
			{
				continue;
			}
		}
		Out.Add(C);
	}
	Algo::SortBy(Out, &FGSTN_DepthCandidate::Height);
}

bool FGSTN_DepthMarkerMath::ChooseFirstBelow(const TArray<FGSTN_DepthCandidate>& Filtered, FGSTN_DepthCandidate& OutCandidate)
{
	if (Filtered.IsEmpty())
	{
		return false;
	}
	// Do not rely on the caller's order
	const FGSTN_DepthCandidate* Best = &Filtered[0];
	for (const FGSTN_DepthCandidate& C : Filtered)
	{
		if (C.Height < Best->Height)
		{
			Best = &C;
		}
	}
	OutCandidate = *Best;
	return true;
}

FGSTN_DepthTarget FGSTN_DepthMarkerMath::ApplyHysteresis(FGSTN_DepthSelectionState& State, bool bHasCandidate, const FGSTN_DepthCandidate& C,
	const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthSelectorParams& Params, double Time)
{
	using namespace GSTNDepthMarkerPrivate;
	const FVector Up = Subject.Up.GetSafeNormal();
	const FVector& Feet = Subject.Feet;

	auto Adopt = [&](float BlendMs, bool bFarther)
	{
		FGSTN_DepthTarget Target = MakeSurfaceTarget(Feet, Up, C.Point, C.Normal, C.SurfaceKey, C.ComponentTransform, Params,
			EGSTN_DepthMarkerState::Ground, 1.f);
		Target.BlendMs = BlendMs;
		Remember(State, C, Target);
		State.LastSwitchTime = Time;
		State.bLastSwitchWasFarther = bFarther;
		return Target;
	};

	if (!State.bHasPrevious)
	{
		return bHasCandidate ? Adopt(0.f, false) : FGSTN_DepthTarget();
	}

	// The previous surface as it is now, re-projected along the current column
	const FGSTN_DepthTarget Previous = MakeSurfaceTarget(Feet, Up, State.PreviousPoint, State.PreviousNormal, State.PreviousKey,
		State.PreviousComponentTransform, Params, EGSTN_DepthMarkerState::Ground, 1.f);
	const double DwellSeconds = Params.MinDwellMs * 0.001;
	const bool bDwelling = (Time - State.LastSwitchTime) < DwellSeconds;

	bool bNearer = false;
	if (bHasCandidate)
	{
		// R1 coplanar: the same surface, or overlapping meshes within tolerance in height and normal
		const bool bSame = C.SurfaceKey == State.PreviousKey;
		const bool bCoplanar = FMath::Abs(C.Height - Previous.Height) <= Params.CoplanarTolerance
			&& AngleDegrees(C.Normal.GetSafeNormal(), Previous.Normal) <= Params.CoplanarAngleDegrees;
		if (bSame || bCoplanar)
		{
			FGSTN_DepthTarget Target = MakeSurfaceTarget(Feet, Up, C.Point, C.Normal, State.PreviousKey, C.ComponentTransform, Params,
				EGSTN_DepthMarkerState::Ground, 1.f);
			State.PreviousPoint = Target.Point;
			State.PreviousNormal = Target.Normal;
			State.PreviousHeight = Target.Height;
			State.PreviousComponentTransform = C.ComponentTransform;
			return Target;
		}

		// R2 nearer wins at once, unless a switch to a farther surface just happened (ping-pong guard)
		bNearer = C.Height < Previous.Height - Params.CoplanarTolerance;
		if (bNearer && !(State.bLastSwitchWasFarther && bDwelling))
		{
			return Adopt(0.f, false);
		}
	}

	// R3 edge hold: the footprint still touches the previous surface within the forgiveness
	if (!bNearer && Footprint.bValid && Footprint.SurfaceKey == State.PreviousKey && Footprint.LateralCm <= Params.EdgeForgivenessCm)
	{
		const float EdgeAlpha = Params.EdgeForgivenessCm > 0.f ? 1.f - Footprint.LateralCm / Params.EdgeForgivenessCm : 1.f;
		FGSTN_DepthTarget Target = MakeSurfaceTarget(Feet, Up, Footprint.Point, State.PreviousNormal, State.PreviousKey,
			State.PreviousComponentTransform, Params, EGSTN_DepthMarkerState::EdgeHold, FMath::Clamp(EdgeAlpha, 0.f, 1.f));
		// The point is the sweep's impact point, not the column's intersection with the old plane
		Target.Point = Footprint.Point;
		Target.Height = static_cast<float>(FVector::DotProduct(Feet - Footprint.Point, Up));
		return Target;
	}

	if (bHasCandidate)
	{
		// R4 switch to a farther surface (or a nearer one held back by the ping-pong guard), once the dwell is over
		if (bDwelling)
		{
			return Previous;
		}
		return Adopt(Params.SwitchBlendMs, true);
	}

	// R5 gap: nothing left under the feet; the reference layer takes over
	State.bHasPrevious = false;
	State.PreviousKey = 0;
	return FGSTN_DepthTarget();
}

bool FGSTN_DepthMarkerMath::ProjectOntoPlane(const FVector& Feet, const FVector& Up, const FVector& PlanePoint, const FVector& PlaneNormal,
	FVector& OutPoint, float& OutS)
{
	const FVector UpN = Up.GetSafeNormal();
	const double Denominator = FVector::DotProduct(UpN, PlaneNormal);
	if (FMath::Abs(Denominator) < 1.e-4)
	{
		return false;
	}
	const double S = FVector::DotProduct(PlanePoint - Feet, PlaneNormal) / Denominator;
	OutS = static_cast<float>(S);
	OutPoint = Feet + UpN * S;
	return true;
}

FVector FGSTN_DepthMarkerMath::ClampTilt(const FVector& Normal, const FVector& Up, float MaxTiltDegrees)
{
	const FVector N = Normal.GetSafeNormal();
	const FVector UpN = Up.GetSafeNormal();
	if (GSTNDepthMarkerPrivate::AngleDegrees(N, UpN) <= MaxTiltDegrees)
	{
		return N;
	}
	const FVector Axis = FVector::CrossProduct(UpN, N).GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return UpN;
	}
	return UpN.RotateAngleAxis(MaxTiltDegrees, Axis).GetSafeNormal();
}

float FGSTN_DepthMarkerMath::SlopeRadiusScale(const FVector& Normal, const FVector& Up)
{
	const double Dot = FVector::DotProduct(Normal.GetSafeNormal(), Up.GetSafeNormal());
	return FMath::Min(2.f, 1.f / static_cast<float>(FMath::Max(Dot, 0.5)));
}

bool FGSTN_DepthMarkerMath::UpdateFrame(FGSTN_DepthSelectionState& State, const FVector& Up, const FGSTN_DepthSelectorParams& Params)
{
	const FVector UpN = Up.GetSafeNormal();
	if (!State.bHasLastUp)
	{
		State.bHasLastUp = true;
		State.LastUp = UpN;
		return false;
	}
	// A slow, continuous change (curved gravity) keeps the state; only a jump in one step resets
	const float Angle = GSTNDepthMarkerPrivate::AngleDegrees(State.LastUp, UpN);
	State.LastUp = UpN;
	if (Angle > Params.UpResetDegrees)
	{
		State.ClearSelection();
		State.GroundedSince = -1.0;
		return true;
	}
	return false;
}

void FGSTN_DepthMarkerMath::UpdateGrounded(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject,
	const FGSTN_DepthSelectorParams& Params, double Time)
{
	const bool bStatic = !Subject.bBaseMoves || Params.bUpdateFromMovingBases;
	if (!Subject.bGrounded || !bStatic)
	{
		State.GroundedSince = -1.0;
		return;
	}
	if (State.GroundedSince < 0.0)
	{
		State.GroundedSince = Time;
	}
	if (Time - State.GroundedSince >= Params.GroundedMinMs * 0.001)
	{
		State.bHasGrounded = true;
		State.LastGroundedPoint = Subject.Feet;
	}
}

bool FGSTN_DepthMarkerMath::ResolveReferencePlane(const FGSTN_DepthSelectionState& State, const FVector& Up,
	const FGSTN_DepthReferenceOverride& Override, FVector& OutPoint, FVector& OutNormal, bool& bOutHazard)
{
	if (Override.bValid)
	{
		OutPoint = Override.Point;
		OutNormal = Override.Normal.GetSafeNormal();
		bOutHazard = Override.bHazard;
		return true;
	}
	if (State.bHasGrounded)
	{
		OutPoint = State.LastGroundedPoint;
		OutNormal = Up.GetSafeNormal();
		bOutHazard = false;
		return true;
	}
	return false;
}

void FGSTN_DepthMarkerMath::ApplyReferenceLayers(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject,
	const FGSTN_DepthSelectorParams& Params, const FGSTN_DepthReferenceOverride& Override, bool bProbed, FGSTN_DepthResult& Result)
{
	const FVector Up = Subject.Up.GetSafeNormal();
	FVector PlanePoint = FVector::ZeroVector;
	FVector PlaneNormal = Up;
	bool bHazard = false;
	FVector OnPlane = FVector::ZeroVector;
	float S = 0.f;
	if (!ResolveReferencePlane(State, Up, Override, PlanePoint, PlaneNormal, bHazard)
		|| !ProjectOntoPlane(Subject.Feet, Up, PlanePoint, PlaneNormal, OnPlane, S))
	{
		State.bAboveShown = false;
		return;
	}
	if (FVector::DotProduct(PlaneNormal, Up) < 0.0)
	{
		PlaneNormal = -PlaneNormal;
	}

	// D: signed height of the feet over the plane, along Up
	const float D = -S;
	const bool bInRange = FMath::Abs(D) <= Params.ReferenceMaxDistanceCm;

	bool bShowAbove = false;
	if (Subject.bAirborne && !Subject.bGrounded && bInRange)
	{
		bShowAbove = State.bAboveShown ? (D < -Params.AboveHideDepthCm) : (D < -Params.AboveShowDepthCm);
	}
	State.bAboveShown = bShowAbove;

	auto MakeRing = [&](EGSTN_DepthMarkerState RingState, float Height)
	{
		FGSTN_DepthTarget Target;
		Target.Kind = EGSTN_DepthTargetKind::ReferencePlane;
		Target.State = RingState;
		Target.Point = OnPlane;
		Target.Normal = PlaneNormal;
		Target.PresentNormal = ClampTilt(PlaneNormal, Up, Params.MaxTiltDegrees);
		Target.Height = Height;
		Target.bHazard = bHazard;
		Target.SlopeRadiusScale = SlopeRadiusScale(PlaneNormal, Up);
		return Target;
	};

	if (bShowAbove)
	{
		Result.Above = MakeRing(EGSTN_DepthMarkerState::AboveReference, -D);
	}
	// A gap at the same level: the ring below reads as "no ground here"
	if (!Result.Below.IsValid() && bProbed && bInRange && D >= 0.f)
	{
		Result.Below = MakeRing(EGSTN_DepthMarkerState::NoGround, D);
	}
}

FGSTN_DepthResult FGSTN_DepthMarkerMath::Evaluate(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject,
	const TArray<FGSTN_DepthCandidate>& RawCandidates, const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthReferenceOverride& Override,
	const FGSTN_DepthSelectorParams& Params, const FGameplayTagQuery& TargetBehaviours, bool bProbed, double Time)
{
	FGSTN_DepthResult Result;
	Result.bGravityReset = UpdateFrame(State, Subject.Up, Params);
	Result.bStateCleared = Result.bGravityReset;
	UpdateGrounded(State, Subject, Params, Time);

	if (bProbed)
	{
		TArray<FGSTN_DepthCandidate> Filtered;
		FilterCandidates(RawCandidates, Subject.Up, Params, TargetBehaviours, Filtered);
		FGSTN_DepthCandidate Chosen;
		const bool bHas = ChooseFirstBelow(Filtered, Chosen);
		Result.Below = ApplyHysteresis(State, bHas, Chosen, Footprint, Subject, Params, Time);
	}

	ApplyReferenceLayers(State, Subject, Params, Override, bProbed, Result);
	return Result;
}

FGSTN_DepthTarget FGSTN_DepthMarkerMath::ReprojectTarget(const FGSTN_DepthTarget& Target, const FTransform& Xf, const FVector& Feet, const FVector& Up)
{
	FGSTN_DepthTarget Out = Target;
	if (Target.Kind != EGSTN_DepthTargetKind::Surface)
	{
		return Out;
	}
	const FVector UpN = Up.GetSafeNormal();
	const FVector WorldPoint = Xf.TransformPosition(Target.LocalPoint);
	const FVector WorldNormal = Xf.TransformVectorNoScale(Target.LocalNormal).GetSafeNormal();
	float S = 0.f;
	if (ProjectOntoPlane(Feet, UpN, WorldPoint, WorldNormal, Out.Point, S))
	{
		Out.Height = -S;
	}
	Out.Normal = WorldNormal;
	return Out;
}

bool FGSTN_DepthMarkerMath::PointInOrientedBox(const FVector& Point, const FTransform& BoxTransform, const FVector& HalfExtent)
{
	const FVector Local = BoxTransform.InverseTransformPositionNoScale(Point);
	return FMath::Abs(Local.X) <= HalfExtent.X && FMath::Abs(Local.Y) <= HalfExtent.Y && FMath::Abs(Local.Z) <= HalfExtent.Z;
}

int32 FGSTN_DepthMarkerMath::NumApproachProbeSlots(const FGSTN_DepthSelectorParams& Params)
{
	return Params.ApproachRingProbes + Params.ApproachVelocityProbes;
}

bool FGSTN_DepthMarkerMath::ApproachProbeSegment(const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthSelectorParams& Params, int32 Slot,
	FVector& OutStart, FVector& OutEnd)
{
	if (Slot < 0 || Slot >= NumApproachProbeSlots(Params))
	{
		return false;
	}
	const FVector Up = Subject.Up.GetSafeNormal();
	FVector Offset = FVector::ZeroVector;
	if (Slot < Params.ApproachRingProbes)
	{
		const FVector Seed = FMath::Abs(Up.Z) < 0.9 ? FVector::UpVector : FVector::RightVector;
		const FVector A = FVector::CrossProduct(Up, Seed).GetSafeNormal();
		const FVector B = FVector::CrossProduct(Up, A);
		const double Angle = 2.0 * UE_DOUBLE_PI * Slot / FMath::Max(Params.ApproachRingProbes, 1);
		Offset = (A * FMath::Cos(Angle) + B * FMath::Sin(Angle)) * Params.ApproachProbeRadiusCm;
	}
	else
	{
		// Along the horizontal velocity: where the character is heading, which a ring of fixed radius may miss
		const FVector HorizontalVelocity = Subject.Velocity - Up * FVector::DotProduct(Subject.Velocity, Up);
		if (HorizontalVelocity.Size() < Params.ApproachMinTowardSpeed)
		{
			return false;
		}
		const int32 Index = Slot - Params.ApproachRingProbes;
		Offset = HorizontalVelocity.GetSafeNormal() * Params.ApproachDistanceCm * (Index + 1) / FMath::Max(Params.ApproachVelocityProbes, 1);
	}
	OutStart = Subject.Feet + Up * Params.ApproachNearAboveFeetCm + Offset;
	OutEnd = OutStart - Up * (Params.ApproachNearAboveFeetCm + Params.ApproachProbeDepthCm);
	return true;
}

namespace GSTNDepthMarkerPrivate
{
	/** One approach candidate measured against the subject of the frame */
	struct FApproachInfo
	{
		const FGSTN_DepthPlaneCandidate* Candidate = nullptr;
		/** Distance along Up from the feet to the plane at the column: negative = the plane is below */
		float S = 0.f;
		float Edge = 0.f;
		float Toward = 0.f;
	};
}

bool FGSTN_DepthMarkerMath::EvaluateApproach(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject,
	const FGSTN_DepthSelectorParams& Params, const FGSTN_DepthTarget& Primary, const TArray<FGSTN_DepthPlaneCandidate>& Probed,
	const TArray<FGSTN_DepthPlaneCandidate>& Explicit, double Time, FGSTN_DepthTarget& OutApproach)
{
	using namespace GSTNDepthMarkerPrivate;
	OutApproach = FGSTN_DepthTarget();
	if (!Params.bApproachEnabled || !Subject.bAirborne || Subject.bSuppressed)
	{
		State.ClearApproach();
		return false;
	}

	const FVector Up = Subject.Up.GetSafeNormal();
	const FVector& Feet = Subject.Feet;
	const FVector HorizontalVelocity = Subject.Velocity - Up * FVector::DotProduct(Subject.Velocity, Up);
	const bool bHasPrimary = Primary.Kind == EGSTN_DepthTargetKind::Surface;
	// The primary plane at the column, along Up from the feet: Height is measured along Down, so S = -Height
	const float PrimaryS = bHasPrimary ? -Primary.Height : 0.f;
	const float ShowDistance = Params.ApproachDistanceCm;
	const float HideDistance = ShowDistance + Params.ApproachHideMarginCm;

	auto Measure = [&](const FGSTN_DepthPlaneCandidate& C, FApproachInfo& Info) -> bool
	{
		FVector Normal = C.Normal.GetSafeNormal();
		// A probed top must face up as it was seen (an underside or a wall is no platform); a designer plane faces up by decree
		if (!C.bExplicit && FVector::DotProduct(Normal, Up) < Params.ApproachMinNormalDot)
		{
			return false;
		}
		if (FVector::DotProduct(Normal, Up) < 0.0)
		{
			Normal = -Normal;
		}
		FVector OnPlane;
		float S = 0.f;
		if (!ProjectOntoPlane(Feet, Up, C.Point, Normal, OnPlane, S))
		{
			return false;
		}
		// Never the primary's own plane: the same surface, or a coplanar one (a second platform at the same level adds nothing)
		if (bHasPrimary)
		{
			const bool bSameSurface = C.SurfaceKey != 0 && C.SurfaceKey == Primary.SurfaceKey;
			const bool bCoplanar = FMath::Abs(S - PrimaryS) <= Params.CoplanarTolerance
				&& AngleDegrees(Normal, Primary.Normal.GetSafeNormal()) <= Params.CoplanarAngleDegrees;
			if (bSameSurface || bCoplanar)
			{
				return false;
			}
		}
		// Below or near the character; a plane well above it is the above layer's business
		if (S > Params.ApproachNearAboveFeetCm)
		{
			return false;
		}
		if (!C.bExplicit)
		{
			if (bHasPrimary)
			{
				const float Rise = S - PrimaryS;
				if (Rise < Params.ApproachMinRiseCm || Rise > Params.ApproachMaxRiseCm)
				{
					return false;
				}
			}
		}
		Info.Candidate = &C;
		Info.S = S;
		if (C.bExplicit)
		{
			Info.Edge = 0.f;
			Info.Toward = 0.f;
			return true;
		}
		const FVector Offset = C.Point - Feet;
		const FVector Horizontal = Offset - Up * FVector::DotProduct(Offset, Up);
		Info.Edge = static_cast<float>(Horizontal.Size());
		Info.Toward = Info.Edge > 1.f ? static_cast<float>(FVector::DotProduct(HorizontalVelocity, Horizontal / Info.Edge)) : 0.f;
		return true;
	};

	auto IsCurrent = [&](const FGSTN_DepthPlaneCandidate& C) { return State.bApproachShown && C.SurfaceKey == State.ApproachKey; };

	TArray<FApproachInfo> ValidExplicit;
	for (const FGSTN_DepthPlaneCandidate& C : Explicit)
	{
		FApproachInfo Info;
		if (Measure(C, Info))
		{
			ValidExplicit.Add(Info);
		}
	}
	TArray<FApproachInfo> ValidProbed;
	for (const FGSTN_DepthPlaneCandidate& C : Probed)
	{
		FApproachInfo Info;
		if (!Measure(C, Info))
		{
			continue;
		}
		// Near enough: the show distance, or the wider hide distance for the plane already shown (no movement test then)
		const bool bCurrent = IsCurrent(C);
		if (Info.Edge > (bCurrent ? HideDistance : ShowDistance))
		{
			continue;
		}
		if (!bCurrent && Info.Edge > Params.ApproachCloseDistanceCm && Info.Toward < Params.ApproachMinTowardSpeed)
		{
			continue;
		}
		ValidProbed.Add(Info);
	}

	// Score: designer planes win over probing (priority, then key); probed ones by nearest edge, then approach speed
	const FApproachInfo* Best = nullptr;
	if (!ValidExplicit.IsEmpty())
	{
		for (const FApproachInfo& Info : ValidExplicit)
		{
			if (!Best || Info.Candidate->Priority > Best->Candidate->Priority
				|| (Info.Candidate->Priority == Best->Candidate->Priority && Info.Candidate->SurfaceKey < Best->Candidate->SurfaceKey))
			{
				Best = &Info;
			}
		}
	}
	else
	{
		for (const FApproachInfo& Info : ValidProbed)
		{
			if (!Best || Info.Edge < Best->Edge - KINDA_SMALL_NUMBER
				|| (FMath::IsNearlyEqual(Info.Edge, Best->Edge) && (Info.Toward > Best->Toward
					|| (FMath::IsNearlyEqual(Info.Toward, Best->Toward) && Info.Candidate->SurfaceKey < Best->Candidate->SurfaceKey))))
			{
				Best = &Info;
			}
		}
	}
	const FApproachInfo* CurrentBest = nullptr;
	if (State.bApproachShown)
	{
		for (const TArray<FApproachInfo>* List : { &ValidExplicit, &ValidProbed })
		{
			for (const FApproachInfo& Info : *List)
			{
				if (Info.Candidate->SurfaceKey == State.ApproachKey && (!CurrentBest || Info.Edge < CurrentBest->Edge))
				{
					CurrentBest = &Info;
				}
			}
		}
	}

	const double Dwell = Params.ApproachMinDwellMs * 0.001;
	const bool bDwelling = (Time - State.ApproachSwitchTime) < Dwell;
	const FApproachInfo* Chosen = nullptr;
	if (Best)
	{
		if (!State.bApproachShown || Best->Candidate->SurfaceKey == State.ApproachKey)
		{
			Chosen = Best;
		}
		else if (CurrentBest)
		{
			// A challenger replaces the shown plane only after the dwell and when clearly nearer (or designer-declared)
			const bool bClearlyBetter = (Best->Candidate->bExplicit && !State.bApproachExplicit)
				|| Best->Edge + Params.ApproachSwitchMarginCm < CurrentBest->Edge;
			Chosen = (!bDwelling && bClearlyBetter) ? Best : CurrentBest;
		}
		else
		{
			// The shown plane is not among the candidates this frame (probes are staggered): it is held, and only a
			// dwell that is over lets the challenger in
			Chosen = bDwelling ? nullptr : Best;
		}
	}

	FVector Point;
	FVector Normal;
	int64 Key;
	FTransform Transform;
	bool bExplicit;
	float Edge;
	if (Chosen)
	{
		Point = Chosen->Candidate->Point;
		Normal = Chosen->Candidate->Normal;
		Key = Chosen->Candidate->SurfaceKey;
		Transform = Chosen->Candidate->ComponentTransform;
		bExplicit = Chosen->Candidate->bExplicit;
		Edge = Chosen->Edge;
		const bool bSwitch = !State.bApproachShown || Key != State.ApproachKey;
		if (bSwitch)
		{
			State.ApproachSwitchTime = Time;
		}
		State.bApproachShown = true;
		State.ApproachKey = Key;
		State.bApproachExplicit = bExplicit;
		State.ApproachPoint = Point;
		State.ApproachNormal = Normal;
		State.ApproachTransform = Transform;
		State.ApproachLastSeenTime = Time;
		OutApproach.BlendMs = bSwitch ? Params.ApproachBlendMs : 0.f;
	}
	else if (State.bApproachShown && (Time - State.ApproachLastSeenTime) <= Params.ApproachHoldMs * 0.001)
	{
		// Held: the stored plane, while it stays in range
		Point = State.ApproachPoint;
		Normal = State.ApproachNormal;
		Key = State.ApproachKey;
		Transform = State.ApproachTransform;
		bExplicit = State.bApproachExplicit;
		const FVector Offset = Point - Feet;
		Edge = bExplicit ? 0.f : static_cast<float>((Offset - Up * FVector::DotProduct(Offset, Up)).Size());
		if (Edge > HideDistance)
		{
			State.ClearApproach();
			return false;
		}
	}
	else
	{
		State.ClearApproach();
		return false;
	}

	const float Span = FMath::Max(HideDistance - Params.ApproachCloseDistanceCm, 1.f);
	const float EdgeAlpha = bExplicit ? 1.f : FMath::Clamp(1.f - (Edge - Params.ApproachCloseDistanceCm) / Span, 0.f, 1.f);
	const float BlendMs = OutApproach.BlendMs;
	OutApproach = MakeSurfaceTarget(Feet, Up, Point, Normal, Key, Transform, Params, EGSTN_DepthMarkerState::Approach, EdgeAlpha);
	OutApproach.Kind = EGSTN_DepthTargetKind::ApproachPlane;
	OutApproach.BlendMs = BlendMs;
	OutApproach.EdgeDistanceCm = bExplicit ? 0.f : Edge;
	return true;
}

void FGSTN_DepthMarkerMath::ComputeFade(float Height, const FGSTN_DepthFadeParams& Fade, float& OutAlpha, float& OutRadiusScale)
{
	const float T = FMath::Clamp(Height / FMath::Max(Fade.FadeEndHeight, 1.f), 0.f, 1.f);
	OutAlpha = FMath::Lerp(Fade.AlphaNear, Fade.AlphaFar, T);
	OutRadiusScale = FMath::Lerp(Fade.RadiusNear, Fade.RadiusFar, T);
}

bool FGSTN_DepthMarkerMath::UpdateVisibility(FGSTN_DepthSelectionState& State, bool bAirborne, float Height, const FGSTN_DepthFadeParams& Fade)
{
	if (!bAirborne)
	{
		State.bVisible = false;
	}
	else if (State.bVisible)
	{
		State.bVisible = Height >= Fade.HideHeight;
	}
	else
	{
		State.bVisible = Height >= Fade.ShowHeight;
	}
	return State.bVisible;
}

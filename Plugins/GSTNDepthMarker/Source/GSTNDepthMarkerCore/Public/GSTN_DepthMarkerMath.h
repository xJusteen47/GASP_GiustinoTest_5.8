// Depth marker: the pure target selection maths (depth-marker.md §2.3-§2.7, §3).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_DepthMarkerTypes.h"

/**
 * Stateless maths of the default (gravity) selector. Plain C++ so the module needs no Engine: the spec calls this class
 * UGSTN_DepthMarkerMath, a BlueprintFunctionLibrary, which lives in GSTNDepthMarkerRuntime as thin wrappers over these functions.
 *
 * Everything works along the subject's own Up, never world Z: a hidden world-Z assumption fails the tests that use diagonal
 * and flipped gravity. Candidates carry Height (distance along Down from the feet), so no trace is needed to test any rule.
 * State is the caller's FGSTN_DepthSelectionState; time is passed in (seconds), never read, so the functions are deterministic.
 */
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthMarkerMath
{
	/**
	 * Step 4. Keeps blocking, non-excluded, non-start-penetrating hits that are not above the feet by more than
	 * PenetrationTolerance and whose face looks up (dot(Normal, Up) > 0), and that pass the behaviour query (an unknown
	 * surface passes when Params.bUnknownSurfacesPass; an empty query passes everything). Result sorted nearest first.
	 */
	static void FilterCandidates(const TArray<FGSTN_DepthCandidate>& In, const FVector& Up, const FGSTN_DepthSelectorParams& Params,
		const FGameplayTagQuery& TargetBehaviours, TArray<FGSTN_DepthCandidate>& Out);

	/** Step 5. The nearest of an already filtered, sorted list: the first surface below the feet. False when empty */
	static bool ChooseFirstBelow(const TArray<FGSTN_DepthCandidate>& Filtered, FGSTN_DepthCandidate& OutCandidate);

	/**
	 * Step 6, rules R1-R5. Decides between the new candidate (may be absent) and the previously selected surface, updates
	 * State and returns the below target (Kind None when there is no surface: the reference layer may then show the ring).
	 * Footprint is the extra sweep result, only meaningful when the candidate is farther or absent.
	 */
	static FGSTN_DepthTarget ApplyHysteresis(FGSTN_DepthSelectionState& State, bool bHasCandidate, const FGSTN_DepthCandidate& Candidate,
		const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthSelectorParams& Params,
		double TimeSeconds);

	/**
	 * Intersects the line Feet + Up * S with the plane (PlanePoint, PlaneNormal). Returns false for a plane parallel to Up.
	 * S > 0 is along Up (the above ring), S < 0 along Down (a surface under the feet): Height = -S.
	 */
	static bool ProjectOntoPlane(const FVector& Feet, const FVector& Up, const FVector& PlanePoint, const FVector& PlaneNormal,
		FVector& OutPoint, float& OutS);

	/** The normal rotated towards Up so it is at most MaxTiltDegrees away from it (§2.5) */
	static FVector ClampTilt(const FVector& Normal, const FVector& Up, float MaxTiltDegrees);

	/** Cosmetic radius stretch on a slope: 1 / max(dot(Normal, Up), 0.5), at most 2 */
	static float SlopeRadiusScale(const FVector& Normal, const FVector& Up);

	/** Step 1. Remembers Up. Returns true and clears the selection when it turned more than UpResetDegrees since the last call */
	static bool UpdateFrame(FGSTN_DepthSelectionState& State, const FVector& Up, const FGSTN_DepthSelectorParams& Params);

	/** Updates the last grounded point after GroundedMinMs on a static surface (§2.7) */
	static void UpdateGrounded(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthSelectorParams& Params,
		double TimeSeconds);

	/**
	 * The reference plane: the override when valid, else the plane through the last grounded point with the CURRENT Up (so a
	 * gravity change rebuilds it). False when neither exists (no plane means no ring).
	 */
	static bool ResolveReferencePlane(const FGSTN_DepthSelectionState& State, const FVector& Up, const FGSTN_DepthReferenceOverride& Override,
		FVector& OutPoint, FVector& OutNormal, bool& bOutHazard);

	/**
	 * Step 8. Fills the reference layers of Result from the plane, independently of the surface search. bHasBelowSurface tells
	 * whether step 5/6 produced a surface target; bProbed whether a probe ran this frame (the ring below needs one).
	 */
	static void ApplyReferenceLayers(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthSelectorParams& Params,
		const FGSTN_DepthReferenceOverride& Override, bool bProbed, FGSTN_DepthResult& InOutResult);

	/**
	 * The whole default selector for one probe tick, on numbers: frame check, grounded point, filter, choose, hysteresis,
	 * reference layers. The runtime only supplies the traces. When bProbed is false the surface search is skipped and the
	 * previous selection is kept (the gate of step 2).
	 */
	static FGSTN_DepthResult Evaluate(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject,
		const TArray<FGSTN_DepthCandidate>& RawCandidates, const FGSTN_DepthFootprintHit& Footprint,
		const FGSTN_DepthReferenceOverride& Override, const FGSTN_DepthSelectorParams& Params, const FGameplayTagQuery& TargetBehaviours,
		bool bProbed, double TimeSeconds);

	/** Re-expresses a stored plane with the hit component's CURRENT transform and re-projects the column: the marker follows moving bases between traces */
	static FGSTN_DepthTarget ReprojectTarget(const FGSTN_DepthTarget& Target, const FTransform& CurrentComponentTransform,
		const FVector& Feet, const FVector& Up);

	/**
	 * Approach marker (marker B). Chooses at most one nearby higher platform and returns its plane as a target: the character
	 * column (Feet + Up * S) projected onto it, which hangs in the air beyond the platform edge.
	 *
	 * Candidates: Probed are walkable tops found by the runtime probes around the character; Explicit are designer planes
	 * that already overlap the character and win over probing. A plane is kept only when it is not above the feet by more than
	 * ApproachNearAboveFeetCm, rises ApproachMinRiseCm..ApproachMaxRiseCm over the primary ground (Primary, the below target),
	 * is not the primary plane (same surface key, or coplanar within CoplanarTolerance), and, if probed, its nearest sampled
	 * point is within ApproachDistanceCm horizontally and the character moves towards it or is within ApproachCloseDistanceCm.
	 * Score: nearest edge distance, then approach speed. Hysteresis: a shown plane is kept up to ApproachDistanceCm plus the hide
	 * margin without the movement test, held ApproachHoldMs when no probe confirms it, and replaced only after
	 * ApproachMinDwellMs by a challenger ApproachSwitchMarginCm nearer. Returns true and fills OutApproach when shown.
	 */
	static bool EvaluateApproach(FGSTN_DepthSelectionState& State, const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthSelectorParams& Params,
		const FGSTN_DepthTarget& Primary, const TArray<FGSTN_DepthPlaneCandidate>& Probed, const TArray<FGSTN_DepthPlaneCandidate>& Explicit,
		double TimeSeconds, FGSTN_DepthTarget& OutApproach);

	/** Number of approach probe slots: the ring plus the velocity probes */
	static int32 NumApproachProbeSlots(const FGSTN_DepthSelectorParams& Params);

	/**
	 * The segment of one approach probe slot: a downward line that starts ApproachNearAboveFeetCm above the feet at a ring offset
	 * (or ahead along the horizontal velocity) and reaches ApproachProbeDepthCm below that. False for a velocity slot while the
	 * character is slower than ApproachMinTowardSpeed.
	 */
	static bool ApproachProbeSegment(const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthSelectorParams& Params, int32 Slot,
		FVector& OutStart, FVector& OutEnd);

	/** True when Point is inside the oriented box (transform without scale, half extents already scaled) */
	static bool PointInOrientedBox(const FVector& Point, const FTransform& BoxTransform, const FVector& HalfExtent);

	/** Alpha and radius factors over the height above the target (linear between the near and far values) */
	static void ComputeFade(float Height, const FGSTN_DepthFadeParams& Fade, float& OutAlpha, float& OutRadiusScale);

	/** Show/hide with hysteresis: shown when airborne and Height >= ShowHeight, hidden below HideHeight. Updates State.bVisible */
	static bool UpdateVisibility(FGSTN_DepthSelectionState& State, bool bAirborne, float Height, const FGSTN_DepthFadeParams& Fade);
};

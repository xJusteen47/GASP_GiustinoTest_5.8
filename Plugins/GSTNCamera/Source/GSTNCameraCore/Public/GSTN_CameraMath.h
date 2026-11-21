// Pure camera maths (camera.md §10). No world, no Engine; every function takes the frame explicitly, so none assumes world Z.
// The BlueprintFunctionLibrary UGSTN_CameraMath in GSTNCameraRuntime wraps the commonly useful ones.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraTypes.h"

struct GSTNCAMERACORE_API FGSTN_CameraMath
{
	// ---- Angles and the frame (camera.md §7.2) ----

	/** Signed shortest difference To - From in degrees, in (-180, 180] */
	static float ShortestAngleDelta(float FromDeg, float ToDeg);

	/** Move CurrentDeg towards TargetDeg by at most MaxDelta degrees along the shortest arc */
	static float StepAngleToward(float CurrentDeg, float TargetDeg, float MaxDeltaDeg);

	/** Vector projected on the plane normal to Up, normalised. Zero when it is (nearly) parallel to Up */
	static FVector ProjectOnUpPlane(const FVector& Vector, const FVector& Up);

	/** The deterministic reference direction of a frame: world X projected on the plane normal to Up, world Y when degenerate (camera.md §7.2). One function for the sim and the encoder */
	static FVector ReferenceForward(const FVector& Up);

	/** Build a frame from Up and a preferred forward (re-orthogonalised; the reference forward when the preference is degenerate) */
	static FGSTN_CameraFrame MakeFrame(const FVector& Up, const FVector& PreferredForward);

	/** Direction at HeadingDeg about Up from the frame's RefForward */
	static FVector HeadingToDirection(const FGSTN_CameraFrame& Frame, float HeadingDeg);

	/** Heading about Up of a direction (its projection on the Up plane); 0 when the direction is parallel to Up */
	static float DirectionToHeading(const FGSTN_CameraFrame& Frame, const FVector& Direction);

	/** Elevation of a direction above the plane normal to Up, degrees */
	static float DirectionToPitch(const FVector& Up, const FVector& Direction);

	/** Rate-limited step of Up towards a target (camera.md §7.2): never turns more than MaxDegPerSecond * Dt. Exactly opposite vectors turn about a deterministic axis, never snap */
	static FVector StepUp(const FVector& CurrentUp, const FVector& TargetUp, float MaxDegPerSecond, float DeltaSeconds);

	/** Carry a forward from the old Up to the new one by the shortest rotation, then re-orthogonalise (parallel transport, camera.md §7.2) */
	static FVector TransportForward(const FVector& OldUp, const FVector& NewUp, const FVector& Forward);

	// ---- Blend (camera.md §2.2) ----

	/** Weight for a raw progress in [0,1]: monotonic, 0 at 0 and 1 at 1 */
	static float BlendAlpha(EGSTN_CameraBlendFunction Function, float Progress, float Exponent);

	/** Heading by the shortest arc; pivot, pitch, arm, offsets and FOV linear */
	static FGSTN_CameraView BlendView(const FGSTN_CameraView& A, const FGSTN_CameraView& B, float Alpha);

	/** The world pose of a view in a frame */
	static FGSTN_CameraPose ComputePose(const FGSTN_CameraFrame& Frame, const FGSTN_CameraView& View);

	/** The view that looks from CameraLocation at Target in the frame (used by the fixed point and rail paths); the arm is the distance */
	static FGSTN_CameraView ViewLookingAt(const FGSTN_CameraFrame& Frame, const FVector& CameraLocation, const FVector& Target, float FovDeg);

	// ---- Orbit (camera.md §4.3) ----

	/** Clamp a manual orbit around a desired angle: yaw within +-limit of the desired heading, pitch within the offsets, zoom within the scale limit */
	static FGSTN_CameraOrbitState ClampOrbitAroundDesired(const FGSTN_CameraOrbitState& Orbit, float DesiredYawDeg, float DesiredPitchDeg, const FGSTN_CameraOrbitLimits& Limits);

	/**
	 * Ease an orbit back to the desired angle once SecondsSinceInput passed DelaySeconds (0 = never). Returns true when the orbit is on the desired angle.
	 * Yaw and pitch move at DegPerSecond; the zoom returns to 1 at the same relative speed.
	 */
	static bool StepRecenter(FGSTN_CameraOrbitState& InOutOrbit, float DesiredYawDeg, float DesiredPitchDeg, float SecondsSinceInput, float DelaySeconds, float DegPerSecond, float DeltaSeconds);

	// ---- Rails (camera.md §4.1) ----

	/** Nearest point of a polyline to a point. Returns false for fewer than two points. OutDistanceAlongCm is the arc length to the point */
	static bool ProjectOnPolyline(const TArray<FVector>& Points, const FVector& Point, FVector& OutPoint, float& OutDistanceAlongCm, int32* OutSegment = nullptr);

	// ---- Section and rule choice (camera.md §3, §4.2) ----

	/** Highest priority; ties: the one entered last, then the smaller volume, then the lower Id. INDEX_NONE for no candidate */
	static int32 ChooseSection(const TArray<FGSTN_CameraSectionCandidate>& Candidates);

	/**
	 * Highest priority matching rule (ties to table order) debounced by the rule delays and the minimum dwell (camera.md §3.4).
	 * Returns the chosen rule index, INDEX_NONE when none ever matched.
	 */
	static int32 StepRuleChoice(FGSTN_CameraRuleChoiceState& InOutState, const TArray<FGSTN_CameraRuleEval>& Rules, float DeltaSeconds, float MinDwellSeconds);

	// ---- Aim (camera.md §6) ----

	/**
	 * Converged aim: the direction from the character's aim origin to the point under the view centre. No hit, or a hit beyond ConvergeMaxCm,
	 * gives the camera forward; a hit nearer than ConvergeMinCm is taken at ConvergeMinCm along the view axis so the aim never inverts.
	 */
	static FVector ConvergedAimDirection(const FVector& ViewOrigin, const FVector& ViewForward, const FVector& AimOrigin, bool bHit, const FVector& HitPoint, float ConvergeMinCm, float ConvergeMaxCm);

	// ---- Frame change (camera.md §5) ----

	/** True when the camera decided to turn the reference by at least MinYawDeltaDeg about Up. Manual orbit never raises it */
	static bool ShouldRaiseFrameChange(const FVector& Up, const FVector& FromForward, const FVector& ToForward, float MinYawDeltaDeg, bool bManualOrbit, float* OutDeltaDeg = nullptr);

	// ---- Free camera (camera.md §2.4) ----

	/** The last ground height along Up that the free camera follows: the jump arc is ignored inside the dead zone */
	static float StepGroundHeight(float GroundHeight, float CurrentHeight, bool bGrounded, float JumpDeadZoneUpCm, float FallFollowCm);

	// ---- Duel (camera.md §2.5) ----

	/**
	 * Horizontal look direction of the duel camera: perpendicular to the line between the fighters, the side nearer to CurrentForward
	 * (so it never flips across them). With no current direction the side puts A on the left of the screen when bAOnLeft. Returns false and
	 * keeps CurrentForward when the horizontal separation is under MinAxisCm.
	 */
	static bool DuelDirection(const FVector& A, const FVector& B, const FVector& Up, const FVector& CurrentForward, bool bAOnLeft, float MinAxisCm, FVector& OutDirection);

	/** Arm that keeps a horizontal separation and a vertical extent inside the frame, clamped to the arm range. HorizontalFovDeg and AspectRatio as in the view */
	static float DuelArm(float SeparationCm, float VerticalExtentCm, float MarginCm, float HorizontalFovDeg, float AspectRatio, float MinArmCm, float MaxArmCm);

	/**
	 * One step of the cross-over safe duel camera (CAM.8). The framing axis follows the line between the fighters projected on the plane
	 * normal to Up, but the SIDE of that line the camera sits on is held: when a fighter jumps over the other the look direction stays put
	 * (the fighters swap screen sides for a moment) and it only turns to the wanted screen sides after the fighters stayed crossed for
	 * SideSwapDelaySeconds with both grounded (or SideSwapMaxDelaySeconds regardless). The heading never turns faster than MaxYawRateDegPerSecond.
	 * Fighters closer than MinSeparationCm on the Up plane freeze the axis (state untouched, bFrozen set). Returns true when the state has a heading.
	 */
	static bool StepDuelCamera(FGSTN_DuelCameraState& InOutState, const FGSTN_CameraFrame& Frame, const FVector& A, const FVector& B, bool bAOnLeft, bool bBothGrounded,
		float MinSeparationCm, float SideSwapDelaySeconds, float SideSwapMaxDelaySeconds, float MaxYawRateDegPerSecond, float DeltaSeconds);

	// ---- Shoulder swap (CAM.2) ----

	/** Move a side blend in [-1, 1] (+1 right, -1 left) towards the target side, crossing the whole range in BlendSeconds. 0 snaps */
	static float StepShoulderBlend(float Current, EGSTN_ShoulderSide TargetSide, float BlendSeconds, float DeltaSeconds);

	// ---- Group framing (CAM.7) ----

	/** One opponent is a duel, two or more an arena fight, none no group framing */
	static EGSTN_CameraGroupFraming ChooseGroupFraming(int32 OpponentCount);

	/**
	 * Shared arena framing (CAM.7, Power Stone): Fighters are the pivots of every fighter, the first one the local pawn (weight LocalPlayerWeight).
	 * Focus is their weighted centroid clamped to Bounds (ignored when not valid; a world AABB, so under a tilted Up it is a coarse limit).
	 * HeadingDeg is returned untouched: the yaw is arena relative and never depends on where the fighters are. The arm is the smallest within
	 * MinArmCm..MaxArmCm that keeps a sphere of FighterRadiusCm around every fighter inside the screen with ScreenMargin (the pitch rises with
	 * the arm, see ArenaPitchForArm); at the limit it is best effort. FocusOverride (the smoothed focus) replaces the computed focus for the fit.
	 */
	static FGSTN_ArenaFraming ArenaFrameAll(const TArray<FVector>& Fighters, const FGSTN_CameraFrame& Frame, float HeadingDeg, const FGSTN_CameraArenaTuning& Tuning, float AspectRatio, const FBox& Bounds, const FVector* FocusOverride = nullptr);

	/** Pitch for an arm: PitchDeg at MinArmCm, MaxExtraPitchDeg lower (looking down more) at MaxArmCm */
	static float ArenaPitchForArm(const FGSTN_CameraArenaTuning& Tuning, float ArmCm);

	/** One damped zoom step: exponential, ZoomOutSeconds when the wanted arm is larger, ZoomInSeconds when smaller. 0 seconds snaps. Never overshoots */
	static float StepArenaZoom(float CurrentArmCm, float WantedArmCm, float ZoomInSeconds, float ZoomOutSeconds, float DeltaSeconds);

	// ---- Platform camera P1a (docs/notes/camera-platforming/p1a.md) ----

	/**
	 * Asymmetric pivot follow: the part of the lag on the plane normal to Up uses CatchUpSeconds while the subject moves away along the lag
	 * (accelerating, running: softer) and SettleSeconds when it does not (braking, turning back: quicker), blended by the speed along the lag
	 * over FullSpeed. The part along Up uses VerticalSeconds. Exponential, frame rate independent; 0 seconds snaps that part
	 */
	static FVector StepPivotAsymmetric(const FVector& Smoothed, const FVector& Target, const FVector& Up, const FVector& Velocity,
		float CatchUpSeconds, float SettleSeconds, float FullSpeed, float VerticalSeconds, float DeltaSeconds);

	/**
	 * Horizontal dead zone: returns the new anchor. The offset Target - Anchor on the plane normal to Up is split along the camera right and
	 * forward (from HeadingForward); each part beyond its half size (Width/2, Depth/2) moves the anchor, the part inside does not. The part along
	 * Up always follows (the vertical band is handled elsewhere). A size of 0 turns that axis off (the anchor follows exactly)
	 */
	static FVector StepDeadZone(const FVector& Anchor, const FVector& Target, const FVector& Up, const FVector& HeadingForward, float WidthCm, float DepthCm);

	/** Ease a value to Target in Seconds (ease-in-out); a new target restarts from the current value. The first call snaps. Returns the value */
	static float StepEasedFloat(FGSTN_CameraEasedFloat& InOutState, float Target, float Seconds, float DeltaSeconds);

	/** The player distance scale of a preset (Custom clamps CustomScale to the slider range) */
	static float DistancePresetScale(const FGSTN_CameraDistancePresets& Presets, EGSTN_CameraDistancePreset Preset, float CustomScale);

	/**
	 * Look input of the ControlRotation source (2026-11-21, P1a pitch lock): the change of the controller's control rotation since the camera left
	 * it at the end of its previous frame (after its write back), in look units (deg / DegreesPerUnit), X = yaw, Y = pitch. It must be read BEFORE
	 * this frame's write back: read after it, the change is the camera's own motion and feeding it back as input makes an integrator that runs the
	 * pitch to its clamp (-70 / +20). bContinuous false (first frame, after a pause or a reactivation) = no input, never a jump to where the
	 * controller was
	 */
	static FVector2D ControlRotationLookDelta(const FRotator& Now, const FRotator& LeftLastFrame, bool bContinuous, float DegreesPerUnit);

	/** Next preset in Near, Medium, Far order (Direction > 0 farther, < 0 nearer), clamped at the ends; Custom steps from Medium */
	static EGSTN_CameraDistancePreset CycleDistancePreset(EGSTN_CameraDistancePreset Current, int32 Direction);

	/**
	 * One step of the gentle follow behind (decision 2A). MoveHeadingDeg is the heading of the horizontal velocity; Scale in [0, 1] is how much
	 * it is allowed now (0 when blocked by a tag, an option, an aim or a soft framing volume). SecondsSinceLook == 0 drops the speed at once.
	 * Returns the new yaw
	 */
	static float StepAutoFollow(FGSTN_CameraAutoFollowState& InOutState, const FGSTN_CameraAutoFollowTuning& Tuning, float YawDeg, float MoveHeadingDeg,
		float HorizontalSpeed, bool bGrounded, float SecondsSinceLook, float Scale, float DeltaSeconds);

	/** Latch the move reference on a camera-made turn of the reference (OldYawDeg is the heading before the turn) */
	static void LatchStickReference(FGSTN_CameraStickLatchState& InOutState, const FGSTN_CameraStickLatchTuning& Tuning, float OldYawDeg);

	/** The heading the move stick should be read against: the latched one until the stick returns under ReleaseMagnitude (or MaxHoldSeconds), else CameraYawDeg */
	static float ResolveStickReference(FGSTN_CameraStickLatchState& InOutState, const FGSTN_CameraStickLatchTuning& Tuning, float StickMagnitude, float CameraYawDeg, float DeltaSeconds);

	/** Weight of a point in an oriented box given in box space: 0 outside, 1 deeper than FalloffCm inside, smooth in between. FalloffCm 0 = 1 inside */
	static float BoxFalloffWeight(const FVector& LocalPoint, const FVector& Extent, float FalloffCm);

	/** Combine soft framing contributions: higher priority first, each takes its weight of the weight left (ties in input order) */
	static FGSTN_CameraBiasResult CombineBias(const TArray<FGSTN_CameraBiasInput>& Inputs);

	/**
	 * Apply the combined framing to the player orbit. Never a hard lock: the pull only runs once SecondsSinceLook passed the resume delay
	 * (look input stops it at once) and moves exponentially in PullSeconds, scaled by the pull weight, capped at MaxPullDegPerSecond.
	 * Firm framing also keeps the yaw within FirmRangeDeg of the framing heading, widened towards 180 as the firm weight drops (volume edge).
	 * Call after the look input was applied. Returns true when it moved the orbit
	 */
	static bool StepBiasOrbit(FGSTN_CameraOrbitState& InOutOrbit, const FGSTN_CameraBiasResult& Bias, float SecondsSinceLook, float DeltaSeconds);

	// ---- Occlusion (camera.md §7.1) ----

	static EGSTN_CameraOcclusionAction DecideOcclusion(EGSTN_CameraOcclusionPolicy Policy, bool bFadeable, float BlockerDistanceFromCameraCm, float NearClipSafeCm);
};

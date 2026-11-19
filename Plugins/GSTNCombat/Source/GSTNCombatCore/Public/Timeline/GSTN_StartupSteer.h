// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"

struct FGSTN_CombatTimelineData;
enum class EGSTN_SteerSource : uint8;

/** Where a frame sits in a move's steering (FGSTN_StartupSteer::ResolveFrame) */
enum class EGSTN_SteerPhase : uint8
{
	/** Nothing to do: no steering this frame, the movement mode turns the character as it always does */
	None,
	/** Before the window opens (move stick sources holding the facing): the yaw is held where it is */
	Before,
	/** The window is open: the yaw follows the stick inside the cone around the move origin */
	Steering,
	/** Locked: from the end of the window to the end of the last active frame, the yaw is held where it is */
	Locked,
};

/** What a frame's command gives the steering: the stick the move's source picks, already chosen (FGSTN_StartupSteer::PickStick) */
struct FGSTN_SteerFrameInput
{
	/** Frames since the move started, pauses excluded */
	int32 TimelineFrame = 0;
	/** Yaw of the move origin (the facing at the start), degrees */
	float StartYawDeg = 0.f;
	/** Facing at the end of the previous frame (Mover sync state), degrees */
	float CurrentYawDeg = 0.f;
	/** The picked stick (+Y up), and whether the source reads anything this frame (the reticle source with the reticle off reads nothing) */
	FVector2D Stick = FVector2D::ZeroVector;
	bool bSourceActive = false;
	float CameraYawDeg = 0.f;
	/** The fixed step; 0 = the rate does not apply (the turn is immediate) */
	float StepSeconds = 0.f;
};

/**
 * Right-stick steering during a move's startup (plan precisione-weakpoint 5): the aim stick turns the move's direction, but never
 * more than the timeline's MaxSteerDegrees away from the facing the move started with. It sits on top of the aim policy (which picks
 * the start facing) and does not replace it.
 *
 * Pure function of the start yaw, the stick and the camera yaw of the frame's input command, so a client and the server compute
 * the same yaw from the same command and a re-simulation gives it again. The sim keeps the start yaw in its sync state and calls
 * this every startup frame: the steer is a clamp against the start, not an accumulated turn, so a dropped frame cannot drift it.
 */
struct GSTNCOMBATCORE_API FGSTN_StartupSteer
{
	/** Stick magnitude (unit circle) under which the aim stick does not steer: a resting thumb must not turn an attack */
	static constexpr float DeadZone = 0.25f;

	/**
	 * Yaw (degrees, -180..180) the move faces after steering. The stick (+Y up, as on the controller) is turned into a world yaw with
	 * the camera yaw (+Y = camera forward), and StartYawDeg is turned toward it by at most MaxSteerDegrees either way. Inside the dead
	 * zone, or with MaxSteerDegrees <= 0, it is StartYawDeg.
	 */
	static float SteerYaw(float StartYawDeg, const FVector2D& AimStick, float CameraYawDeg, float MaxSteerDegrees);

	/** True when the stick is out of the dead zone and the move can steer at all: only then does a frame ask for a turn */
	static bool IsSteering(const FVector2D& AimStick, float MaxSteerDegrees);

	/**
	 * Aim handling (M11 L9): TargetYawDeg reached from CurrentYawDeg by at most RateDegPerSecond * StepSeconds this frame, the short way
	 * round. A rate or a step of 0 gives the target at once. Pure: the current yaw is the pose at the end of the previous frame (sync state)
	 */
	static float RateLimitYaw(float CurrentYawDeg, float TargetYawDeg, float RateDegPerSecond, float StepSeconds);

	/**
	 * The timeline frame the steering stops at (exclusive): the first active frame, as FGSTN_TimelinePhases counts it (hitbox windows
	 * that hit somebody), without building the phases. 0 for a move with no hitbox: nothing to aim, nothing steers
	 */
	static int32 GetSteerEndFrame(const FGSTN_CombatTimelineData& Data);

	/**
	 * The stick a move's source reads in a frame (owner 2026-11-14): AimStickWithReticle the aim stick while the reticle is on (else none),
	 * MoveStick the move stick, MoveOrAimStick the aim stick while the reticle is on and the aim stick is out of the dead zone, else the move
	 * stick. False when the source reads nothing this frame
	 */
	static bool PickStick(EGSTN_SteerSource Source, const FVector2D& MoveStick, const FVector2D& AimStick, bool bPrecisionAim, FVector2D& OutStick);

	/** Whether the source is one of the move stick ones, which hold the facing (FGSTN_SteerWindow::bHoldFacing) */
	static bool IsMoveStickSource(EGSTN_SteerSource Source);

	/** The steer window of a move, [OutStart, OutEnd) in timeline frames: FGSTN_SteerWindow's frames, -1 resolved to GetSteerEndFrame */
	static void GetSteerWindow(const FGSTN_CombatTimelineData& Data, int32& OutStart, int32& OutEnd);

	/** The frame the facing hold ends at (exclusive): the end of the last active hitbox window, never before the steer window ends. 0 without hitboxes */
	static int32 GetHoldEndFrame(const FGSTN_CombatTimelineData& Data);

	/**
	 * The whole per-frame decision (owner 2026-11-14): the phase of the frame and, unless None, the yaw the move faces at its end.
	 *  - Steering: the stick out of the dead zone aims a yaw clamped to MaxSteerDegrees around the move origin; inside the dead zone the reticle
	 *    source goes back to the origin (as it always did), the move stick sources stay where they are (releasing the stick keeps the aim).
	 *    With SteerRateDegPerSecond > 0 the yaw moves from the current one by at most rate * RateScale(t) * StepSeconds.
	 *  - Before / Locked (move stick sources with bHoldFacing): the current yaw, so the movement mode does not turn it.
	 * Pure: a function of the data and of In, so every machine and every resimulation gets the same yaw. OutStickActive says the stick was
	 * out of the dead zone (only then does the combat tick ask Mover for a turn)
	 */
	static EGSTN_SteerPhase ResolveFrame(const FGSTN_CombatTimelineData& Data, const FGSTN_SteerFrameInput& In, float& OutYawDeg, bool& OutStickActive);

	/**
	 * RateLimitYaw inside the cone: for caps above 90 degrees the short way round can leave the cone (from +170 to -170 through the back), so
	 * the step is taken in the offset from the start yaw instead. Caps of 180 turn the short way (the cone is the whole circle)
	 */
	static float RateLimitInCone(float StartYawDeg, float CurrentYawDeg, float TargetYawDeg, float MaxSteerDegrees, float MaxStepDeg);

	/** Yaw at 65536 steps per turn, as the move origin keeps it, in the int16 a combat request carries (wraps, so -180 and 180 are one value) */
	static int16 QuantizeYaw(float YawDeg);
	static float DequantizeYaw(int16 Value);
};

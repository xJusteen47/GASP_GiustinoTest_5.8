// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"

/** One recorded pose of a platform: the pose at the end of a simulation frame (many-enemies-replication.md 4.4) */
struct GSTNPLATFORMCORE_API FGSTN_PoseSample
{
	/** Simulation frame whose end this pose is; INDEX_NONE for an empty slot */
	int32 Frame = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	/** Length of the frame that ends at this pose (s), for velocities */
	double StepSeconds = 0.0;
};

/**
 * Ring of the last Capacity poses of a platform, indexed by simulation frame (many-enemies-replication.md 4.4, the shared data of
 * lag compensation and of approach D). Recording a frame again overwrites it, which is what a re-simulation does: the poses are a
 * function of the frame, so the second record writes the same values.
 *
 * Velocities are derived from two consecutive frames, never read from physics: a kinematic collider teleported at the pose point
 * has no physics velocity, and a physics velocity would differ between machines with their frame timing. Pure and deterministic:
 * the same records give the same bits.
 */
struct GSTNPLATFORMCORE_API FGSTN_PoseHistory
{
	/** About 380 ms at 62.5 Hz: enough for one RTT of lag compensation (MaxRewindMs) and for the velocity of any recent frame */
	static constexpr int32 Capacity = 24;

	/** Writes the end pose of Frame (scale ignored). Frame must be >= 0 */
	void Record(int32 Frame, const FTransform& Pose, double StepSeconds);

	/** The pose recorded for exactly that frame, if it is still in the ring */
	bool Sample(int32 Frame, FGSTN_PoseSample& OutSample) const;

	/**
	 * Velocity at a world point of the rigid frame between the end of Frame-1 and the end of Frame: linear velocity of the origin
	 * plus angular velocity crossed with the point's offset from the origin at the end of Frame. False (and zero) when either frame
	 * is missing or the step is not positive
	 */
	bool GetVelocityAtPoint(int32 Frame, const FVector& WorldPoint, FVector& OutVelocity) const;

	/** Highest frame recorded, INDEX_NONE if empty */
	int32 GetLatestFrame() const { return LatestFrame; }

	void Reset();

private:
	static int32 SlotOf(int32 Frame) { return ((Frame % Capacity) + Capacity) % Capacity; }

	FGSTN_PoseSample Ring[Capacity];
	int32 LatestFrame = INDEX_NONE;
};

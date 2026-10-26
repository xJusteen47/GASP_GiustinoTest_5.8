// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PoseHistory.h"

void FGSTN_PoseHistory::Record(int32 Frame, const FTransform& Pose, double StepSeconds)
{
	if (Frame < 0)
	{
		return;
	}
	FGSTN_PoseSample& Slot = Ring[SlotOf(Frame)];
	Slot.Frame = Frame;
	Slot.Location = Pose.GetLocation();
	Slot.Rotation = Pose.GetRotation().GetNormalized();
	Slot.StepSeconds = StepSeconds;
	LatestFrame = FMath::Max(LatestFrame, Frame);
}

bool FGSTN_PoseHistory::Sample(int32 Frame, FGSTN_PoseSample& OutSample) const
{
	if (Frame < 0)
	{
		return false;
	}
	const FGSTN_PoseSample& Slot = Ring[SlotOf(Frame)];
	if (Slot.Frame != Frame)
	{
		return false;
	}
	OutSample = Slot;
	return true;
}

bool FGSTN_PoseHistory::GetVelocityAtPoint(int32 Frame, const FVector& WorldPoint, FVector& OutVelocity) const
{
	OutVelocity = FVector::ZeroVector;
	FGSTN_PoseSample Now, Before;
	if (!Sample(Frame, Now) || !Sample(Frame - 1, Before) || Now.StepSeconds <= 0.0)
	{
		return false;
	}
	const double InvDt = 1.0 / Now.StepSeconds;
	const FVector Linear = (Now.Location - Before.Location) * InvDt;

	// Rotation over the frame as an axis and the shortest angle, turned into an angular velocity
	FQuat Delta = Now.Rotation * Before.Rotation.Inverse();
	if (Delta.W < 0.0)
	{
		Delta = FQuat(-Delta.X, -Delta.Y, -Delta.Z, -Delta.W);
	}
	FVector Axis;
	double Angle;
	Delta.ToAxisAndAngle(Axis, Angle);
	const FVector Angular = Angle > UE_DOUBLE_SMALL_NUMBER ? Axis * (Angle * InvDt) : FVector::ZeroVector;

	OutVelocity = Linear + FVector::CrossProduct(Angular, WorldPoint - Now.Location);
	return true;
}

void FGSTN_PoseHistory::Reset()
{
	for (FGSTN_PoseSample& Slot : Ring)
	{
		Slot = FGSTN_PoseSample();
	}
	LatestFrame = INDEX_NONE;
}

// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_GravityFrame.h"

namespace GSTNGravityFramePrivate
{
	FVector UnitUp(const FVector& Up)
	{
		return Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	}
}

bool FGSTN_GravityFrame::IsWorldUp(const FVector& Up)
{
	return Up.IsNearlyZero() || GSTNGravityFramePrivate::UnitUp(Up).Equals(FVector::UpVector, 1.0e-6);
}

FQuat FGSTN_GravityFrame::WorldToGravity(const FVector& Up)
{
	return IsWorldUp(Up) ? FQuat::Identity : FQuat::FindBetweenNormals(FVector::UpVector, GSTNGravityFramePrivate::UnitUp(Up));
}

float FGSTN_GravityFrame::YawOfDirection(const FVector& WorldDirection, const FVector& Up)
{
	if (IsWorldUp(Up))
	{
		return static_cast<float>(WorldDirection.Rotation().Yaw);
	}
	return static_cast<float>(WorldToGravity(Up).Inverse().RotateVector(WorldDirection).Rotation().Yaw);
}

float FGSTN_GravityFrame::YawOfRotation(const FQuat& WorldRotation, const FVector& Up)
{
	if (IsWorldUp(Up))
	{
		return static_cast<float>(WorldRotation.Rotator().Yaw);
	}
	return static_cast<float>((WorldToGravity(Up).Inverse() * WorldRotation).Rotator().Yaw);
}

float FGSTN_GravityFrame::YawOfRotator(const FRotator& WorldRotation, const FVector& Up)
{
	return IsWorldUp(Up) ? static_cast<float>(WorldRotation.Yaw) : YawOfRotation(WorldRotation.Quaternion(), Up);
}

FVector FGSTN_GravityFrame::DirectionOfYaw(float YawDeg, const FVector& Up)
{
	const FVector Local = FRotator(0.0, YawDeg, 0.0).Vector();
	return IsWorldUp(Up) ? Local : WorldToGravity(Up).RotateVector(Local);
}

FRotator FGSTN_GravityFrame::WithYaw(const FRotator& WorldRotation, float YawDeg, const FVector& Up)
{
	if (IsWorldUp(Up))
	{
		return FRotator(WorldRotation.Pitch, FRotator::NormalizeAxis(YawDeg), WorldRotation.Roll);
	}
	const FQuat ToGravity = WorldToGravity(Up);
	FRotator Relative = (ToGravity.Inverse() * WorldRotation.Quaternion()).Rotator();
	Relative.Yaw = FRotator::NormalizeAxis(YawDeg);
	return (ToGravity * Relative.Quaternion()).Rotator();
}

FQuat FGSTN_GravityFrame::UprightRotation(const FQuat& WorldRotation, const FVector& Up)
{
	const float Yaw = YawOfRotation(WorldRotation, Up);
	if (IsWorldUp(Up))
	{
		return FRotator(0.0, Yaw, 0.0).Quaternion();
	}
	return WorldToGravity(Up) * FRotator(0.0, Yaw, 0.0).Quaternion();
}

FVector FGSTN_GravityFrame::Flatten(const FVector& V, const FVector& Up)
{
	if (Up.IsNearlyZero())
	{
		return V;
	}
	const FVector UnitUp = GSTNGravityFramePrivate::UnitUp(Up);
	return V - UnitUp * FVector::DotProduct(V, UnitUp);
}

FVector FGSTN_GravityFrame::FollowGravity(const FVector& Authored, const FVector& EntityGravity)
{
	const double Strength = EntityGravity.Size();
	if (Strength <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	const FVector Up = -EntityGravity / Strength;
	const double Scale = Strength / NominalGravity;
	const FVector Turned = IsWorldUp(Up) ? Authored : WorldToGravity(Up).RotateVector(Authored);
	return FMath::IsNearlyEqual(Scale, 1.0, 1.0e-9) ? Turned : Turned * Scale;
}

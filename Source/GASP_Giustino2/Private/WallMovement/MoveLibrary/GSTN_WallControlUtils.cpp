// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"

void UGSTN_WallControlUtils::MakePlaneAxes(const FVector& WallNormal, const FVector& GravityUp, FVector& OutHorizontal, FVector& OutVertical)
{
	OutVertical = FVector::VectorPlaneProject(GravityUp, WallNormal).GetSafeNormal();
	if (OutVertical.IsNearlyZero())
	{
		// Ceiling or floor: no gravity tangent; any stable tangent will do
		FVector Seed = FMath::Abs(WallNormal.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		OutVertical = FVector::VectorPlaneProject(Seed, WallNormal).GetSafeNormal();
	}
	OutHorizontal = FVector::CrossProduct(OutVertical, WallNormal).GetSafeNormal();
}

FVector2D UGSTN_WallControlUtils::ToPlane(const FVector& WorldVector, const FVector& Horizontal, const FVector& Vertical)
{
	return FVector2D(FVector::DotProduct(WorldVector, Horizontal), FVector::DotProduct(WorldVector, Vertical));
}

FVector UGSTN_WallControlUtils::FromPlane(const FVector2D& PlaneVector, const FVector& Horizontal, const FVector& Vertical)
{
	return Horizontal * PlaneVector.X + Vertical * PlaneVector.Y;
}

FVector UGSTN_WallControlUtils::ProjectEntryVelocity(const FGSTN_WallSlideControlSettings& Settings, const FVector& Velocity, const FVector& WallNormal)
{
	const FVector OnPlane = FVector::VectorPlaneProject(Velocity, WallNormal);
	if (OnPlane.SizeSquared() <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	const float Speed = FMath::Min(Velocity.Size() * Settings.EntrySpeedKeep, Settings.MaxEntrySpeed);
	return OnPlane.GetSafeNormal() * Speed;
}

bool UGSTN_WallControlUtils::ResolveBrake(const FGSTN_WallBrakeInput& BrakeInput, const FVector2D& VelocityOnPlane, const FVector2D& InputOnPlane, bool bTagActive)
{
	if (bTagActive)
	{
		return true;
	}
	if (FMath::Abs(InputOnPlane.Y) < BrakeInput.StickThreshold)
	{
		return false;
	}
	if (BrakeInput.bStickUp && InputOnPlane.Y > 0.f)
	{
		return true;
	}
	return BrakeInput.bStickAgainstMotion && InputOnPlane.Y * VelocityOnPlane.Y < 0.f;
}

FVector2D UGSTN_WallControlUtils::StepSlideVelocity(const FGSTN_WallSlideControlSettings& Settings, const FVector2D& VelocityOnPlane, const FVector2D& InputOnPlane, bool bBrake, float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return VelocityOnPlane;
	}

	// Horizontal: slow decay of the momentum, little control along it, hard brake against it
	float H = VelocityOnPlane.X * FMath::Exp(-Settings.HorizontalDrag * DeltaSeconds);
	const float InputH = InputOnPlane.X;
	if (FMath::Abs(InputH) > Settings.InputDeadzone)
	{
		const bool bAgainst = FMath::Abs(H) > UE_KINDA_SMALL_NUMBER && FMath::Sign(InputH) != FMath::Sign(H);
		if (bAgainst)
		{
			H = FMath::Sign(H) * FMath::Max(FMath::Abs(H) - Settings.HorizontalBrakeDecel * FMath::Abs(InputH) * DeltaSeconds, 0.f);
		}
		else if (FMath::Abs(H) < Settings.HorizontalControlMaxSpeed)
		{
			const float Along = FMath::Sign(InputH);
			H = Along * FMath::Min(FMath::Abs(H) + Settings.HorizontalControlAccel * FMath::Abs(InputH) * DeltaSeconds, Settings.HorizontalControlMaxSpeed);
		}
	}

	// Vertical, as a downward speed S (negative = moving up)
	float S = -VelocityOnPlane.Y;
	if (bBrake)
	{
		if (S > Settings.BrakeMinSpeed)
		{
			S = FMath::Max(S - Settings.VerticalBrakeDecel * DeltaSeconds, Settings.BrakeMinSpeed);
		}
		else if (S < 0.f)
		{
			S = FMath::Min(S + Settings.VerticalBrakeDecel * DeltaSeconds, 0.f);
		}
	}
	else
	{
		const float Gravity = Settings.GravityAcceleration * Settings.GravityScale;
		const float DownInput = FMath::Max(-InputOnPlane.Y, 0.f) > Settings.InputDeadzone ? -InputOnPlane.Y : 0.f;
		const float Terminal = Settings.TerminalDownSpeed + Settings.DownBoost * FMath::Clamp(DownInput, 0.f, 1.f);
		if (S >= Terminal)
		{
			// Entered faster than terminal: decay toward it, never an instant clamp
			S = FMath::Max(S - Gravity * DeltaSeconds, Terminal);
		}
		else
		{
			// Soft approach: the pull fades out quadratically, so terminal is reached asymptotically and never exceeded
			const float Ratio = Terminal > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(S / Terminal, 0.f, 1.f) : 1.f;
			S = FMath::Min(S + Gravity * (1.f - Ratio * Ratio) * DeltaSeconds, Terminal);
		}
	}

	return FVector2D(H, -S);
}

FVector2D UGSTN_WallControlUtils::StepClimbVelocity(const FGSTN_WallClimbControlSettings& Settings, const FVector2D& VelocityOnPlane, const FVector2D& InputOnPlane, float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return VelocityOnPlane;
	}
	FVector2D Input = InputOnPlane;
	const float InputSize = Input.Size();
	FVector2D Target = FVector2D::ZeroVector;
	if (InputSize > Settings.InputDeadzone)
	{
		Target = Input / InputSize * FMath::Min(InputSize, 1.f) * Settings.ClimbSpeed;
	}

	const FVector2D Delta = Target - VelocityOnPlane;
	const float Distance = Delta.Size();
	if (Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return Target;
	}
	const float Rate = Target.SizeSquared() < VelocityOnPlane.SizeSquared() ? Settings.ClimbBrake : Settings.ClimbAccel;
	const float Step = Rate * DeltaSeconds;
	return Step >= Distance ? Target : VelocityOnPlane + Delta / Distance * Step;
}

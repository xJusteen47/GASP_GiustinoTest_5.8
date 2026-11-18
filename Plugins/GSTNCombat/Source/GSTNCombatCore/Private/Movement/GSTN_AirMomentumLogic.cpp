// Pure combat logic: the air momentum model (GSTN_AirMomentumTypes.h).

#include "Movement/GSTN_AirMomentumTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AirMomentumTypes)

namespace
{
	FVector MoveToward(const FVector& From, const FVector& To, double MaxStep)
	{
		const FVector Delta = To - From;
		const double Size = Delta.Size();
		return Size <= MaxStep || Size <= UE_SMALL_NUMBER ? To : From + Delta * (MaxStep / Size);
	}

	FVector Shrink(const FVector& Vector, double Amount)
	{
		const double Size = Vector.Size();
		return Size <= Amount || Size <= UE_SMALL_NUMBER ? FVector::ZeroVector : Vector * ((Size - Amount) / Size);
	}
}

FGSTN_AirMomentumOutput FGSTN_AirMomentumLogic::Step(const FGSTN_AirMomentumTuning& Tuning, const FGSTN_AirMomentumInput& Input)
{
	const double Dt = FMath::Max<double>(Input.DeltaSeconds, 0.0);
	const double Control = FMath::Clamp<double>(Input.Control, 0.0, 1.0);
	const FVector Stick = Input.Stick.GetClampedToMaxSize(1.0);
	const double StickSize = Stick.Size();
	const FVector StickDir = StickSize > UE_KINDA_SMALL_NUMBER ? Stick / StickSize : FVector::ZeroVector;

	// External channel: slow decay, its own drag, and a brake only for the part of the stick that points against it
	FVector External = Input.External;
	External *= 1.0 - FMath::Clamp<double>(Input.ExternalDragPerFrame, 0.0, 1.0);
	double ExternalDecel = Tuning.ExternalDeceleration;
	const double ExternalSize = External.Size();
	if (ExternalSize > UE_KINDA_SMALL_NUMBER && StickSize > UE_KINDA_SMALL_NUMBER)
	{
		const double Against = -StickDir.Dot(External / ExternalSize) * StickSize;
		if (Against > 0.0 && !Input.bDriftDI)
		{
			ExternalDecel += Tuning.ExternalBrake * Against * Control;
		}
	}
	External = Shrink(External, ExternalDecel * Dt);
	if (External.Size() < Tuning.ExternalMinSpeed)
	{
		External = FVector::ZeroVector;
	}

	// Player channel: the rest of the velocity
	const FVector Player = Input.Velocity - Input.External;
	FVector NewPlayer;
	if (StickSize <= UE_KINDA_SMALL_NUMBER)
	{
		NewPlayer = Shrink(Player, Tuning.GetNeutralDeceleration() * Dt);
	}
	else
	{
		// One scalar cap for every direction (no vectoring). The stick along the external push adds nothing past the cap: the player channel
		// may only fill what the external one leaves of it in the stick direction
		const double ExternalAlong = FMath::Max(External.Dot(StickDir), 0.0);
		const double TargetSpeed = FMath::Max(Input.TopSpeed * StickSize - ExternalAlong, 0.0);
		const FVector Target = StickDir * TargetSpeed;
		// Over the cap along the stick (a fast takeoff, a cap that went down): keep that speed, it bleeds at the neutral rate, never braked
		const double PlayerAlong = Player.Dot(StickDir);
		const bool bBraking = PlayerAlong < 0.0;
		if (!bBraking && PlayerAlong > TargetSpeed)
		{
			const FVector Side = Player - StickDir * PlayerAlong;
			const double KeptAlong = FMath::Max(PlayerAlong - Tuning.GetNeutralDeceleration() * Dt, TargetSpeed);
			NewPlayer = StickDir * KeptAlong + MoveToward(Side, FVector::ZeroVector, Tuning.AirAcceleration * Control * Dt);
		}
		else
		{
			const double Rate = (bBraking ? Tuning.AirBrake : Tuning.AirAcceleration) * Control;
			NewPlayer = MoveToward(Player, Target, Rate * Dt);
		}
	}

	FGSTN_AirMomentumOutput Out;
	Out.External = External;
	Out.Velocity = NewPlayer + External;
	return Out;
}

FVector FGSTN_AirMomentumLogic::ClampExternalToMoved(const FVector& External, const FVector& ProposedHorizontal, const FVector& MovedHorizontal, float MinSpeed)
{
	if (External.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	// What the collision took (a wall): the part of the push along it goes too, up to the amount taken. Free flight takes nothing
	FVector Clamped = External;
	const FVector Removed = ProposedHorizontal - MovedHorizontal;
	const double RemovedSize = Removed.Size();
	if (RemovedSize > 1.0)
	{
		const FVector Normal = Removed / RemovedSize;
		const double Into = Clamped.Dot(Normal);
		if (Into > 0.0)
		{
			Clamped -= Normal * FMath::Min(Into, RemovedSize);
		}
	}
	if (Clamped.Dot(MovedHorizontal) <= 0.0)
	{
		return FVector::ZeroVector;
	}
	Clamped = Clamped.GetClampedToMaxSize(MovedHorizontal.Size());
	return Clamped.Size() < MinSpeed ? FVector::ZeroVector : Clamped;
}

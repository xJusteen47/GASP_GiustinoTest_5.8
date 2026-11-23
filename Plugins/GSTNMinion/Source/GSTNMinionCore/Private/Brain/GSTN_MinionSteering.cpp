#include "Brain/GSTN_MinionSteering.h"

FVector2D UGSTN_MinionSteeringUtils::Seek(const FVector2D& From, const FVector2D& To)
{
	const FVector2D Delta = To - From;
	const double Length = Delta.Size();
	return Length > 0.1 ? Delta / Length : FVector2D::ZeroVector;
}

void UGSTN_MinionSteeringUtils::Arrive(const FVector2D& From, const FVector2D& To, float MaxSpeed, float StopRadius, float SlowRadius, FVector2D& OutDirection, float& OutSpeed)
{
	const FVector2D Delta = To - From;
	const double Distance = Delta.Size();
	OutDirection = FVector2D::ZeroVector;
	OutSpeed = 0.f;
	if (Distance <= FMath::Max(static_cast<double>(StopRadius), 0.1))
	{
		return;
	}
	OutDirection = Delta / Distance;
	const double Slow = FMath::Max(static_cast<double>(SlowRadius), 0.0);
	// Linear ramp across the slow ring; with no ring the speed is full right up to the stop radius
	const double Scale = Slow > UE_KINDA_SMALL_NUMBER ? FMath::Clamp((Distance - StopRadius) / Slow, 0.0, 1.0) : 1.0;
	OutSpeed = static_cast<float>(MaxSpeed * Scale);
}

FVector2D UGSTN_MinionSteeringUtils::Separation(const FVector2D& Position, TConstArrayView<FVector2D> Neighbours, float Radius)
{
	if (Radius <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}
	FVector2D Push = FVector2D::ZeroVector;
	const double RadiusSquared = static_cast<double>(Radius) * Radius;
	for (const FVector2D& Neighbour : Neighbours)
	{
		const FVector2D Away = Position - Neighbour;
		const double DistanceSquared = Away.SizeSquared();
		if (DistanceSquared >= RadiusSquared || DistanceSquared < 0.01)
		{
			continue;
		}
		const double Distance = FMath::Sqrt(DistanceSquared);
		Push += (Away / Distance) * (1.0 - Distance / Radius);
	}
	const double Length = Push.Size();
	return Length > 1.0 ? Push / Length : Push;
}

FVector2D UGSTN_MinionSteeringUtils::BlendSeparation(const FVector2D& SeekDirection, const FVector2D& SeparationPush, float SeparationWeight)
{
	const FVector2D Sum = SeekDirection + SeparationPush * FMath::Max(SeparationWeight, 0.f);
	const double Length = Sum.Size();
	return Length > UE_KINDA_SMALL_NUMBER ? Sum / Length : FVector2D::ZeroVector;
}

float UGSTN_MinionSteeringUtils::YawOfDirection(const FVector2D& Direction)
{
	return Direction.SizeSquared() > UE_KINDA_SMALL_NUMBER ? static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X))) : 0.f;
}

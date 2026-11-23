#include "Movement/GSTN_TumbleContact.h"

#include "GSTN_SurfaceUtils.h"

EGSTN_TumbleContactKind FGSTN_TumbleContactSyncState::Classify(const FVector& Normal, const FVector& Up)
{
	const double Along = Normal.GetSafeNormal().Dot(Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
	constexpr double Cos45 = 0.70710678118654752440;
	if (Along >= Cos45)
	{
		return EGSTN_TumbleContactKind::Floor;
	}
	return Along <= -Cos45 ? EGSTN_TumbleContactKind::Ceiling : EGSTN_TumbleContactKind::Wall;
}

FGSTN_TumbleContactSyncState FGSTN_TumbleContactSyncState::FromHit(const FHitResult& Hit, const FVector& Velocity, const FVector& Up, int32 Frame)
{
	FGSTN_TumbleContactSyncState Out;
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const double Into = -Velocity.Dot(Normal);
	if (!Hit.bBlockingHit || Normal.IsNearlyZero() || Into <= 0.0)
	{
		return Out;
	}
	Out.Kind = Classify(Normal, Up);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Out.NormalQ[Axis] = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Normal[Axis] * 32767.0), -32767, 32767));
	}
	Out.ImpactSpeedQ = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(Into), 0, 65535));
	Out.ContactFrame = Frame;
	Out.SurfaceTypeIndex = UGSTN_SurfaceUtils::Resolve(Hit).SurfaceTypeIndex;
	return Out;
}

void FGSTN_TumbleContactSyncState::NetSerialize(FArchive& Ar)
{
	uint8 KindBits = static_cast<uint8>(Kind);
	Ar.SerializeBits(&KindBits, 2);
	Kind = static_cast<EGSTN_TumbleContactKind>(KindBits);
	if (Kind == EGSTN_TumbleContactKind::None)
	{
		if (Ar.IsLoading())
		{
			*this = FGSTN_TumbleContactSyncState();
		}
		return;
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Ar << NormalQ[Axis];
	}
	Ar << ImpactSpeedQ;
	Ar << ContactFrame;
	Ar << SurfaceTypeIndex;
}

bool FGSTN_TumbleContactSyncState::operator==(const FGSTN_TumbleContactSyncState& Other) const
{
	if (Kind != Other.Kind)
	{
		return false;
	}
	return Kind == EGSTN_TumbleContactKind::None
		|| (NormalQ[0] == Other.NormalQ[0] && NormalQ[1] == Other.NormalQ[1] && NormalQ[2] == Other.NormalQ[2] && ImpactSpeedQ == Other.ImpactSpeedQ
			&& ContactFrame == Other.ContactFrame && SurfaceTypeIndex == Other.SurfaceTypeIndex);
}

// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_Pushbox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_Pushbox)

const FGSTN_TimelineTrack_Pushbox* FGSTN_Pushbox::FindTrackAt(const FGSTN_CombatTimelineData& Data, int32 Frame)
{
	if (Frame == INDEX_NONE)
	{
		return nullptr;
	}
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Entry : Data.Tracks)
	{
		const FGSTN_TimelineTrack_Pushbox* Track = Entry.GetPtr<FGSTN_TimelineTrack_Pushbox>();
		if (Track && Track->Contains(Frame))
		{
			return Track;
		}
	}
	return nullptr;
}

bool FGSTN_Pushbox::ResolveLocal(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Capsule,
	const FGSTN_PushboxSettings& Settings, FGSTN_BodyCapsuleShape& OutLocal)
{
	if (!Settings.bEnabled)
	{
		return false;
	}
	OutLocal = Capsule;
	OutLocal.Radius = Capsule.Radius * Settings.CapsuleRadiusScale;
	if (const FGSTN_TimelineTrack_Pushbox* Track = Data ? FindTrackAt(*Data, TimelineFrame) : nullptr)
	{
		if (!Track->bEnabled)
		{
			return false;
		}
		OutLocal.Radius = Track->Radius > 0.f ? Track->Radius : OutLocal.Radius;
		OutLocal.HalfHeight = Track->HalfHeight > 0.f ? Track->HalfHeight : Capsule.HalfHeight;
		OutLocal.Offset = Track->bUseOffset ? Track->Offset : Capsule.Offset;
	}
	return OutLocal.Radius > 0.f && OutLocal.HalfHeight > 0.f;
}

FVector FGSTN_Pushbox::PairShare(const FGSTN_PushboxBody& Self, const FGSTN_PushboxBody& Other, const FGSTN_PushboxSettings& Settings, const FVector& Up)
{
	if (Self.bFrozen || Other.bFrozen || !Self.Cylinder.IsActive() || !Other.Cylinder.IsActive() || Self.EntityId == Other.EntityId)
	{
		return FVector::ZeroVector;
	}
	const FVector Delta = Self.Cylinder.Center - Other.Cylinder.Center;
	const double Vertical = FVector::DotProduct(Delta, Up);
	if (FMath::Abs(Vertical) >= Self.Cylinder.HalfHeight + Other.Cylinder.HalfHeight)
	{
		return FVector::ZeroVector;
	}
	const FVector Horizontal = Delta - Vertical * Up;
	const double Distance = Horizontal.Size();
	const double Penetration = static_cast<double>(Self.Cylinder.Radius) + Other.Cylinder.Radius - Distance;
	if (Penetration <= 0.0)
	{
		return FVector::ZeroVector;
	}
	FVector Direction;
	if (Distance > UE_KINDA_SMALL_NUMBER)
	{
		Direction = Horizontal / Distance;
	}
	else
	{
		// Coincident: a world-fixed axis in the plane, the same on both sides, signed by the id order so the shares stay opposite
		FVector Axis = FVector::ForwardVector - FVector::DotProduct(FVector::ForwardVector, Up) * Up;
		if (!Axis.Normalize())
		{
			Axis = (FVector::RightVector - FVector::DotProduct(FVector::RightVector, Up) * Up).GetSafeNormal();
		}
		Direction = Self.EntityId < Other.EntityId ? -Axis : Axis;
	}
	const double Share = FMath::Min(Settings.PushGainPerFrame * Penetration * 0.5, Settings.MaxPushPerFrame * 0.5);
	return Direction * Share;
}

FVector FGSTN_Pushbox::ComputePush(const FGSTN_PushboxBody& Self, TConstArrayView<FGSTN_PushboxBody> Others, const FGSTN_PushboxSettings& Settings, const FVector& Up)
{
	if (!Settings.bEnabled || !Self.Cylinder.IsActive())
	{
		return FVector::ZeroVector;
	}
	FVector Push = FVector::ZeroVector;
	for (const FGSTN_PushboxBody& Other : Others)
	{
		Push += PairShare(Self, Other, Settings, Up);
	}
	return Push.GetClampedToMaxSize(Settings.MaxPushPerFrame);
}

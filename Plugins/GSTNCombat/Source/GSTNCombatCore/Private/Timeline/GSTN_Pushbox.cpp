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

FGSTN_PushboxSettings::FGSTN_PushboxSettings()
{
	// Default data, by name like FGSTN_GrabRules::IgnoreTagNames: a downed body is crossed (owner 2A), a tumbling one only meets the world (plan 4)
	FGSTN_PushboxStateRule& Knockdown = StateRules.AddDefaulted_GetRef();
	Knockdown.StateTag = FName(TEXT("Combat.State.Knockdown"));
	FGSTN_PushboxStateRule& Tumble = StateRules.AddDefaulted_GetRef();
	Tumble.StateTag = FName(TEXT("Combat.State.Tumble"));
}

bool FGSTN_Pushbox::ResolveLocal(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Capsule,
	const FGSTN_PushboxSettings& Settings, FGSTN_BodyCapsuleShape& OutLocal)
{
	return ResolveLocal(Data, TimelineFrame, Capsule, Settings, TConstArrayView<FName>(), OutLocal);
}

const FGSTN_PushboxStateRule* FGSTN_Pushbox::FindStateRule(const FGSTN_PushboxSettings& Settings, TConstArrayView<FName> StateTags)
{
	for (const FGSTN_PushboxStateRule& Rule : Settings.StateRules)
	{
		if (!Rule.StateTag.IsNone() && StateTags.Contains(Rule.StateTag))
		{
			return &Rule;
		}
	}
	return nullptr;
}

bool FGSTN_Pushbox::ResolveLocal(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Capsule,
	const FGSTN_PushboxSettings& Settings, TConstArrayView<FName> StateTags, FGSTN_BodyCapsuleShape& OutLocal)
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
	else if (const FGSTN_PushboxStateRule* Rule = FindStateRule(Settings, StateTags))
	{
		if (!Rule->bEnabled)
		{
			return false;
		}
		OutLocal.Radius = Rule->Radius > 0.f ? Rule->Radius : OutLocal.Radius;
		OutLocal.HalfHeight = Rule->HalfHeight > 0.f ? Rule->HalfHeight : Capsule.HalfHeight;
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
	// (ConstrainMove uses the same tie axis, GSTNPushboxPrivate::TieAxis)
	const double Cap = Settings.MaxPushPerFrame + Settings.PushCapGrowth * Penetration;
	const double Share = FMath::Min(Settings.PushGainPerFrame * Penetration * 0.5, Cap * 0.5);
	return Direction * Share;
}

namespace GSTNPushboxPrivate
{
	/** The horizontal part of V (perpendicular to Up) */
	static FVector Horizontal(const FVector& V, const FVector& Up)
	{
		return V - FVector::DotProduct(V, Up) * Up;
	}

	/** Coincident centres: the world-fixed in-plane axis PairShare uses, signed by id order so the pair's answers stay opposite */
	static FVector TieAxis(int32 SelfId, int32 OtherId, const FVector& Up)
	{
		FVector Axis = Horizontal(FVector::ForwardVector, Up);
		if (!Axis.Normalize())
		{
			Axis = Horizontal(FVector::RightVector, Up).GetSafeNormal();
		}
		return SelfId < OtherId ? -Axis : Axis;
	}
}

FVector FGSTN_Pushbox::ComputePush(const FGSTN_PushboxBody& Self, TConstArrayView<FGSTN_PushboxBody> Others, const FGSTN_PushboxSettings& Settings, const FVector& Up)
{
	if (!Settings.bEnabled || !Self.Cylinder.IsActive())
	{
		return FVector::ZeroVector;
	}
	FVector Push = FVector::ZeroVector;
	double Deepest = 0.0;
	for (const FGSTN_PushboxBody& Other : Others)
	{
		const FVector Share = PairShare(Self, Other, Settings, Up);
		if (!Share.IsZero())
		{
			Push += Share;
			const double Distance = GSTNPushboxPrivate::Horizontal(Self.Cylinder.Center - Other.Cylinder.Center, Up).Size();
			Deepest = FMath::Max(Deepest, static_cast<double>(Self.Cylinder.Radius) + Other.Cylinder.Radius - Distance);
		}
	}
	return Push.GetClampedToMaxSize(Settings.MaxPushPerFrame + Settings.PushCapGrowth * Deepest);
}

FVector FGSTN_Pushbox::ConstrainMove(const FGSTN_PushboxBody& Self, const FVector& Move, TConstArrayView<FGSTN_PushboxBody> Others,
	const FGSTN_PushboxSettings& Settings, const FVector& Up, FVector* InOutVelocity)
{
	using namespace GSTNPushboxPrivate;
	if (!Settings.bEnabled || !Self.Cylinder.IsActive())
	{
		return Move;
	}
	FVector Result = Move;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FGSTN_PushboxBody& Other : Others)
		{
			if (Other.EntityId == Self.EntityId || !Other.Cylinder.IsActive())
			{
				continue;
			}
			// Vertical spans at the start of the frame: a body falling onto a head meets its core only once the spans overlap
			const FVector Delta = Self.Cylinder.Center - Other.Cylinder.Center;
			if (FMath::Abs(FVector::DotProduct(Delta, Up)) >= static_cast<double>(Self.Cylinder.HalfHeight) + Other.Cylinder.HalfHeight)
			{
				continue;
			}
			const FVector StartH = Horizontal(Delta, Up);
			const double StartDistance = StartH.Size();
			const double PushSum = static_cast<double>(Self.Cylinder.Radius) + Other.Cylinder.Radius;
			const bool bCore = Settings.bCoreEnabled && Self.Cylinder.CoreRadius > 0.f && Other.Cylinder.CoreRadius > 0.f;
			const double CoreSum = bCore ? static_cast<double>(Self.Cylinder.CoreRadius) + Other.Cylinder.CoreRadius : 0.0;
			if (StartDistance >= FMath::Max(PushSum, CoreSum))
			{
				continue;
			}
			const FVector Normal = StartDistance > UE_KINDA_SMALL_NUMBER ? StartH / StartDistance : TieAxis(Self.EntityId, Other.EntityId, Up);

			// Approach clamp, once: keep from 1 at the pushbox edge down to ApproachKeptAtCore at the core of the move toward the other
			if (Pass == 0)
			{
				const double Inward = FVector::DotProduct(Result, Normal);
				if (Inward < 0.0)
				{
					const double Span = PushSum - CoreSum;
					const double T = Span > UE_KINDA_SMALL_NUMBER ? FMath::Clamp((StartDistance - CoreSum) / Span, 0.0, 1.0) : 0.0;
					const double Kept = FMath::Lerp(static_cast<double>(Settings.ApproachKeptAtCore), 1.0, T);
					Result -= (1.0 - Kept) * Inward * Normal;
					if (InOutVelocity)
					{
						const double InwardVelocity = FVector::DotProduct(*InOutVelocity, Normal);
						if (InwardVelocity < 0.0)
						{
							*InOutVelocity -= (1.0 - Kept) * InwardVelocity * Normal;
						}
					}
				}
			}

			// Rigid core: the end of the move may close at most half the free gap to the other's start (the other may close the other half from
			// its side in the same frame: both read the start). Already inside: no closer than where it started plus half the recovery
			if (bCore)
			{
				const double MinDistance = StartDistance >= CoreSum ? CoreSum + (StartDistance - CoreSum) * 0.5
					: StartDistance + FMath::Min(CoreSum - StartDistance, static_cast<double>(Settings.CoreRecoverPerFrame)) * 0.5;
				const FVector EndH = StartH + Horizontal(Result, Up);
				const double EndDistance = EndH.Size();
				if (EndDistance < MinDistance)
				{
					const FVector Out = EndDistance > UE_KINDA_SMALL_NUMBER ? EndH / EndDistance : Normal;
					Result += Out * (MinDistance - EndDistance);
					if (InOutVelocity)
					{
						const double InwardVelocity = FVector::DotProduct(*InOutVelocity, Out);
						if (InwardVelocity < 0.0)
						{
							*InOutVelocity -= InwardVelocity * Out;
						}
					}
				}
			}
		}
	}
	return Result;
}

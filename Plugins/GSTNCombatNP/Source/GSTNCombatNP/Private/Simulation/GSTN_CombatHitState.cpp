// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatHitState.h"

#include "Serialization/Archive.h"

namespace
{
	uint32 ZigZag(int32 Value)
	{
		return (static_cast<uint32>(Value) << 1) ^ static_cast<uint32>(Value >> 31);
	}

	int32 UnZigZag(uint32 Value)
	{
		return static_cast<int32>((Value >> 1) ^ (~(Value & 1u) + 1u));
	}

	uint16 QuantizeAngle(double Degrees)
	{
		const double Turn = FRotator::ClampAxis(Degrees) / 360.0;
		return static_cast<uint16>(FMath::RoundToInt(Turn * FGSTN_MoveOrigin::RotationUnitsPerTurn) & 0xFFFF);
	}

	double DequantizeAngle(uint16 Units)
	{
		return static_cast<double>(Units) * 360.0 / FGSTN_MoveOrigin::RotationUnitsPerTurn;
	}

	// Bits of the presence mask on the wire
	enum EHitPart : uint8
	{
		PartMoveOrigin = 1 << 0,
		PartTargets = 1 << 1,
		PartConfirm = 1 << 2,
		PartHitstun = 1 << 3,
		PartImpact = 1 << 4,
		PartLaunch = 1 << 5,
		PartSDI = 1 << 6,
		PartProfile = 1 << 7,
		PartCount = 8
	};

	void SerializeFramePlusOne(FArchive& Ar, int32& Frame)
	{
		uint32 Packed = static_cast<uint32>(FMath::Max(Frame + 1, 0));
		Ar.SerializeIntPacked(Packed);
		if (Ar.IsLoading())
		{
			Frame = static_cast<int32>(FMath::Min<uint32>(Packed, 0x7FFFFFFFu)) - 1;
		}
	}
}

FGSTN_MoveOrigin FGSTN_MoveOrigin::Make(const FTransform& Transform)
{
	FGSTN_MoveOrigin Origin;
	Origin.bSet = true;
	const FVector Location = Transform.GetLocation();
	constexpr double Limit = 100000000.0;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Origin.PositionQ[Axis] = FMath::RoundToInt(FMath::Clamp(Location[Axis] * PositionUnitsPerCm, -Limit, Limit));
	}
	const FRotator Rotation = Transform.Rotator();
	Origin.RotationQ[0] = QuantizeAngle(Rotation.Pitch);
	Origin.RotationQ[1] = QuantizeAngle(Rotation.Yaw);
	Origin.RotationQ[2] = QuantizeAngle(Rotation.Roll);
	return Origin;
}

FTransform FGSTN_MoveOrigin::ToTransform() const
{
	const FVector Location(PositionQ[0] / PositionUnitsPerCm, PositionQ[1] / PositionUnitsPerCm, PositionQ[2] / PositionUnitsPerCm);
	return FTransform(FRotator(DequantizeAngle(RotationQ[0]), DequantizeAngle(RotationQ[1]), DequantizeAngle(RotationQ[2])), Location);
}

bool FGSTN_MoveOrigin::operator==(const FGSTN_MoveOrigin& Other) const
{
	if (bSet != Other.bSet)
	{
		return false;
	}
	if (!bSet)
	{
		return true;
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (PositionQ[Axis] != Other.PositionQ[Axis] || RotationQ[Axis] != Other.RotationQ[Axis])
		{
			return false;
		}
	}
	return true;
}

void FGSTN_HitSyncState::SetPendingLaunch(const FVector& Velocity)
{
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		PendingLaunchQ[Axis] = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Velocity[Axis] / 4.0), -32767, 32767));
	}
}

FVector FGSTN_HitSyncState::GetPendingLaunch() const
{
	return FVector(PendingLaunchQ[0] * 4.0, PendingLaunchQ[1] * 4.0, PendingLaunchQ[2] * 4.0);
}

const FGSTN_HitTarget& FGSTN_HitSyncState::GetHitTarget(int32 AgeIndex) const
{
	check(AgeIndex >= 0 && AgeIndex < HitTargetsNum);
	return HitTargets[(HitTargetsHead + MaxHitTargets - HitTargetsNum + AgeIndex) % MaxHitTargets];
}

bool FGSTN_HitSyncState::AddHitTarget(int32 EntityId, int32 TimelineStartFrame, uint8 HitGroup)
{
	const bool bReplaced = HitTargetsNum == MaxHitTargets;
	FGSTN_HitTarget& Slot = HitTargets[HitTargetsHead % MaxHitTargets];
	Slot.EntityId = EntityId;
	Slot.TimelineStartFrame = TimelineStartFrame;
	Slot.HitGroup = HitGroup;
	HitTargetsHead = static_cast<uint8>((HitTargetsHead + 1) % MaxHitTargets);
	HitTargetsNum = static_cast<uint8>(FMath::Min<int32>(HitTargetsNum + 1, MaxHitTargets));
	return bReplaced;
}

bool FGSTN_HitSyncState::HasHitTarget(int32 EntityId, int32 TimelineStartFrame, uint8 HitGroup) const
{
	for (int32 Age = 0; Age < HitTargetsNum; ++Age)
	{
		if (GetHitTarget(Age) == FGSTN_HitTarget{ EntityId, TimelineStartFrame, HitGroup })
		{
			return true;
		}
	}
	return false;
}

FGSTN_HitSyncState FGSTN_HitSyncState::Canonical(const FGSTN_HitLiveContext& Context) const
{
	FGSTN_HitSyncState Out;
	if (Context.bInTimeline)
	{
		Out.MoveOrigin = MoveOrigin;
		Out.HitConfirmFlags = HitConfirmFlags;
		for (int32 Age = 0; Age < HitTargetsNum; ++Age)
		{
			const FGSTN_HitTarget& Target = GetHitTarget(Age);
			if (Target.TimelineStartFrame == Context.TimelineStartFrame)
			{
				Out.HitTargets[Out.HitTargetsNum++] = Target;
			}
		}
		Out.HitTargetsHead = static_cast<uint8>(Out.HitTargetsNum % MaxHitTargets);
	}
	if (HitstunEndFrame > Context.Frame)
	{
		Out.HitstunEndFrame = HitstunEndFrame;
	}
	if (ImpactEndFrame > Context.Frame)
	{
		Out.ImpactEndFrame = ImpactEndFrame;
	}
	if (Context.bTumbling)
	{
		Out.ImpactProfileQ = ImpactProfileQ;
		Out.GroundBounceCount = GroundBounceCount;
		Out.GroundBounceFirstFrame = GroundBounceCount != 0 ? GroundBounceFirstFrame : INDEX_NONE;
		Out.WallBounceCount = WallBounceCount;
		Out.WallBounceFirstFrame = WallBounceCount != 0 ? WallBounceFirstFrame : INDEX_NONE;
		if (BumpEndFrame > Context.Frame && BumpKind != 0)
		{
			Out.BumpEndFrame = BumpEndFrame;
			Out.BumpKind = BumpKind;
		}
		if (SurfaceImpactKind != 0)
		{
			Out.SurfaceImpactFrame = SurfaceImpactFrame;
			Out.SurfaceImpactKind = SurfaceImpactKind;
		}
	}
	if (Context.HitstopEndFrame > Context.Frame)
	{
		Out.PendingReaction = PendingReaction;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Out.PendingLaunchQ[Axis] = PendingLaunchQ[Axis];
		}
		// The balloon belongs to the launch: without one there is nothing to warp (and nothing of it goes on the wire)
		Out.PendingBalloonFrames = (PendingLaunchQ[0] | PendingLaunchQ[1] | PendingLaunchQ[2]) != 0 ? PendingBalloonFrames : 0;
		Out.SDIDistanceUsed = SDIDistanceUsed;
	}
	return Out;
}

bool FGSTN_HitSyncState::operator==(const FGSTN_HitSyncState& Other) const
{
	if (!(MoveOrigin == Other.MoveOrigin) || HitTargetsNum != Other.HitTargetsNum || HitConfirmFlags != Other.HitConfirmFlags
		|| HitstunEndFrame != Other.HitstunEndFrame || ImpactEndFrame != Other.ImpactEndFrame || PendingReaction != Other.PendingReaction
		|| SDIDistanceUsed != Other.SDIDistanceUsed || ImpactProfileQ != Other.ImpactProfileQ || PendingBalloonFrames != Other.PendingBalloonFrames
		|| GroundBounceCount != Other.GroundBounceCount || GroundBounceFirstFrame != Other.GroundBounceFirstFrame
		|| WallBounceCount != Other.WallBounceCount || WallBounceFirstFrame != Other.WallBounceFirstFrame || BumpEndFrame != Other.BumpEndFrame
		|| BumpKind != Other.BumpKind || SurfaceImpactFrame != Other.SurfaceImpactFrame || SurfaceImpactKind != Other.SurfaceImpactKind)
	{
		return false;
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (PendingLaunchQ[Axis] != Other.PendingLaunchQ[Axis])
		{
			return false;
		}
	}
	for (int32 Age = 0; Age < HitTargetsNum; ++Age)
	{
		if (!(GetHitTarget(Age) == Other.GetHitTarget(Age)))
		{
			return false;
		}
	}
	return true;
}

bool FGSTN_HitSyncState::IsDefault(const FGSTN_HitLiveContext& Context) const
{
	return Canonical(Context) == FGSTN_HitSyncState();
}

void FGSTN_HitSyncState::NetSerialize(FArchive& Ar, const FGSTN_HitLiveContext& Context)
{
	FGSTN_HitSyncState Local = Ar.IsSaving() ? Canonical(Context) : FGSTN_HitSyncState();

	uint8 bLive = Ar.IsSaving() && !(Local == FGSTN_HitSyncState()) ? 1 : 0;
	Ar.SerializeBits(&bLive, 1);
	if (!bLive)
	{
		if (Ar.IsLoading())
		{
			*this = FGSTN_HitSyncState();
		}
		return;
	}

	uint8 Mask = 0;
	if (Ar.IsSaving())
	{
		Mask |= Local.MoveOrigin.bSet ? PartMoveOrigin : 0;
		Mask |= Local.HitTargetsNum > 0 ? PartTargets : 0;
		Mask |= Local.HitConfirmFlags != 0 ? PartConfirm : 0;
		Mask |= Local.HitstunEndFrame != INDEX_NONE ? PartHitstun : 0;
		Mask |= Local.ImpactEndFrame != INDEX_NONE ? PartImpact : 0;
		Mask |= (Local.PendingLaunchQ[0] | Local.PendingLaunchQ[1] | Local.PendingLaunchQ[2]) != 0 ? PartLaunch : 0;
		Mask |= Local.SDIDistanceUsed != 0 ? PartSDI : 0;
		Mask |= Local.ImpactProfileQ != 0 || Local.GroundBounceCount != 0 || Local.WallBounceCount != 0 || Local.BumpKind != 0
			|| Local.SurfaceImpactKind != 0 ? PartProfile : 0;
	}
	Ar.SerializeBits(&Mask, PartCount);
	uint8 Reaction = Local.PendingReaction;
	Ar.SerializeBits(&Reaction, 2);

	if (Mask & PartMoveOrigin)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			uint32 Packed = ZigZag(Local.MoveOrigin.PositionQ[Axis]);
			Ar.SerializeIntPacked(Packed);
			Local.MoveOrigin.PositionQ[Axis] = UnZigZag(Packed);
		}
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Ar << Local.MoveOrigin.RotationQ[Axis];
		}
		Local.MoveOrigin.bSet = true;
	}
	if (Mask & PartTargets)
	{
		// The targets of a live canonical state all belong to the running timeline: only the entity and the group go on the wire
		// The block is present only with at least one target: Count - 1 fits the ring of 16 in the same 4 bits
		static_assert(MaxHitTargets >= 1 && MaxHitTargets <= 16, "the target count goes on the wire as Count - 1 in 4 bits");
		uint8 CountMinusOne = static_cast<uint8>(FMath::Max<int32>(Local.HitTargetsNum - 1, 0));
		Ar.SerializeBits(&CountMinusOne, 4);
		const uint8 Count = static_cast<uint8>(CountMinusOne + 1);
		if (Count > MaxHitTargets)
		{
			Ar.SetError();
			return;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGSTN_HitTarget& Target = Local.HitTargets[Index];
			uint32 IdPlusOne = static_cast<uint32>(FMath::Max(Target.EntityId + 1, 0));
			Ar.SerializeIntPacked(IdPlusOne);
			Ar << Target.HitGroup;
			if (Ar.IsLoading())
			{
				Target.EntityId = static_cast<int32>(FMath::Min<uint32>(IdPlusOne, 0x7FFFFFFFu)) - 1;
				Target.TimelineStartFrame = Context.TimelineStartFrame;
			}
		}
		Local.HitTargetsNum = Count;
		Local.HitTargetsHead = static_cast<uint8>(Count % MaxHitTargets);
	}
	if (Mask & PartConfirm)
	{
		Ar << Local.HitConfirmFlags;
	}
	if (Mask & PartHitstun)
	{
		SerializeFramePlusOne(Ar, Local.HitstunEndFrame);
	}
	if (Mask & PartImpact)
	{
		SerializeFramePlusOne(Ar, Local.ImpactEndFrame);
	}
	if (Mask & PartLaunch)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Ar << Local.PendingLaunchQ[Axis];
		}
		uint8 bBalloon = Local.PendingBalloonFrames != 0 ? 1 : 0;
		Ar.SerializeBits(&bBalloon, 1);
		if (bBalloon)
		{
			Ar << Local.PendingBalloonFrames;
		}
	}
	if (Mask & PartSDI)
	{
		Ar << Local.SDIDistanceUsed;
	}
	if (Mask & PartProfile)
	{
		Ar << Local.ImpactProfileQ;
		// The ground bounce chain rides with the profile (both live while tumbling): 1 bit, then the count (4 bits) and the first frame
		uint8 bChain = Local.GroundBounceCount != 0 ? 1 : 0;
		Ar.SerializeBits(&bChain, 1);
		if (bChain)
		{
			uint8 Count = static_cast<uint8>(FMath::Min<int32>(Local.GroundBounceCount, 15));
			Ar.SerializeBits(&Count, 4);
			Local.GroundBounceCount = Count;
			SerializeFramePlusOne(Ar, Local.GroundBounceFirstFrame);
		}
		// The wall and ceiling chain, the same way, then the bump: 1 bit, its face (2 bits) and its end frame
		uint8 bWallChain = Local.WallBounceCount != 0 ? 1 : 0;
		Ar.SerializeBits(&bWallChain, 1);
		if (bWallChain)
		{
			uint8 Count = static_cast<uint8>(FMath::Min<int32>(Local.WallBounceCount, 15));
			Ar.SerializeBits(&Count, 4);
			Local.WallBounceCount = Count;
			SerializeFramePlusOne(Ar, Local.WallBounceFirstFrame);
		}
		uint8 bBump = Local.BumpKind != 0 ? 1 : 0;
		Ar.SerializeBits(&bBump, 1);
		if (bBump)
		{
			uint8 Kind = Local.BumpKind;
			Ar.SerializeBits(&Kind, 2);
			Local.BumpKind = Kind;
			SerializeFramePlusOne(Ar, Local.BumpEndFrame);
		}
		// The last wall or ceiling contact: 1 bit, its face (2 bits) and its frame
		uint8 bSurface = Local.SurfaceImpactKind != 0 ? 1 : 0;
		Ar.SerializeBits(&bSurface, 1);
		if (bSurface)
		{
			uint8 Kind = Local.SurfaceImpactKind;
			Ar.SerializeBits(&Kind, 2);
			Local.SurfaceImpactKind = Kind;
			SerializeFramePlusOne(Ar, Local.SurfaceImpactFrame);
		}
	}

	if (Ar.IsLoading())
	{
		Local.PendingReaction = Reaction;
		*this = Local;
	}
}

FGSTN_HitSyncState FGSTN_HitSyncState::Interpolated(const FGSTN_HitSyncState* From, const FGSTN_HitSyncState& To, float PCT)
{
	FGSTN_HitSyncState Out = To;
	if (From && From->MoveOrigin.bSet && To.MoveOrigin.bSet)
	{
		const double Alpha = FMath::Clamp(static_cast<double>(PCT), 0.0, 1.0);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const double A = From->MoveOrigin.PositionQ[Axis];
			const double B = To.MoveOrigin.PositionQ[Axis];
			Out.MoveOrigin.PositionQ[Axis] = FMath::RoundToInt(A + (B - A) * Alpha);

			// Shortest way round the circle on the 16 bit angle
			const int32 Delta = static_cast<int16>(static_cast<uint16>(To.MoveOrigin.RotationQ[Axis] - From->MoveOrigin.RotationQ[Axis]));
			Out.MoveOrigin.RotationQ[Axis] = static_cast<uint16>(From->MoveOrigin.RotationQ[Axis] + FMath::RoundToInt(Delta * Alpha));
		}
	}
	return Out;
}

void FGSTN_HitSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf(" Hit: targets %d confirm %d stun->%d impact->%d profile %d pending %d launch(%d,%d,%d)", static_cast<int32>(HitTargetsNum),
		static_cast<int32>(HitConfirmFlags), HitstunEndFrame, ImpactEndFrame, static_cast<int32>(ImpactProfileQ), static_cast<int32>(PendingReaction),
		static_cast<int32>(PendingLaunchQ[0]), static_cast<int32>(PendingLaunchQ[1]), static_cast<int32>(PendingLaunchQ[2]));
	if (GroundBounceCount != 0)
	{
		Out.Appendf(" groundBounces %d from %d", static_cast<int32>(GroundBounceCount), GroundBounceFirstFrame);
	}
	if (WallBounceCount != 0)
	{
		Out.Appendf(" wallBounces %d from %d", static_cast<int32>(WallBounceCount), WallBounceFirstFrame);
	}
	if (BumpKind != 0)
	{
		Out.Appendf(" bump %d->%d", static_cast<int32>(BumpKind), BumpEndFrame);
	}
	if (SurfaceImpactKind != 0)
	{
		Out.Appendf(" surface %d@%d", static_cast<int32>(SurfaceImpactKind), SurfaceImpactFrame);
	}
	if (MoveOrigin.bSet)
	{
		Out.Appendf(" origin(%d,%d,%d)", MoveOrigin.PositionQ[0], MoveOrigin.PositionQ[1], MoveOrigin.PositionQ[2]);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/GSTN_WallMovementTypes.h"

#include "Engine/NetSerialization.h"
#include "MoveLibrary/RollbackBlackboardWrappers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMovementTypes)

namespace GSTNWallBlackboard
{
	const FName EntryInfo = TEXT("GSTN_WallEntryInfo");
	const FName BaseInfo = TEXT("GSTN_WallBaseInfo");

	void EnsureEntries(FRollbackBlackboardSimWrapper& Blackboard)
	{
		// Same shape as the one Mover keeps for its own per-frame entries. Written at the end of a tick and read by the next one,
		// so the value must persist across frames; 64 slots = the deepest rollback Network Prediction can rewind, so a re-simulation
		// finds the value of the frame before it instead of an invalidated entry
		URollbackBlackboard::EntrySettings Settings;
		Settings.SizingPolicy = EBlackboardSizingPolicy::FixedDeclaredSize;
		Settings.FixedSize = 64;
		Settings.PersistencePolicy = EBlackboardPersistencePolicy::Forever;
		Settings.RollbackPolicy = EBlackboardRollbackPolicy::InvalidatedOnRollback;

		// CreateEntry keeps the first entry (and returns false) if it already exists
		if (!Blackboard.HasEntry(EntryInfo))
		{
			Blackboard.CreateEntry<FGSTN_WallEntryInfo>(EntryInfo, Settings);
		}
		if (!Blackboard.HasEntry(BaseInfo))
		{
			Blackboard.CreateEntry<FGSTN_WallBaseInfo>(BaseInfo, Settings);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// FGSTN_WallInputs
// ------------------------------------------------------------------------------------------------

bool FGSTN_WallInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	uint8 bDetachBit = bWantsToDetach ? 1 : 0;
	Ar.SerializeBits(&bDetachBit, 1);
	bWantsToDetach = (bDetachBit != 0);

	bOutSuccess = true;
	return true;
}

void FGSTN_WallInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("WantsToDetach: %d\n", bWantsToDetach ? 1 : 0);
}

// ------------------------------------------------------------------------------------------------
// FGSTN_WallMovementSyncState
// ------------------------------------------------------------------------------------------------

FMoverDataStructBase* FGSTN_WallMovementSyncState::Clone() const
{
	return new FGSTN_WallMovementSyncState(*this);
}

bool FGSTN_WallMovementSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	// The reset time is sent whenever it is set, even without memory: the predicted and the authority state must agree on it
	// (ShouldReconcile compares it), and a later wall grab can only make it stale, never wrong
	const bool bHasResetTime = MemoryResetTimeMs >= 0.0;
	uint8 Flags = (bHasWall ? 1 : 0) | (bIsSkidding ? 2 : 0) | (bHasWallMemory ? 4 : 0) | (bHasResetTime ? 8 : 0) | (SurfaceSwitchTicks > 0 ? 16 : 0);
	Ar.SerializeBits(&Flags, 5);
	bHasWall = (Flags & 1) != 0;
	bIsSkidding = (Flags & 2) != 0;
	bHasWallMemory = (Flags & 4) != 0;

	SerializeFixedVector<1, 16>(WallNormal, Ar);
	SerializePackedVector<100, 30>(WallContactPoint, Ar);
	Ar << TimeWithoutWall;
	Ar << PullAwayTime;

	if (bHasWallMemory)
	{
		SerializeFixedVector<1, 16>(LastWallNormal, Ar);
		Ar << LastWallHeight;
		Ar << LastWallTimeMs;
	}
	else if (Ar.IsLoading())
	{
		LastWallNormal = FVector::ZeroVector;
		LastWallHeight = 0.f;
		LastWallTimeMs = 0.0;
	}

	if ((Flags & 8) != 0)
	{
		Ar << MemoryResetTimeMs;
	}
	else if (Ar.IsLoading())
	{
		MemoryResetTimeMs = -1.0;
	}

	if ((Flags & 16) != 0)
	{
		Ar << SurfaceSwitchTicks;
	}
	else if (Ar.IsLoading())
	{
		SurfaceSwitchTicks = 0;
	}

	bOutSuccess = true;
	return true;
}

void FGSTN_WallMovementSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("HasWall: %d  TimeWithoutWall: %.2f  Skidding: %d  PullAway: %.2f\n", bHasWall ? 1 : 0, TimeWithoutWall, bIsSkidding ? 1 : 0, PullAwayTime);
	Out.Appendf("WallNormal: X=%.3f Y=%.3f Z=%.3f\n", WallNormal.X, WallNormal.Y, WallNormal.Z);
	Out.Appendf("WallContact: X=%.2f Y=%.2f Z=%.2f\n", WallContactPoint.X, WallContactPoint.Y, WallContactPoint.Z);
	if (bHasWallMemory)
	{
		Out.Appendf("LastWall: N=(%.2f %.2f %.2f) H=%.1f T=%.0fms\n", LastWallNormal.X, LastWallNormal.Y, LastWallNormal.Z, LastWallHeight, LastWallTimeMs);
	}
	if (MemoryResetTimeMs >= 0.0)
	{
		Out.Appendf("MemoryResetTime: %.0fms\n", MemoryResetTimeMs);
	}
}

bool FGSTN_WallMovementSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FGSTN_WallMovementSyncState& Authority = static_cast<const FGSTN_WallMovementSyncState&>(AuthorityState);
	constexpr float NormalCosTolerance = 0.996f; // ~5 degrees
	// The timers feed thresholds (NoWallGraceTime, PullAwayDelay, ReentryCooldown, HeightCapTolerance), so a drift larger than
	// float noise changes a decision a few ticks later. Tolerances are a fraction of a tick / of a centimeter-scale tolerance
	constexpr float TimerToleranceSeconds = 0.02f;
	constexpr float HeightToleranceCm = 2.f;
	constexpr double TimeToleranceMs = 1.0;

	// Discrete state: exact
	if (bHasWall != Authority.bHasWall
		|| bIsSkidding != Authority.bIsSkidding
		|| bHasWallMemory != Authority.bHasWallMemory
		|| (MemoryResetTimeMs >= 0.0) != (Authority.MemoryResetTimeMs >= 0.0))
	{
		return true;
	}

	if (!FMath::IsNearlyEqual(TimeWithoutWall, Authority.TimeWithoutWall, TimerToleranceSeconds)
		|| !FMath::IsNearlyEqual(PullAwayTime, Authority.PullAwayTime, TimerToleranceSeconds))
	{
		return true;
	}

	// Only compare normals while on a wall: off-wall both normals are zero and their dot product (0) would always mismatch.
	if (bHasWall && FVector::DotProduct(WallNormal, Authority.WallNormal) < NormalCosTolerance)
	{
		return true;
	}

	// Memory fields only travel (and only mean something) with bHasWallMemory
	// (a zero normal on both sides is a match: its dot product would be 0)
	const bool bLastNormalDiffers = !(LastWallNormal.IsNearlyZero() && Authority.LastWallNormal.IsNearlyZero())
		&& FVector::DotProduct(LastWallNormal, Authority.LastWallNormal) < NormalCosTolerance;
	if (bHasWallMemory
		&& (bLastNormalDiffers
			|| !FMath::IsNearlyEqual(LastWallHeight, Authority.LastWallHeight, HeightToleranceCm)
			|| !FMath::IsNearlyEqual(LastWallTimeMs, Authority.LastWallTimeMs, TimeToleranceMs)))
	{
		return true;
	}

	return MemoryResetTimeMs >= 0.0 && !FMath::IsNearlyEqual(MemoryResetTimeMs, Authority.MemoryResetTimeMs, TimeToleranceMs);
}

void FGSTN_WallMovementSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FGSTN_WallMovementSyncState& FromState = static_cast<const FGSTN_WallMovementSyncState&>(From);
	const FGSTN_WallMovementSyncState& ToState = static_cast<const FGSTN_WallMovementSyncState&>(To);

	const FVector LerpedNormal = FMath::Lerp(FromState.WallNormal, ToState.WallNormal, Pct);
	WallNormal = LerpedNormal.GetSafeNormal(UE_SMALL_NUMBER, ToState.WallNormal);
	WallContactPoint = FMath::Lerp(FromState.WallContactPoint, ToState.WallContactPoint, Pct);
	TimeWithoutWall = FMath::Lerp(FromState.TimeWithoutWall, ToState.TimeWithoutWall, Pct);
	bHasWall = ToState.bHasWall;
	bIsSkidding = ToState.bIsSkidding;
	PullAwayTime = ToState.PullAwayTime;
	bHasWallMemory = ToState.bHasWallMemory;
	LastWallNormal = ToState.LastWallNormal;
	LastWallHeight = ToState.LastWallHeight;
	LastWallTimeMs = ToState.LastWallTimeMs;
	MemoryResetTimeMs = ToState.MemoryResetTimeMs;
	SurfaceSwitchTicks = ToState.SurfaceSwitchTicks;
}

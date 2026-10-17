// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/GSTN_WallMovementTypes.h"

#include "Engine/NetSerialization.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMovementTypes)

namespace GSTNWallBlackboard
{
	const FName EntryInfo = TEXT("GSTN_WallEntryInfo");
	const FName BaseInfo = TEXT("GSTN_WallBaseInfo");
	const FName MemoryResetTimeMs = TEXT("GSTN_WallMemoryResetTimeMs");
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

	uint8 Flags = (bHasWall ? 1 : 0) | (bIsSkidding ? 2 : 0) | (bHasWallMemory ? 4 : 0);
	Ar.SerializeBits(&Flags, 3);
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
}

bool FGSTN_WallMovementSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FGSTN_WallMovementSyncState& Authority = static_cast<const FGSTN_WallMovementSyncState&>(AuthorityState);
	constexpr float NormalCosTolerance = 0.996f; // ~5 degrees

	// Only compare normals while on a wall: off-wall both normals are zero and their dot product (0) would always mismatch.
	return bHasWall != Authority.bHasWall
		|| (bHasWall && FVector::DotProduct(WallNormal, Authority.WallNormal) < NormalCosTolerance);
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
}

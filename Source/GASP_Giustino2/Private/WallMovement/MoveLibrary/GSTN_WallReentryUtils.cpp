// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallReentryUtils.h"

#include "Components/PrimitiveComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "WallMovement/GSTN_WallMovementTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallReentryUtils)

EGSTN_WallMemoryReset UGSTN_WallReentryUtils::FindMemoryReset(const FGSTN_WallReentryRules& Rules, const FGSTN_WallReentryQuery& Query)
{
	const FGSTN_WallMovementSyncState* Memory = Query.Memory;
	if (!Memory)
	{
		return EGSTN_WallMemoryReset::None;
	}

	if (Query.bHasModeChange && Rules.MemoryResetModes.Contains(Query.ChangePrevMode) && Query.ChangeTimeMs >= Memory->LastWallTimeMs)
	{
		return EGSTN_WallMemoryReset::ModeChange;
	}
	// A record whose mode is not the start state's mode lost the change into the current mode to a rollback (see the class comment): that change left
	// the record's mode, after the record's time. If the record's mode is a reset mode entered after the wall, the lost change came from it after the wall
	if (Rules.bInferModeChangeLostToRollback && Query.bHasModeChange && !Query.CurrentMode.IsNone() && Query.ChangeMode != Query.CurrentMode
		&& Rules.MemoryResetModes.Contains(Query.ChangeMode) && Query.ChangeTimeMs >= Memory->LastWallTimeMs)
	{
		return EGSTN_WallMemoryReset::InferredModeChange;
	}
	// Manual reset: recorded in the sync state by FGSTN_ResetWallMemoryEffect (predicted and rolled back with the state)
	// (half a millisecond of slack: the reset time and LastWallTimeMs are the same instant computed along different paths)
	if (Memory->MemoryResetTimeMs >= 0.0 && Memory->MemoryResetTimeMs + 0.5 >= Memory->LastWallTimeMs)
	{
		return EGSTN_WallMemoryReset::Manual;
	}
	return Query.bResetTagPresent ? EGSTN_WallMemoryReset::Tag : EGSTN_WallMemoryReset::None;
}

FGSTN_WallReentryResult UGSTN_WallReentryUtils::Judge(const FGSTN_WallReentryRules& Rules, const FGSTN_WallReentryQuery& Query)
{
	FGSTN_WallReentryResult Result;
	const FGSTN_WallMovementSyncState* Memory = Query.Memory;
	if (!Memory)
	{
		return Result;
	}
	Result.Reset = FindMemoryReset(Rules, Query);
	if (!Memory->bHasWallMemory || Result.Reset != EGSTN_WallMemoryReset::None)
	{
		return Result;
	}

	const FVector LastFlatNormal = FVector::VectorPlaneProject(Memory->LastWallNormal, Query.GravityUp).GetSafeNormal();
	const FVector FlatWallNormal = FVector::VectorPlaneProject(Query.WallNormal, Query.GravityUp).GetSafeNormal();
	Result.bSameWall = !LastFlatNormal.IsNearlyZero() && !FlatWallNormal.IsNearlyZero()
		&& FVector::DotProduct(LastFlatNormal, FlatWallNormal) >= FMath::Cos(FMath::DegreesToRadians(Rules.SameWallAngle));
	if (!Result.bSameWall)
	{
		return Result;
	}

	if (Rules.ReentryCooldown > 0.f && (Query.NowMs - Memory->LastWallTimeMs) < Rules.ReentryCooldown * 1000.0)
	{
		Result.Verdict = EGSTN_WallReentryVerdict::Cooldown;
		return Result;
	}
	if (Rules.SameWallPolicy == EGSTN_SameWallPolicy::Deny)
	{
		Result.Verdict = EGSTN_WallReentryVerdict::Denied;
		return Result;
	}
	if (Rules.SameWallPolicy != EGSTN_SameWallPolicy::HeightCap)
	{
		return Result;
	}

	if (Rules.bBaseRelativeHeightCap && Query.bWallMoving)
	{
		if (!Memory->bLastWallOnMovingBase)
		{
			// No frame to compare a moving wall's heights in: cooldown only
			Result.bHeightCapSkipped = true;
			return Result;
		}
		Result.bBaseRelative = true;
		Result.ComparedHeight = Query.Height - Query.WallBaseHeight;
		Result.ComparedCap = Memory->LastWallHeight - Memory->LastWallBaseHeight + Rules.HeightCapTolerance;
	}
	else
	{
		Result.ComparedHeight = Query.Height;
		Result.ComparedCap = Memory->LastWallHeight + Rules.HeightCapTolerance;
	}
	if (Result.ComparedHeight > Result.ComparedCap)
	{
		Result.Verdict = EGSTN_WallReentryVerdict::HeightCap;
	}
	return Result;
}

bool UGSTN_WallReentryUtils::GetMovingWallHeight(const UPrimitiveComponent* Wall, const FVector& GravityUp, float& OutHeight)
{
	OutHeight = 0.f;
	if (!Wall || !UBasedMovementUtils::IsADynamicBase(Wall))
	{
		return false;
	}
	OutHeight = FVector::DotProduct(Wall->GetComponentLocation(), GravityUp);
	return true;
}

const TCHAR* UGSTN_WallReentryUtils::LexReset(EGSTN_WallMemoryReset Reset)
{
	switch (Reset)
	{
	case EGSTN_WallMemoryReset::ModeChange: return TEXT("mode");
	case EGSTN_WallMemoryReset::InferredModeChange: return TEXT("mode (record lost to a rollback)");
	case EGSTN_WallMemoryReset::Manual: return TEXT("manual");
	case EGSTN_WallMemoryReset::Tag: return TEXT("tag");
	default: return TEXT("none");
	}
}

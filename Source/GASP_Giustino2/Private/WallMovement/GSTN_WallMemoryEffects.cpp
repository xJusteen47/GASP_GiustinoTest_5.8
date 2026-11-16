// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/GSTN_WallMemoryEffects.h"

#include "MoverSimulationTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/GSTN_WallMovementTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMemoryEffects)

bool FGSTN_ResetWallMemoryEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!ApplyEffectParams.TimeStep)
	{
		return false;
	}

	// Start of the step being simulated = end of the previous one, which is where the last wall tick stamped LastWallTimeMs:
	// a wall left before this point is forgotten, the wall the character is on (stamped later by its own ticks) is not
	const double ResetTimeMs = ApplyEffectParams.TimeStep->BaseSimTimeMs;

	FGSTN_WallMovementSyncState& WallState = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FGSTN_WallMovementSyncState>();
	WallState.MemoryResetTimeMs = ResetTimeMs;

	GSTN_WALL_LOG(1, TEXT("WALL MEMORY RESET at %.0fms"), ResetTimeMs);

	// True = the sync state changed: Mover copies it into the state the movement mode starts from, so the mode's tick keeps it
	return true;
}

FInstantMovementEffect* FGSTN_ResetWallMemoryEffect::Clone() const
{
	return new FGSTN_ResetWallMemoryEffect(*this);
}

void FGSTN_ResetWallMemoryEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FGSTN_ResetWallMemoryEffect::GetScriptStruct() const
{
	return FGSTN_ResetWallMemoryEffect::StaticStruct();
}

FString FGSTN_ResetWallMemoryEffect::ToSimpleString() const
{
	return TEXT("ResetWallMemory");
}

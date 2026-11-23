// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InstantMovementEffect.h"
#include "GSTN_WallMemoryEffects.generated.h"

/**
 * Forgets the walls touched so far: the next wall grab ignores the same-wall rules (height cap, cooldown, deny).
 *
 * An instant movement effect, so the reset happens inside the movement simulation: it is applied at a definite simulation time,
 * recorded in FGSTN_WallMovementSyncState::MemoryResetTimeMs (predicted, rolled back and reconciled with the rest of the wall
 * state) and re-applied if the frame is re-simulated. Setting a Mover blackboard value from game code instead would not be
 * rewound by a rollback.
 *
 * It only covers walls left before it is applied: a wall the character is on right now keeps refreshing the memory.
 * To make every end agree without a correction, queue it from the same simulated event on server and clients
 * (ability, layered move), like any other Mover instant effect. UGSTN_WallMovementLibrary::ResetWallMemory queues it.
 */
USTRUCT(BlueprintType, Category = "Mover|Instant Movement Effect", DisplayName = "Reset Wall Memory Instant Movement Effect")
struct GASP_GIUSTINO2_API FGSTN_ResetWallMemoryEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

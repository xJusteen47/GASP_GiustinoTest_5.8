// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallMovementLibrary.h"

#include "MoverComponent.h"
#include "WallMovement/GSTN_WallMemoryEffects.h"
#include "WallMovement/GSTN_WallMovementDebug.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMovementLibrary)

void UGSTN_WallMovementLibrary::ResetWallMemory(UMoverComponent* MoverComponent)
{
	if (!MoverComponent)
	{
		return;
	}

	// Applied by the simulation at the start of its next step (see FGSTN_ResetWallMemoryEffect), so the reset is part of the
	// predicted state instead of a side write that a rollback would not undo
	MoverComponent->QueueInstantMovementEffect(MakeShared<FGSTN_ResetWallMemoryEffect>());

	GSTN_WALL_LOG(1, TEXT("WALL MEMORY RESET queued"));
}

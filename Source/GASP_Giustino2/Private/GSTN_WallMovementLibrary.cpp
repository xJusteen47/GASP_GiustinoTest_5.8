// Fill out your copyright notice in the Description page of Project Settings.

#include "GSTN_WallMovementLibrary.h"

#include "GSTN_SimpleWallMovementMode.h"
#include "GSTN_WallMovementDebug.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMovementLibrary)

void UGSTN_WallMovementLibrary::ResetWallMemory(UMoverComponent* MoverComponent)
{
	UMoverBlackboard* SimBlackboard = MoverComponent ? MoverComponent->GetSimBlackboard_Mutable() : nullptr;
	if (!SimBlackboard)
	{
		return;
	}

	// The memory stays in the sync state; entry transitions ignore walls left before this time.
	// End of the last simulated step, so it also covers the wall the character is on (or just left) right now
	const FMoverTimeStep& LastStep = MoverComponent->GetLastTimeStep();
	const double ResetTimeMs = LastStep.BaseSimTimeMs + LastStep.StepMs;
	SimBlackboard->Set(GSTNWallBlackboard::MemoryResetTimeMs, ResetTimeMs);

	GSTN_WALL_LOG(1, TEXT("WALL MEMORY RESET (manual) at %.0fms"), ResetTimeMs);
}

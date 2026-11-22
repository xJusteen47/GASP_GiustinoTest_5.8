// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_WallMovementLibrary.generated.h"

class UMoverComponent;

UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallMovementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Forgets the walls touched so far: the next wall grab ignores the same-wall rules (height cap, cooldown, deny).
	 * Call it when an ability or event should allow climbing the same wall again. For a data-driven alternative,
	 * see UGSTN_WallEntryTransition::MemoryResetTags.
	 *
	 * Queues FGSTN_ResetWallMemoryEffect: the reset is applied inside the movement simulation (next step) and stored in the
	 * predicted wall sync state. Call it from the same simulated event on every end (ability, server and clients alike): called
	 * from the client only, the server state has no reset and the client is reconciled back to it (the reset is lost).
	 */
	UFUNCTION(BlueprintCallable, Category = "Mover|Wall")
	static void ResetWallMemory(UMoverComponent* MoverComponent);
};

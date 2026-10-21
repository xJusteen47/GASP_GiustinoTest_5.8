// Mover state tracking for simulation code (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MoverTypes.h"
#include "MoverSimulationTypes.h"
#include "GSTN_MoverSimStateTracker.generated.h"

class UMoverComponent;

/**
 * Mover's simulation state as of the last simulated (or rolled back to) frame, for code that runs inside the fixed tick
 * before Mover, such as the gameplay abilities.
 *
 * UMoverComponent's own queries (GetMovementModeName, HasGameplayTag, IsOnGround...) read the state cached for the game
 * frame. During a re-simulation many simulation frames run in one game frame, so those queries return the newest state
 * instead of the state of the frame being re-simulated, and the abilities take different decisions than they did when
 * predicting (or than the server did): reconciles. This component follows each simulation step and each rollback instead.
 *
 * Created on demand (FindOrAdd) on the Mover component's owner. Not replicated.
 */
UCLASS(ClassGroup = (AbilitySystem))
class GSTNINPUTNP_API UGSTN_MoverSimStateTracker : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_MoverSimStateTracker();

	/** The tracker of this Mover component, created and bound the first time */
	static UGSTN_MoverSimStateTracker* FindOrAdd(UMoverComponent* MoverComponent);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Sync state at the end of the last simulated frame (the input state of the next one) */
	const FMoverSyncState& GetSyncState() const { return SyncState; }

	FName GetMovementModeName() const { return SyncState.MovementMode; }

	/** Same as UMoverComponent::HasGameplayTag, on the tracked simulation state (e.g. Mover_IsOnGround) */
	bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const;

private:
	void Bind(UMoverComponent* MoverComponent);

	UFUNCTION()
	void HandlePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& InSyncState, FMoverAuxStateContext& InAuxState);

	UFUNCTION()
	void HandlePostSimulationRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep);

	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> Mover;

	FMoverSyncState SyncState;
};

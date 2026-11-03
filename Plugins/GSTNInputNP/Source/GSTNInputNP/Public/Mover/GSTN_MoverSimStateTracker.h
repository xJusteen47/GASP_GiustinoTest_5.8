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
UCLASS(ClassGroup = (AbilitySystem), BlueprintType)
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

	/** The Mover movement mode at the end of the last simulated frame, for Blueprint abilities and context providers */
	UFUNCTION(BlueprintPure, Category = "Mover|Sim State")
	FName GetMovementModeName() const { return SyncState.MovementMode; }

	/** Same as UMoverComponent::HasGameplayTag, on the tracked simulation state (e.g. Mover_IsOnGround) */
	UFUNCTION(BlueprintPure, Category = "Mover|Sim State")
	bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const;

	/**
	 * The direction Mover considers up (UMoverComponent::GetUpDirection: gravity derived unless overridden) as of the last
	 * simulated frame, rounded to 1/4096 per component so that machines that compute it from the same gravity hold the same
	 * bits. Gravity points the other way: every "up", "down" and "horizontal" of simulation code is relative to this, never to world Z.
	 * Mover's gravity is a component setting that no rollback restores, so this is the value the last step ran with.
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Sim State")
	FVector GetUpDirection() const { return UpDirection; }

	/** The unit direction gravity pulls in, -GetUpDirection() */
	UFUNCTION(BlueprintPure, Category = "Mover|Sim State")
	FVector GetGravityDirection() const { return -UpDirection; }

	/** The world space velocity at the end of the last simulated frame (zero without a default sync state) */
	UFUNCTION(BlueprintPure, Category = "Mover|Sim State")
	FVector GetVelocity() const;

	/** The velocity component along GetUpDirection(): positive while moving away from the ground, whatever the gravity direction */
	UFUNCTION(BlueprintPure, Category = "Mover|Sim State")
	double GetUpSpeed() const { return GetVelocity().Dot(UpDirection); }

	/**
	 * True when the last simulated frame started a launch layered move (FLayeredMove_Launch or a child: a trampoline, a
	 * knockback launcher, a test launch) that is in the sync state, i.e. the character was thrown by something other than its
	 * own jump. Derived from the restored sync state only, so a rollback and a re-simulation give the same answer. The jump impulse
	 * is an instant effect, not a layered move, and never sets it. Abilities that must not assume they are the only upward launch source
	 * (air jump count, coyote time) react to it.
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Sim State")
	bool WasExternallyLaunchedThisFrame() const { return bExternallyLaunched; }

private:
	void Bind(UMoverComponent* MoverComponent);

	UFUNCTION()
	void HandlePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& InSyncState, FMoverAuxStateContext& InAuxState);

	UFUNCTION()
	void HandlePostSimulationRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep);

	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> Mover;

	/** Reads the quantised up direction and the launch flag of SyncState, produced by the step that began at StepBaseTimeMs */
	void CaptureFrameFacts(const FMoverSyncState& InSyncState, double StepBaseTimeMs);

	FMoverSyncState SyncState;
	FVector UpDirection = FVector::UpVector;
	bool bExternallyLaunched = false;
};

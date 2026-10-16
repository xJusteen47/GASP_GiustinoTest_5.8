// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverTypes.h"
#include "GSTN_GASNPPTestComponent.generated.h"

class UMoverComponent;
class UNpAbilitySystemComponent;
class UInputMappingContext;
class UInputAction;

/**
 * Test helper for the GAS + Network Prediction + Mover integration.
 * - Registers the test input mapping context on the locally controlled pawn (Enhanced Input + NpAbilitySystemComponent).
 * - Counts Mover rollbacks and simulation ticks (and how many of them were re-simulations).
 * - Exposes a JSON snapshot of the simulated state so server and client copies of the same pawn can be compared.
 */
UCLASS(ClassGroup = (Test), meta = (BlueprintSpawnableComponent))
class GASP_GIUSTINO2_API UGSTN_GASNPPTestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_GASNPPTestComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** JSON snapshot: role, counters, transform/velocity/mode, attributes, tags, active effects, cooldowns. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	FString GetStateSnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	void ResetCounters();

	UFUNCTION(BlueprintPure, Category = "GASNPP Test")
	bool IsTestInputReady() const { return bInputReady; }

	/** Holds an action's value through Enhanced Input injection (e.g. IA_Move with (0,1,0) to walk forward). Local player only. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	bool StartHoldingInput(UInputAction* Action, FVector Value);

	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	bool StopHoldingInput(UInputAction* Action);

	/** Injects a single-frame press of an action (e.g. IA_GASNPP_Dash). Local player only. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	bool PressInput(UInputAction* Action);

	UPROPERTY(EditDefaultsOnly, Category = "GASNPP Test")
	TObjectPtr<UInputMappingContext> TestMappingContext;

	UPROPERTY(VisibleInstanceOnly, Category = "GASNPP Test")
	int32 RollbackCount = 0;

	/** Sum of rolled back (expunged) simulation time in ms. */
	UPROPERTY(VisibleInstanceOnly, Category = "GASNPP Test")
	double RolledBackTimeMs = 0.0;

	UPROPERTY(VisibleInstanceOnly, Category = "GASNPP Test")
	int32 SimTickCount = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "GASNPP Test")
	int32 ResimTickCount = 0;

	/** Re-simulated Mover ticks during which the ability system was also re-simulating. */
	UPROPERTY(VisibleInstanceOnly, Category = "GASNPP Test")
	int32 AbilityResimTickCount = 0;

private:
	UFUNCTION()
	void HandlePreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd);

	UFUNCTION()
	void HandlePostSimulationRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep);

	void TrySetupInput();

	class UEnhancedInputLocalPlayerSubsystem* GetLocalInputSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> Mover;

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	bool bInputReady = false;
};

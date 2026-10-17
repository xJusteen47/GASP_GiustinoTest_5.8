// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "GSTN_CombatSimComponent.generated.h"

class UGameplayAbility;
class UInputAction;
class UNpAbilitySystemComponent;
struct FAbilitySystemTimeStep;

/**
 * Runs the combat Network Prediction model of its actor and drives the actor's ability system from it.
 *
 * The fork's ability system sync state cannot be extended, so combat state lives in its own model, sorted just before the
 * ability system (which is before Mover) in the fixed tick. Rollback and re-simulation restore and re-tick every fixed
 * tick model together, so both stay consistent.
 *
 * The combat tick only writes its own sync state. Everything it wants from the ability system (activate an ability,
 * freeze the montage during hitstop) is a frame-stamped request in that state, executed by this component from the
 * ability system's OnSimulationPreTick, i.e. inside the ability system's own tick, where the fork has the right frame,
 * time and rollback bookkeeping. Nothing else writes into the combat state during a frame.
 *
 * Current scope (risk spike): buttons sampled from Enhanced Input or scripted by tests, a press of button i activates
 * ButtonAbilities[i], a press of HitstopTestButton opens a hitstop window that pauses the synced montage.
 *
 * Add it to a pawn that also has a UNpAbilitySystemComponent. Replicated; interpolated simulated proxies do not tick it.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_CombatSimComponent : public UNetworkPredictionComponent
{
	GENERATED_BODY()

public:
	UGSTN_CombatSimComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Network Prediction driver and simulation
	void InitializeSimulationState(FGSTN_CombatSyncState* Sync, FGSTN_CombatAuxState* Aux);
	void ProduceInput(const int32 DeltaTimeMS, FGSTN_CombatInputCmd* Cmd);
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_CombatStateTypes>& Input, const TNetSimOutput<FGSTN_CombatStateTypes>& Output);
	void FinalizeFrame(const FGSTN_CombatSyncState* Sync, const FGSTN_CombatAuxState* Aux);

	/**
	 * Replaces the sampled buttons with Mask (bit i = button i held) until ClearScriptedButtons. For tests and AI; only
	 * has an effect on the machine that produces the input (owning client, or server for AI)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Input")
	void SetScriptedButtons(int32 Mask);

	UFUNCTION(BlueprintCallable, Category = "Combat|Input")
	void ClearScriptedButtons();

	/** Sends a press of Mask in the next input only, even if the buttons are never seen held (a tap shorter than a frame) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Input")
	void TapScriptedButtons(int32 Mask);

	/** Latest simulated (or interpolated) combat state */
	const FGSTN_CombatSyncState* GetSyncState() const;

	/** Logical buttons, bit i of the input = ButtonActions[i]. At most 32 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TArray<TObjectPtr<UInputAction>> ButtonActions;

	/** Ability activated by a press of button i (spike: one ability per button, no slots or buffering yet) */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike")
	TArray<TSubclassOf<UGameplayAbility>> ButtonAbilities;

	/** Button whose press opens a hitstop window. -1 = none */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "-1", ClampMax = "31"))
	int32 HitstopTestButton = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "1"))
	int32 HitstopTestFrames = 8;

protected:
	virtual void InitializeNetworkPredictionProxy() override;

private:
	uint32 SampleHeldButtons() const;

	void BindAbilitySystem();
	void HandleAbilitySimulationPreTick(const FAbilitySystemTimeStep& TimeStep);

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	FDelegateHandle PreTickHandle;

	// Input production only (owning machine), not simulation state
	uint32 ScriptedButtons = 0;
	uint32 PendingTaps = 0;
	bool bUseScriptedButtons = false;
};

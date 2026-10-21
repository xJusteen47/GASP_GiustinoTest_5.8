// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "GSTN_CombatSimComponent.generated.h"

class UGameplayAbility;
class UGSTN_MoverSimStateTracker;
class UInputAction;
class UEnhancedPlayerInput;
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
 * Every tick the command (buttons, raw stick, camera yaw) is classified against the facing the Mover tracker knows for the
 * end of the previous frame and appended to the input history in the sync state. Still the risk spike's behaviour on top:
 * a press of button i activates ButtonAbilities[i], a press of HitstopTestButton opens a hitstop window that pauses the
 * synced montage; slots and the buffer replace the button mapping next.
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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

	/**
	 * The input history in the notation of FGSTN_InputScript (oldest first, newest at the end), the held buttons and the
	 * last ability request. For the debug display, logs and the network test harness
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	FString GetDebugString(int32 MaxEvents = 8) const;

	/** Logical buttons, bit i of the input = ButtonActions[i]. At most 32 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TArray<TObjectPtr<UInputAction>> ButtonActions;

	/** The 2D move action, read as the stick on the controller (+Y up). Unset = the stick is always neutral */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> MoveAction;

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
	/** Enhanced Input state of the local player, null when this machine does not control the pawn */
	const UEnhancedPlayerInput* GetPlayerInput() const;
	uint32 SampleHeldButtons() const;
	void SampleStick(FGSTN_CombatInputCmd& Cmd) const;

	/** Yaw the character faces at the end of the previous frame, from the Mover tracker */
	float GetSimulatedFacingYaw() const;

	void BindAbilitySystem();
	void HandleAbilitySimulationPreTick(const FAbilitySystemTimeStep& TimeStep);

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_MoverSimStateTracker> MoverTracker;

	FDelegateHandle PreTickHandle;

	// Input production only (owning machine), not simulation state
	uint32 ScriptedButtons = 0;
	uint32 PendingTaps = 0;
	bool bUseScriptedButtons = false;
};

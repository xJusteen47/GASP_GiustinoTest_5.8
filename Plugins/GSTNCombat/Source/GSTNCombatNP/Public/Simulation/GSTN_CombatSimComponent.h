// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Events/GSTN_SimEvent.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_ButtonMap.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CommandSourceInterface.h"
#include "Simulation/GSTN_InputSimulator.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_CombatSimComponent.generated.h"

class UGSTN_CommandBindingSet;
class UGSTN_InputConfig;
class UGSTN_InputContextProvider;
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
 * Every tick the input simulation of GSTNInputNP (FGSTN_InputSimulator) runs on the embedded input state: the command is
 * sampled, classified against the facing the context provider knows for the end of the previous frame, appended to the
 * input history, and the command binding set decides which ability the buffered input starts. This component is the
 * simulator's host (IGSTN_InputSimHost: owner tags and ability checks from the ability system, movement context from the
 * provider) and adds what is combat's: the debug hitstop window that pauses the synced montage, until hits replace it.
 *
 * Abilities ask this component, through IGSTN_CommandSourceInterface, why they started (GetActivationContext) and whether their
 * trigger button is still held (IsCommandButtonHeld). Every command the simulation requests also leaves an
 * Input.Event.CommandExecuted entry in an outbox that FinalizeFrame publishes on the authority only (the listen-server host's
 * own pawn included), so the event stream counts each activation once, however often a client re-simulates it.
 *
 * The input command is made by an input source (the local player by default; an AI source for server-driven pawns).
 * SetScriptedButtons / TapScriptedButtons override the buttons of that command for tests.
 *
 * Add it to a pawn that also has a UNpAbilitySystemComponent. Replicated; interpolated simulated proxies do not tick it.
 */
UCLASS(ClassGroup = (Combat), Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_CombatSimComponent : public UNetworkPredictionComponent, public IGSTN_InputSimHost, public IGSTN_CommandSourceInterface
{
	GENERATED_BODY()

public:
	UGSTN_CombatSimComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Network Prediction driver and simulation
	void InitializeSimulationState(FGSTN_CombatSyncState* Sync, FGSTN_CombatAuxState* Aux);
	void ProduceInput(const int32 DeltaTimeMS, FGSTN_InputCmd* Cmd);
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_CombatStateTypes>& Input, const TNetSimOutput<FGSTN_CombatStateTypes>& Output);
	void FinalizeFrame(const FGSTN_CombatSyncState* Sync, const FGSTN_CombatAuxState* Aux);

	// IGSTN_InputSimHost
	virtual float GetFacingYaw() const override;
	virtual void GetOwnerTags(FGameplayTagContainer& OutTags) const override;
	virtual void AppendContextTags(FGameplayTagContainer& OutTags) const override;
	virtual uint8 GetContextFingerprint() const override;
	virtual void AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const override;
	virtual bool CanExecuteBinding(int32 BindingIndex) const override;

	// IGSTN_CommandSourceInterface. Read the sync state the simulation is on, so call them from ActivateAbility or an ability's tick
	UFUNCTION(BlueprintPure, Category = "Combat|Input")
	virtual FGSTN_ActivationContext GetActivationContext() const override;

	UFUNCTION(BlueprintPure, Category = "Combat|Input")
	virtual bool IsCommandButtonHeld(int32 Button) const override;

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

	/** The compiled commands and bindings: the meaning of the indices in the sync state and in the commands */
	const FGSTN_CommandTable& GetCommandTable() const { return CommandTable; }

	/**
	 * The input history in the notation of FGSTN_InputScript (oldest first, newest at the end), the held buttons and the
	 * last ability request. For the debug display, logs and the network test harness
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	FString GetDebugString(int32 MaxEvents = 8) const;

	/**
	 * Buttons (bit i = Buttons[i]) and the move action. Every machine of a match must use the same asset. Soft: loaded once at
	 * BeginPlay (CompileInput), together with its input actions, never while simulating
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TSoftObjectPtr<UGSTN_InputConfig> InputConfig;

	/**
	 * Commands, their bindings to abilities and reset rules. Fixed for the lifetime of the component (the indices go into the
	 * sync state). Soft: loaded once at BeginPlay, with the slot sets and abilities it names
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TSoftObjectPtr<UGSTN_CommandBindingSet> SlotBindings;

	/** Reset rules of the character, applied before the binding set's own */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input")
	TArray<TInstancedStruct<FGSTN_BufferResetRule>> ResetRules;

	/** Makes the input command; must implement IGSTN_InputSource. Default: the local player (UGSTN_PlayerInputSource), use UGSTN_AIInputSource for pawns the server drives */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Combat|Input", meta = (MustImplement = "/Script/GSTNInputNP.GSTN_InputSource"))
	TObjectPtr<UObject> InputSource;

	/** Facing, context tags and landing events of the owner's movement. Default: Mover */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Combat|Input")
	TObjectPtr<UGSTN_InputContextProvider> ContextProvider;

	/** Button whose press opens a hitstop window. -1 = none */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "-1", ClampMax = "31"))
	int32 HitstopTestButton = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "1"))
	int32 HitstopTestFrames = 8;

protected:
	virtual void InitializeNetworkPredictionProxy() override;

private:
	/** Builds the command table and the button map from the assets (BeginPlay, before any simulation tick) */
	void CompileInput();

	void BindAbilitySystem();
	void HandleAbilitySimulationPreTick(const FAbilitySystemTimeStep& TimeStep);

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	// Compiled from the assets once: read-only while simulating, so neither is rolled back
	FGSTN_CommandTable CommandTable;
	FGSTN_ButtonMap ButtonMap;

	// The assets InputConfig / SlotBindings resolved to at setup, strongly held because the table points into them and the
	// input source reads the config every frame. Never resolved in a tick
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_InputConfig> ActiveInputConfig;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CommandBindingSet> ActiveSlotBindings;

	FDelegateHandle PreTickHandle;

	// Written by the simulation tick on every machine, published and emptied in FinalizeFrame (authority only); not sync state
	FGSTN_SimEventOutbox EventOutbox;

	// Input production only (owning machine), not simulation state
	uint32 ScriptedButtons = 0;
	uint32 PendingTaps = 0;
	bool bUseScriptedButtons = false;
};

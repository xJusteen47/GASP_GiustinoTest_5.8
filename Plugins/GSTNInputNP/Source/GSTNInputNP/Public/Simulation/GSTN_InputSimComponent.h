// Standalone input simulation component (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Events/GSTN_SimEvent.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_ButtonMap.h"
#include "Simulation/GSTN_CommandSourceInterface.h"
#include "Simulation/GSTN_InputSimTypes.h"
#include "Simulation/GSTN_InputSimulator.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_InputSimComponent.generated.h"

class UGSTN_InputConfig;
class UGSTN_InputContextProvider;
class UNpAbilitySystemComponent;
struct FAbilitySystemTimeStep;

/** Sync state of the standalone input model: the embedded input simulation state and nothing else. */
struct FGSTN_InputModelSyncState
{
	FGSTN_InputSimState Input;

	void NetSerialize(const FNetSerializeParams& P)
	{
		Input.NetSerialize(P.Ar);
	}

	bool ShouldReconcile(const FGSTN_InputModelSyncState& AuthorityState) const
	{
		// The input state is authoritative like the rest of the sync state: the field that differs is traced by the call
		return Input.ShouldReconcile(AuthorityState.Input);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Input.ToString(Out);
		Out.Append("\n");
	}

	/** Discrete state: interpolated proxies just take the newer frame */
	void Interpolate(const FGSTN_InputModelSyncState* From, const FGSTN_InputModelSyncState* To, float PCT)
	{
		*this = *To;
	}
};

/** No auxiliary state */
struct FGSTN_InputModelAuxState
{
	void NetSerialize(const FNetSerializeParams& P) {}
	bool ShouldReconcile(const FGSTN_InputModelAuxState& AuthorityState) const { return false; }
	void ToString(FAnsiStringBuilderBase& Out) const {}
	void Interpolate(const FGSTN_InputModelAuxState* From, const FGSTN_InputModelAuxState* To, float PCT) {}
};

using FGSTN_InputModelStateTypes = TNetworkPredictionStateTypes<FGSTN_InputCmd, FGSTN_InputModelSyncState, FGSTN_InputModelAuxState>;

/**
 * Runs the input simulation as a Network Prediction model of its own, for a project that wants buffered, prioritised
 * commands bound to gameplay abilities without the combat plugin (input.md section 5.4). It is the same simulation the combat
 * component embeds (FGSTN_InputSimulator), hosted with nothing added: no hitstop, no timelines.
 *
 * Each fixed tick the model samples the input source's command, classifies the stick against the facing the context provider
 * knows, updates the input history in its sync state, and requests the binding the buffered input selects. The request is a
 * frame-stamped field of the sync state, executed from the ability system's OnSimulationPreTick, so activation runs with the
 * right frame and rolls back with everything else. The model is sorted right before the ability system, which is before
 * Mover, like the combat model.
 *
 * Every command it requests also appends an Input.Event.CommandExecuted entry to an outbox that FinalizeFrame publishes to
 * UGSTN_SimEventSubsystem on the authority only, the listen-server host's own pawn included.
 *
 * A pawn uses either this component or the combat component (which embeds the same simulation). With both, two models would
 * read the same buttons: this one logs an error at BeginPlay and stays inert.
 *
 * Add it to a pawn that also has a UNpAbilitySystemComponent. Replicated; interpolated simulated proxies do not tick it.
 */
UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class GSTNINPUTNP_API UGSTN_InputSimComponent : public UNetworkPredictionComponent, public IGSTN_InputSimHost, public IGSTN_CommandSourceInterface
{
	GENERATED_BODY()

public:
	UGSTN_InputSimComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Network Prediction driver and simulation
	void InitializeSimulationState(FGSTN_InputModelSyncState* Sync, FGSTN_InputModelAuxState* Aux);
	void ProduceInput(const int32 DeltaTimeMS, FGSTN_InputCmd* Cmd);
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_InputModelStateTypes>& Input, const TNetSimOutput<FGSTN_InputModelStateTypes>& Output);
	void FinalizeFrame(const FGSTN_InputModelSyncState* Sync, const FGSTN_InputModelAuxState* Aux);

	// IGSTN_InputSimHost
	virtual float GetFacingYaw() const override;
	virtual void GetOwnerTags(FGameplayTagContainer& OutTags) const override;
	virtual void AppendContextTags(FGameplayTagContainer& OutTags) const override;
	virtual uint8 GetContextFingerprint() const override;
	virtual void AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const override;
	virtual bool CanExecuteBinding(int32 BindingIndex) const override;

	// IGSTN_CommandSourceInterface. Read the sync state the simulation is on, so call them from ActivateAbility or an ability's tick
	UFUNCTION(BlueprintPure, Category = "Input")
	virtual FGSTN_ActivationContext GetActivationContext() const override;

	UFUNCTION(BlueprintPure, Category = "Input")
	virtual bool IsCommandButtonHeld(int32 Button) const override;

	/** Latest simulated (or interpolated) state */
	const FGSTN_InputModelSyncState* GetSyncState() const;

	/** The compiled commands and bindings: the meaning of the indices in the sync state and in the commands */
	const FGSTN_CommandTable& GetCommandTable() const { return CommandTable; }

	/** The input history in the notation of FGSTN_InputScript, the held buttons and the last ability request. For logs and tests */
	UFUNCTION(BlueprintCallable, Category = "Input|Debug")
	FString GetDebugString(int32 MaxEvents = 8) const;

	/** Whether the component found a conflicting input component at BeginPlay and does nothing */
	bool IsInert() const { return bInert; }

	/**
	 * Buttons (bit i = Buttons[i]) and the move action. Every machine of a match must use the same asset. Soft: loaded once at
	 * BeginPlay (CompileInput), together with its input actions, never while simulating
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UGSTN_InputConfig> InputConfig;

	/**
	 * Commands, their bindings to abilities and reset rules. Fixed for the lifetime of the component (the indices go into the
	 * sync state). Soft: loaded once at BeginPlay, with the slot sets and abilities it names
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UGSTN_CommandBindingSet> SlotBindings;

	/** Reset rules of the character, applied before the binding set's own */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TInstancedStruct<FGSTN_BufferResetRule>> ResetRules;

	/** Makes the input command; must implement IGSTN_InputSource. Default: the local player (UGSTN_PlayerInputSource), use UGSTN_AIInputSource for pawns the server drives */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Input", meta = (MustImplement = "/Script/GSTNInputNP.GSTN_InputSource"))
	TObjectPtr<UObject> InputSource;

	/** Facing, context tags and landing events of the owner's movement. Default: Mover */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Input")
	TObjectPtr<UGSTN_InputContextProvider> ContextProvider;

protected:
	virtual void InitializeNetworkPredictionProxy() override;

private:
	/** Builds the command table and the button map from the assets (BeginPlay, before any simulation tick) */
	void CompileInput();

	void BindAbilitySystem();
	void HandleAbilitySimulationPreTick(const FAbilitySystemTimeStep& TimeStep);

	/** Another component of the owner runs an input simulation too */
	bool HasConflictingInputComponent() const;

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

	// The same on every machine (it depends on the owner's components), so an inert component stays deterministic
	bool bInert = false;
};

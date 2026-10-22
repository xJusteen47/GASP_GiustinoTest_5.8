// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverTypes.h"
#include "GameplayEffectTypes.h"
#include "GSTN_GASNPPTestComponent.generated.h"

class UMoverComponent;
class UNpAbilitySystemComponent;
class UInputMappingContext;
class UInputAction;
struct FAbilitySystemTimeStep;

/** Activation counters of one probe ability on this machine. Observation only: not rolled back, not simulation state. */
struct FGSTN_ProbeCounters
{
	/** CanActivateAbility calls (before any refusal), and how many were made while the ability system restored a frame */
	int32 Attempts = 0;
	int32 AttemptsRestoring = 0;
	/** ActivateAbility calls in the forward simulation, in a re-simulation, and while a frame was being restored */
	int32 Activations = 0;
	int32 ActivationsResim = 0;
	int32 ActivationsRestoring = 0;
	/** Simulation frame of the latest activation outside a restore (a re-simulation overwrites it with the same frame) */
	int32 LastFrame = -1;
	/** Simulation frame at which that activation ended (the frames it lived show whether it ticked in its activation frame) */
	int32 EndFrame = -1;
};

/**
 * Test helper for the GAS + Network Prediction + Mover integration.
 * - Registers the test input mapping context on the locally controlled pawn (Enhanced Input + NpAbilitySystemComponent).
 * - Counts Mover rollbacks and simulation ticks (and how many of them were re-simulations).
 * - Exposes a JSON snapshot of the simulated state so server and client copies of the same pawn can be compared.
 * - Hosts the M3 spike probes: server-side grants of ability batches and infinite effects from outside the simulation, the
 *   activation counters of the trigger probes (GSTN_TestProbeAbilities.h) and a stats probe that rewrites Mover's MaxSpeed
 *   and gravity every tick from attributes.
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

	// ---- M3 spike probes. The Server* functions run on the authority only, from outside the simulation (a plain game thread
	// call, like a loadout component would), and return false / 0 elsewhere.

	/**
	 * Grants one trigger probe ability to this pawn: "EventRaiser", "EventTarget", "TagRaiser" or "TagTarget" (see
	 * GSTN_TestProbeAbilities.h). Grant at most one raiser at a time. Returns false if it is already owned or unknown.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerGrantProbe(const FString& Name);

	/** Removes every trigger probe ability (raisers and targets) from this pawn. Returns how many specs were cleared. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	int32 ServerRevokeProbes();

	/**
	 * Grants Count of the 40 distinct bulk probe ability classes, starting at FirstIndex and wrapping around, in one game thread
	 * call. Classes already owned are skipped. Returns how many specs were granted.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	int32 ServerGrantBulk(int32 FirstIndex = 0, int32 Count = 40);

	/** Clears every bulk probe spec of this pawn. Returns how many were cleared. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	int32 ServerRevokeBulk();

	/** Applies (true) or removes (false) the infinite bulk effect from outside the simulation. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerSetBulkEffect(bool bApply);

	/** Stats probe preset driven by an infinite effect: 0 = none, 1 = speed x2 and gravity x0.5, 2 = speed x0.5 and gravity x2. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerSetStatsPreset(int32 Preset);

	/** How many of the 40 bulk probe classes this machine finds with FindAbilitySpecFromClass (valid on the authority and on the owning client) */
	UFUNCTION(BlueprintPure, Category = "GASNPP Test|Probe")
	int32 CountOwnedBulkSpecs() const;

	/**
	 * Stats probe: every simulated tick Mover's MaxSpeed and gravity are rewritten as the values captured when the probe was
	 * enabled times the ProbeSpeedScale / ProbeGravityScale attributes. Set it on every copy of the pawn (server and clients) before
	 * changing the attributes, not mid-flight. Writing it every tick from simulated data is what makes it rollback-safe.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASNPP Test|Probe")
	bool bStatsProbeEnabled = false;

	/** Same as setting bStatsProbeEnabled, for Python (the harness calls it on every copy of the pawn, in every world) */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	void SetStatsProbeEnabled(bool bEnabled) { bStatsProbeEnabled = bEnabled; }

	/** Clears the activation counters of the trigger probes only (ResetCounters clears them too, with the rollback counters) */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	void ResetProbeCounters() { ProbeCounters.Reset(); }

	// Probe bookkeeping, called by the probe abilities
	void RecordProbeAttempt(FName Probe, bool bRestoring);
	void RecordProbeActivation(FName Probe, bool bResimulating, bool bRestoring);
	void RecordProbeEnd(FName Probe);

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

	void HandleAbilityPreTick(const FAbilitySystemTimeStep& TimeStep);

	void ApplyStatsProbe();

	void TrySetupInput();

	class UEnhancedInputLocalPlayerSubsystem* GetLocalInputSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> Mover;

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	bool bInputReady = false;

	TMap<FName, FGSTN_ProbeCounters> ProbeCounters;

	/** Simulation frame of the latest tick of the ability system, to date the probe activations */
	int32 AbilityFrame = -1;

	/** Last Mover mode and floor-hit state logged by gstn.test.LogMoverModeChanges */
	FName LoggedMode;
	bool bLoggedFloor = false;

	// Server-side handles of the infinite effects applied by the probes
	FActiveGameplayEffectHandle BulkEffectHandle;
	FActiveGameplayEffectHandle StatsEffectHandle;

	// Stats probe: the values captured before the first rewrite, and what was written last
	bool bStatsBaseCaptured = false;
	float BaseMaxSpeed = 0.f;
	FVector BaseGravity = FVector::ZeroVector;
	float AppliedSpeedScale = 1.f;
	float AppliedGravityScale = 1.f;
};

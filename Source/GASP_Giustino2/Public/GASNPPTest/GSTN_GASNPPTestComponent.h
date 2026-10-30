// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverTypes.h"
#include "GameplayEffectTypes.h"
#include "GSTN_GASNPPTestComponent.generated.h"

class UGSTN_LoadoutComponent;
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
	/** ActivateAbility calls in the forward simulation, in a re-simulation (flagged as such, or on a frame at or below LastFrame:
	 * a replayed activation), and while a frame was being restored */
	int32 Activations = 0;
	int32 ActivationsResim = 0;
	int32 ActivationsRestoring = 0;
	/** Highest simulation frame with an activation outside a restore (a replay of an earlier frame never lowers it) */
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
UCLASS(ClassGroup = (Test), Config = Game, meta = (BlueprintSpawnableComponent))
class GASP_GIUSTINO2_API UGSTN_GASNPPTestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_GASNPPTestComponent();

	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** JSON snapshot: role, counters, transform/velocity/mode, attributes, tags, active effects, cooldowns. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	FString GetStateSnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	void ResetCounters();

	/** Every simulation tick since the last call (resimulated ticks included), as a JSON array of
	    {t: BaseSimTimeMs, loc, vel, mode, tags: Test.* tags}, then forgets them. The harness judges heights, speeds and tag spans on
	    these, not on the editor tick, so a check does not depend on how many sim steps one editor frame holds. */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	FString DrainSimHistory();

	/**
	 * Authority only: puts the pawn back where it spawned (a teleport through Mover's instant effects, so it is predicted and rolled back like any other move;
	 * the caller waits for the pawn to settle). The scenarios start from here, wherever the previous one ended.
	 * Offset: shifts the spot so that the pawns of one process do not stand in each other's way (they all spawn within a metre).
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	bool ServerResetToSpawn(FVector Offset);

	/**
	 * Authority only: teleports the pawn to exactly this world location (no floor search, orientation kept) and stops it, through Mover instant effects like
	 * ServerResetToSpawn. The trampoline scenario drops the pawn onto a surface from a height with it
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	bool ServerTeleportExact(FVector Location);

	/** Where this pawn spawned (the origin its lane is measured from); zero before BeginPlay */
	UFUNCTION(BlueprintPure, Category = "GASNPP Test")
	FVector GetSpawnLocation() const { return SpawnLocation; }

	/**
	 * Authority only: the harness's per scenario reset. ServerResetToSpawn plus velocity zero (the teleport keeps the velocity of the previous scenario), and the server-side test state a scenario can leave behind: probe and bulk abilities, the bulk and stats effects,
	 * hitstun and escape, both weapons' stat effects, the EX and super meters. The loadout (kit) is not touched: scenarios count its revision from
	 * where it is. Everything goes through queued moves or out-of-band ability system changes, like the helpers it calls.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	bool ServerResetTestState(FVector Offset);

	/**
	 * The step snapshot entry (H6) of this entity as a JSON object: tracker pose, velocity, up, hitstop flag, action state, the hurt state of every hurtbox
	 * of the set, the reaction flags and the damage percent. Frame INDEX_NONE (or any negative) reads the latest input history frame. Empty string when
	 * the pawn has no combat component or no sync state yet. Valid on every machine (an interpolated proxy shows its presentation pose).
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test")
	FString ReadSnapshotEntry(int32 Frame = -1);

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

	/** Loaded into TestMappingContext at BeginPlay when that is empty. Config: [/Script/GASP_Giustino2.GSTN_GASNPPTestComponent] in DefaultGame.ini */
	UPROPERTY(Config, EditDefaultsOnly, Category = "GASNPP Test")
	TSoftObjectPtr<UInputMappingContext> DefaultTestMappingContext;

	// ---- M3 spike probes. The Server* functions run on the authority only, from outside the simulation (a plain game thread
	// call, like a loadout component would), and return false / 0 elsewhere.

	/**
	 * Grants one trigger probe ability to this pawn: "EventRaiser", "EventTarget", "TagRaiser" or "TagTarget" (see
	 * GSTN_TestProbeAbilities.h). Grant at most one raiser at a time. Returns false if it is already owned or unknown.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerGrantProbe(const FString& Name);

	/**
	 * Grants (true) or removes (false) the platform jump kit, UGSTN_GA_PlatformJump and UGSTN_GA_PlatformFastFall, which the test binding set
	 * binds to Command.Movement.Jump / FastFall. Authority only, from outside the simulation. Returns how many specs changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	int32 ServerSetJumpKit(bool bGrant);

	/**
	 * Grants (true) or removes (false) the abilities of the movement kit that the test binding set binds: Dash is UGSTN_GA_Dash, Dodge is the ground
	 * dodge, the air dodge and the escape dodge, Interact is the placeholder grab and pick up. Authority only, from outside the simulation: a kit without
	 * a dodge is a pawn whose Dodge flag is false. Returns how many specs changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	int32 ServerSetMoveKit(bool bDash, bool bDodge, bool bInteract);

	/** Applies (true) or removes (false) an infinite effect granting Combat.State.Hitstun, the stand-in of the hit reaction of M4. Authority only */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerSetHitstun(bool bApply);

	/** Applies (true) or removes (false) an infinite effect granting Combat.Lock.Movement: the locomotion speed is zero but the stick still reaches the combat tick (DI). Authority only */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerSetMovementLock(bool bApply);

	/** Applies (true) or removes (false) an infinite effect granting Test.State.Escape: the pawn then uses the escape dodge, which is not blocked in hitstun. Authority only */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerSetEscapeKit(bool bApply);

	/** Applies (true) or removes (false) an infinite effect granting Test.State.Planted: a grounded pawn then dodges with the planted ground dodge (no take-off). Authority only */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerSetPlanted(bool bApply);

	/**
	 * Makes the attacker outcome of the process answer Ricochet for every hit (true) or Normal (false). The outcome registry is one per process, so every
	 * PIE world and every machine of it decides the same way: switch it between attacks, never while a hit is in flight
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Hits")
	static void SetForceRicochet(bool bForce);

	/** "start,end" of the hurtbox window (frames from the start of the timeline) of the default ground (false) or air (true) dodge, read from the class data; "-1,-1" if none */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Hits")
	static FString GetDodgeWindow(bool bAir);

	/**
	 * Sets Mover's gravity override (acceleration in cm/s^2 world space) on THIS machine's copy of the pawn, or clears it. Call it on every
	 * machine, while the pawn stands still: gravity is a Mover component setting, not simulated state, so every copy must hold the same value
	 * before it matters (the tilted gravity scenario)
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool SetTestGravity(FVector Acceleration, bool bEnable);

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

	/**
	 * Server only: puts the primary weapon's stats effect on the pawn (what the loadout does at spawn), or takes both weapons'
	 * effects off. Weapon scenario setup and cleanup, from outside the simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	bool ServerEquipPrimaryWeapon(bool bEquip);

	/**
	 * Server only: applies the loadout of the C15 test content (/Game/GASNPPTest/Combat) to this pawn through the combat component's
	 * ServerApplyLoadoutChange: the kit named KitName and, when SupportWeaponName is not empty, that weapon as the additional weapon.
	 * The loadout component is added to the PlayerState at runtime on the first call (the shared PlayerState Blueprint is not edited).
	 * Returns "OK", or the findings of the loadout rules joined with " | ".
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Loadout")
	FString ServerApplyTestLoadout(const FString& KitName, const FString& SupportWeaponName);

	/** ServerApplyTestLoadout with supers: SuperNamesCsv lists the SUP_Test_* assets in slot order (comma separated, may be empty) */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Loadout")
	FString ServerApplyTestLoadoutWithSupers(const FString& KitName, const FString& SupportWeaponName, const FString& SuperNamesCsv);

	/** Server only: applies an instant meter grant (0 = ExMeter +60, 1 = SuperMeter +100). An out-of-band ability system change, like a grant */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Loadout")
	bool ServerGrantMeter(int32 Which);

	/** Server only: puts the loadout component on the PlayerState if there is none yet (clients get it by replication). Null without a PlayerState */
	UGSTN_LoadoutComponent* EnsureLoadoutComponent();

	/** Server only: sets the DamagePercent of the health set (the hit scenarios start victims at a damage). An out-of-band attribute write that the owner reconciles */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Hits")
	bool ServerSetDamagePercent(float Percent);

	/**
	 * The combat events this world published about this pawn since the last clear (only the authority's world publishes), one per entry as
	 * "role:type:frame:magnitude:tag,tag..." with role I (the pawn was the instigator) or T (the pawn was the target), type HitLanded, HitReceived or KO
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Hits")
	FString GetHitEventLog() const;

	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Hits")
	void ClearHitEventLog() { HitEventLog.Reset(); }

	/** Same as setting bStatsProbeEnabled, for Python (the harness calls it on every copy of the pawn, in every world) */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	void SetStatsProbeEnabled(bool bEnabled) { bStatsProbeEnabled = bEnabled; }

	/** Clears the activation counters of the trigger probes only (ResetCounters clears them too, with the rollback counters) */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Probe")
	void ResetProbeCounters() { ProbeCounters.Reset(); TimelineStartedCounts.Reset(); WeaponSwitchedCount = 0; }

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

	/**
	 * Mover's state as of the frame being simulated. The sim history and the mode log read velocity and mode here, not from Mover's own
	 * queries: those return the presentation copy, which re-simulated ticks do not update (a re-simulated tick then reported the
	 * pre-rollback mode and velocity next to its re-simulated position, and the harness saw mode flips and speeds that never happened).
	 */
	UPROPERTY(Transient)
	TObjectPtr<class UGSTN_MoverSimStateTracker> SimState;

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	bool bInputReady = false;

	struct FSimHistoryEntry
	{
		int32 SimTimeMs = 0;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FName Mode;
		bool bResimulating = false;
		TArray<FString> Tags;
	};
	TArray<FSimHistoryEntry> SimHistory;

	/** Where the pawn was at BeginPlay on the authority */
	FVector SpawnLocation = FVector::ZeroVector;
	bool bHasSpawnLocation = false;

	TMap<FName, FGSTN_ProbeCounters> ProbeCounters;

	/** TimelineStarted events the authority published for this pawn, by source name (adopted, command, forced). Empty on a client: nothing is published there */
	TMap<FName, int32> TimelineStartedCounts;
	int32 TimelineStartedListener = INDEX_NONE;

	/** HitLanded, HitReceived and KO events about this pawn (GetHitEventLog) and the listener ids */
	TArray<FString> HitEventLog;
	int32 HitEventListener = INDEX_NONE;

	/** WeaponSwitched events the authority published for this pawn (nothing on a client) */
	int32 WeaponSwitchedCount = 0;
	int32 WeaponSwitchedListener = INDEX_NONE;

	/** Simulation frame of the latest tick of the ability system, to date the probe activations */
	int32 AbilityFrame = -1;

	/** Last Mover mode and floor-hit state logged by gstn.test.LogMoverModeChanges */
	FName LoggedMode;
	bool bLoggedFloor = false;

	// Server-side handles of the infinite effects applied by the probes
	FActiveGameplayEffectHandle BulkEffectHandle;
	FActiveGameplayEffectHandle StatsEffectHandle;
	FActiveGameplayEffectHandle HitstunEffectHandle;
	FActiveGameplayEffectHandle MovementLockEffectHandle;
	FActiveGameplayEffectHandle EscapeEffectHandle;
	FActiveGameplayEffectHandle PlantedEffectHandle;

	// Stats probe: the values captured before the first rewrite, and what was written last
	bool bStatsBaseCaptured = false;
	float BaseMaxSpeed = 0.f;
	FVector BaseGravity = FVector::ZeroVector;
	float AppliedSpeedScale = 1.f;
	float AppliedGravityScale = 1.f;
};

// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Events/GSTN_SimEvent.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_ButtonMap.h"
#include "Jump/GSTN_GravityPhases.h"
#include "Jump/GSTN_JumpLogic.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_CommandSourceInterface.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Simulation/GSTN_InputSimulator.h"
#include "Simulation/GSTN_WeaponStatsSwap.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "World/GSTN_StepSnapshot.h"
#include "GSTN_CombatSimComponent.generated.h"

class UGSTN_CombatRules;
class UGSTN_CombatWorldSubsystem;
class UGSTN_CommandBindingSet;
class UGSTN_HurtboxSetDefinition;
class UGSTN_GA_BaseCombat;
class UGSTN_GA_PlatformJump;
class UGSTN_InputConfig;
class UGSTN_InputContextProvider;
class UGSTN_LoadoutComponent;
class UGSTN_MoverSimStateTracker;
class UAnimMontage;
class UGameplayEffect;
class UMoverComponent;
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
class GSTNCOMBATNP_API UGSTN_CombatSimComponent : public UNetworkPredictionComponent, public IGSTN_InputSimHost, public IGSTN_CommandSourceInterface, public IGSTN_SnapshotSource
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

	/**
	 * Network Prediction calls it on every instance of the model, whenever the simulation rolls back to a state. Drops the hits that
	 * were written for frames the re-simulation will run again (their attackers write them again)
	 */
	void RestoreFrame(const FGSTN_CombatSyncState* Sync, const FGSTN_CombatAuxState* Aux);

	/**
	 * Whether this instance runs the combat tick on this machine: the authority and the locally controlled pawn do, an interpolated
	 * simulated proxy never does. This is where hits are delivered, not a branch of the simulation: nothing a tick computes depends on it
	 */
	bool TicksOnThisMachine() const;

	/**
	 * Delivers a hit to this victim (combat-m4-hits 7). Written only where the instance ticks, so a client never queues a hit for a
	 * remote victim it only interpolates. @return false when nothing was queued
	 */
	virtual bool PostHit(const FGSTN_HitInboxEntry& Entry) override;

	/**
	 * The rules the attacker side runs the hit pipeline of, to predict the attacker's own hitstop and meter (combat-m4-hits 6). Resolved at
	 * BeginPlay from the combat settings (the standard rules when none is set); a test or a game replaces them here, outside the simulation
	 */
	void SetCombatRules(UGSTN_CombatRules* Rules);
	const UGSTN_CombatRules* GetCombatRules() const { return CombatRules; }

	/**
	 * "frame:victim:group:hitbox:hurtbox:ricochet:attackerHitstop;..." of the hits the attacker side made, the last 512 frames. Written
	 * while gstn.Combat.Debug.HitLog is 1. A re-simulated frame replaces its entry, so an owner's log after a reconcile compares with the
	 * authority's frame by frame. Debug only: never read by the simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	FString GetHitLog() const;

	/** What the inbox of this victim received, one entry per drained hit, the last 512 frames. Under the same switch as the hit log */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	FString GetInboxLog() const;

	/**
	 * "frame:hitbox:space:x,y,z;..." of the world centre of every open hitbox, the last 512 frames, written under the same switch as the hit
	 * log. The harness checks the frame of a hit against the geometry with it
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	FString GetHitboxTrace() const;

	/** The victim side's drain at the start of its combat tick of Frame: the entries stamped before it, in (AttackerId, hitbox) order, appended to Out */
	void DrainHitInbox(int32 Frame, TArray<FGSTN_HitInboxEntry>& Out) { HitInbox.Drain(Frame, Out); }

	const FGSTN_HitInbox& GetHitInbox() const { return HitInbox; }

	// IGSTN_SnapshotSource. The entry of this entity for the step snapshot: the state at the end of the previous frame (the sync state and the
	// Mover sim state tracker), or the presentation state of an interpolated proxy
	virtual bool ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out) override;

	/** The world subsystem this component registers with and invalidates on RestoreFrame. Bound at BeginPlay; a test binds one by hand */
	void SetCombatWorldSubsystem(UGSTN_CombatWorldSubsystem* Subsystem);

	/** Registers with the world subsystem once the NP proxy has its id (a client gets it from the replication of the proxy, after BeginPlay). Called until it has */
	void TryRegisterWithWorld();

	// IGSTN_InputSimHost
	virtual float GetFacingYaw() const override;
	virtual void GetOwnerTags(FGameplayTagContainer& OutTags) const override;
	virtual void AppendContextTags(FGameplayTagContainer& OutTags) const override;
	virtual uint8 GetContextFingerprint() const override;
	virtual void AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const override;
	virtual bool CanExecuteBinding(int32 BindingIndex) const override;
	virtual bool IsCommandAllowed(int32 CommandIndex) const override;

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

	/**
	 * The combat clock of the frame being simulated: the Network Prediction frame minus every hitstop frame so far
	 * (Core.PausedFramesTotal), so it stands still while the combatant is in hitstop and a wait counted on it does not elapse then.
	 * Same contract as GetActivationContext: call it from inside the simulation (an ability's activation or tick), where it is
	 * deterministic and the same on every machine and in every re-simulation. INDEX_NONE before the first simulated frame
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Clock")
	int32 GetCombatClock() const;

	/** Whether the combat state of the frame being simulated says a timeline is running (EGSTN_ActionState::InTimeline). Simulation read, like GetCombatClock */
	UFUNCTION(BlueprintPure, Category = "Combat|Clock")
	bool IsInTimeline() const;

	/** The simulation frame the combat state is on (the latest frame of the input history), INDEX_NONE before the first. Simulation read, like GetCombatClock */
	int32 GetSimulationFrame() const;

	/**
	 * Whether the combat state of the frame being simulated says a timeline of exactly this ability class is running, and the
	 * frame it started on (the ability compares it with the one it started on to notice a replacement by the same class)
	 */
	bool IsTimelineRunning(const UClass* AbilityClass, int32& OutStartFrame) const;

	/**
	 * The camera aim of the command of the frame being simulated (UGSTN_InputConfig::bSendCameraAim, combat-m3 7.10): control
	 * rotation pitch and yaw, roll 0. The simulation keeps it on its way past the ability system's tick of the same frame, rewritten
	 * every simulated frame, so a re-simulation reads the command it replays. Zero when the command carries no aim (HasAim)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Input")
	FRotator GetAimRotation() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Input")
	bool HasAim() const { return FrameAim.bHasAim; }

	/**
	 * The jump state of the frame being simulated (combat-m3 7.9): phase, air jumps used, fast fall. Same contract as GetActivationContext:
	 * a simulation read, called from an ability's activation or tick. False before the first simulated frame
	 */
	bool ReadJumpState(FGSTN_JumpState& OutState) const;

	/** The tracker of the owner's Mover component (the movement facts of the end of the previous frame: up direction, velocity, ground tag), null without a Mover component. The movement abilities read the local gravity frame from it */
	UGSTN_MoverSimStateTracker* GetMoverTracker() { return GetJumpTracker(); }

	/**
	 * Rewrites Mover's gravity override to the authored gravity scaled by the gravity phase of the jump state of the frame being
	 * simulated (hold, cut, fall, fast fall), along the local gravity whatever its direction. Called by the stats movement modifier
	 * from OnPreMovement, i.e. on every simulated substep. A pawn without the combat stats, or that never leaves scale 1, is left alone
	 */
	void ApplyJumpGravity(UMoverComponent& MoverComp, double DeltaSeconds);

	/** Latest simulated (or interpolated) combat state */
	const FGSTN_CombatSyncState* GetSyncState() const;

	/**
	 * The compiled commands and bindings the combat state is on: the meaning of the indices in the sync state and in the commands. Inside the
	 * simulation (an ability's activation or tick) it is the table of the frame being simulated; elsewhere the one of the latest state
	 */
	const FGSTN_CommandTable& GetCommandTable() const;

	/** The table set of exactly this loadout revision (0 = the Blueprint's), null if this machine does not have it (never built, or expired) */
	const UGSTN_CombatTableSet* FindTableSet(int32 Revision) const;

	/**
	 * Adds a table set built for a loadout revision (game thread, outside the simulation: the loadout component's apply, on every machine).
	 * A set of the same revision is replaced. Keeps the Blueprint's set and the newest MaxRetainedTableRevisions others; a tick that finds
	 * no set for the revision it demands runs closed (no commands) until it exists
	 */
	void RegisterTableSet(UGSTN_CombatTableSet* Set);

	/** Frames the combat tick ran with the gate closed (the demanded table was missing). Diagnostic, not state: it also counts re-simulated frames */
	int32 GetGateClosedFrames() const { return GateClosedFrames; }

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

	/**
	 * Timeline abilities that no command binds but something else starts (a gameplay event or tag trigger, an effect, another
	 * ability, AI) and that the combat tick adopts (combat-m3 7.4). They join the timeline registry after the bound ones, in
	 * this order, so the list is part of the sync contract like the bindings: the same on every machine, fixed after BeginPlay
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Timeline")
	TArray<TSubclassOf<UGSTN_GA_BaseCombat>> AdoptableTimelines;

	/**
	 * Queues the permanent stats movement modifier (FGSTN_StatsMovementModifier) on the owner's Mover component once its
	 * simulation exists, on the authority and the owning client. It makes the character's speeds follow its WalkSpeed stat
	 * and stops them under Combat.Lock.Movement; it does nothing for an ability system without the combat attribute set.
	 * Turn it off for a pawn whose speeds the combat stats must not touch
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Stats")
	bool bQueueStatsModifier = true;

	/**
	 * The stats effect of each weapon the combatant can switch to, in loadout order (0 = primary). A weapon switch track of a
	 * running timeline picks the next index from this list's length and the combat tick requests SwapWeaponStats; the ability
	 * system's pre-tick then removes the old weapon's effect and applies the new one (FGSTN_WeaponStatsSwap). The primary weapon
	 * does not change during a match, so the switch is primary <-> support. An entry may leave Effect empty: a support weapon
	 * inherits the kit's stats by default. The loadout fills it; fixed after BeginPlay like the bindings, the same on every machine
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Stats")
	TArray<FGSTN_WeaponStatsEntry> WeaponStats;

	/** The hurtboxes this entity is hit through (combat-m4-hits 3). Soft: loaded once at BeginPlay. None = the entity cannot be hit */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hits")
	TSoftObjectPtr<UGSTN_HurtboxSetDefinition> HurtboxSet;

	/** Button whose press opens a hitstop window. -1 = none */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "-1", ClampMax = "31"))
	int32 HitstopTestButton = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "1"))
	int32 HitstopTestFrames = 8;

	/** Setter for the harness (see HitstopTestMontage): the length of the window the debug button opens */
	UFUNCTION(BlueprintCallable, Category = "Combat|Spike")
	void SetHitstopTestFrames(int32 Frames) { HitstopTestFrames = FMath::Max(Frames, 1); }

	/**
	 * Montage the debug hitstop button starts on the same frame as the window (a synced montage, so it holds through the window like an
	 * attack's). None = no montage. The harness sets it from Python through SetHitstopTestMontage (a live component must not be edited
	 * with set_editor_property)
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Spike")
	TObjectPtr<UAnimMontage> HitstopTestMontage = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Combat|Spike")
	void SetHitstopTestMontage(UAnimMontage* Montage) { HitstopTestMontage = Montage; }

	/** "playing,time,latestFrame,hitstopStart,hitstopEnd" of the synced montage and the window, for the harness: the same on the authority and the owner while the window is open */
	UFUNCTION(BlueprintCallable, Category = "Combat|Spike")
	FString GetHitstopMontageInfo() const;

	/**
	 * "frame:time;frame:time;..." of the synced montage after each simulated frame (the last 256), written while HitstopTestMontage is set. A
	 * re-simulated frame overwrites its entry, so the owner's trace after a reconcile is comparable with the authority's frame by frame, however
	 * rarely the editor ticks
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Spike")
	FString GetHitstopMontageTrace() const;

	/**
	 * Instant effect a Freeze request applies to this entity (combat-m4-hits 8): holds its Mover still for the window and restores the
	 * velocity afterwards. Replace it with UGSTN_GE_FreezeZeroVelocity for a victim that leaves the freeze at rest. The duration travels
	 * as the SetByCaller Combat.SetByCaller.FreezeSeconds
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hits")
	TSubclassOf<UGameplayEffect> FreezeEffect;

	/**
	 * Server only, from outside the simulation (a plain game thread call, like the loadout component's apply): writes the revision
	 * of the loadout the tables were built from into the pending sync state (combat-m3 7.8). It travels to the clients with the next
	 * correction, and the combat tick compares it with the revision of its local table (FGSTN_CombatSyncCore::IsLoadoutCurrent).
	 * Returns false on a machine without authority or before the proxy has a pending state.
	 */
	bool ServerWriteLoadoutRevision(uint16 Revision);

	/**
	 * Server only, from outside the simulation: a kit change by pickup (combat-m3 direction of 2026-10-23). Judges the loadout first
	 * (a refused one changes nothing, the running timeline included), then ends the running timeline (its ability and the action
	 * state), applies the loadout on the loadout component (the re-grant and the stats effects of the new kit) and writes the new
	 * revision into the sync state through ServerWriteLoadoutRevision. Returns false with the findings in OutErrors when refused.
	 * The grants reach the owning client through the simulation's sync state: one reconcile per change, like any grant.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool ServerApplyLoadoutChange(UGSTN_LoadoutComponent* Loadout, const FGSTN_LoadoutData& Requested, TArray<FString>& OutErrors);

	/**
	 * Feeds the loadout's compiled table to the simulation: on every machine, when the loadout component applies a loadout (the server
	 * directly, a client from the OnRep), the table is converted into a table set and registered under the loadout's revision. The set
	 * takes over at the revision boundary of the combat tick. The component finds a loadout component by itself (on the owner, else on the
	 * pawn's PlayerState) and binds it; call this to bind one explicitly. Binding the same component twice does nothing
	 */
	void BindLoadoutComponent(UGSTN_LoadoutComponent* Loadout);

	/** The stats effect entries of a resolved loadout's weapons, in loadout order (a weapon without a stats effect leaves its Effect empty) */
	static TArray<FGSTN_WeaponStatsEntry> BuildWeaponStats(const FGSTN_ResolvedLoadout& Resolved);

protected:
	virtual void InitializeNetworkPredictionProxy() override;

private:
	/** Builds the command table and the button map from the assets (BeginPlay, before any simulation tick) */
	void CompileInput();

	void BindAbilitySystem();

	UFUNCTION()
	void HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision);

	/** Looks for the loadout component to bind, a few times a second, until one is bound (the PlayerState and its component arrive after BeginPlay on a client) */
	void PollLoadoutComponent();

	TWeakObjectPtr<UGSTN_LoadoutComponent> BoundLoadout;
	int32 LoadoutPollTicks = 0;

	/** Server: the simulation frame of the last accepted loadout change, INDEX_NONE before the first (MinFramesBetweenLoadoutChanges) */
	int32 LastLoadoutChangeFrame = INDEX_NONE;

	/** The tracker of the owner's Mover component, bound on first use; null without a Mover component */
	UGSTN_MoverSimStateTracker* GetJumpTracker();

	/** The stats and settings the jump state machine reads, from the ability system of the frame being simulated. False without the combat attribute set */
	bool BuildJumpParams(FGSTN_JumpParams& OutParams) const;

	/** Step 5a of the combat tick: the jump bookkeeping of the frame from the Mover state of the end of the previous one. HeldButtons are the buttons of this frame's command */
	void StepJump(int32 Frame, uint32 HeldButtons, FGSTN_CombatSyncCore& Core);

	/** Step 5b: a request of this frame that is a jump or a fast fall turns into the state change (take-off, fast fall) */
	void ApplyJumpRequest(int32 Frame, int32 BindingIndex, FGSTN_CombatSyncCore& Core);

	/** Whether a jump or fast fall binding may be requested now, from the jump state this frame is writing. Other bindings are never refused here */
	bool IsJumpBindingAllowed(int32 BindingIndex) const;

	/** The movement facts and stats of the frame the tick is on, read once per tick for the gate and the take-off. Rewritten every tick, never state */
	FGSTN_JumpMoverSample FrameJumpSample;
	FGSTN_JumpParams FrameJumpParams;
	bool bFrameJumpValid = false;

	/** The authored gravity next to what the phases wrote over it; configuration mirrored from outside the simulation, see FGSTN_GravityBase */
	FGSTN_GravityBase GravityBase;

	TWeakObjectPtr<UGSTN_MoverSimStateTracker> JumpTracker;

	/** Queues the stats modifier on Mover once (see bQueueStatsModifier); called from the component tick until it has */
	void TryQueueStatsModifier();
	bool bStatsModifierQueued = false;

	void HandleAbilitySimulationPreTick(const FAbilitySystemTimeStep& TimeStep);

	/** Applies the freeze effect for Frames frames of StepMs each. Only from the ability system's pre-tick (rule 2: Mover is moved through a queued move) */
	void ExecuteFreeze(TSubclassOf<UGameplayEffect> Effect, int32 Frames, float StepMs);

	/** Montage time per frame, see GetHitstopMontageTrace. Debug only: never read by the simulation */
	TMap<int32, float> HitstopMontageTrace;

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	// Compiled from the assets once: read-only while simulating, so not rolled back
	FGSTN_ButtonMap ButtonMap;

	/**
	 * Everything the loadout decides (command table, timeline registry, tiers, jump roles, weapon stats) lives in table sets, one per
	 * loadout revision (combat-m3 9, loadout-table-swap-design). The active table is a pure function of the rolled-back state: the set of
	 * FGSTN_CombatSyncCore::TableRevision, never "the current table" of this component, so a re-simulated frame meets the table it had
	 * the first time. Revision 0 is the Blueprint's binding set and is pinned; the others are the newest MaxRetainedTableRevisions
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_CombatTableSet>> TableSets;

	/** A set with no commands: the table of a closed frame and the answer before any set exists. Never null after BeginPlay */
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CombatTableSet> EmptyTableSet;

	/** Registers a set without the retention rules of RegisterTableSet's caller side (BeginPlay's revision 0) and loads what the registry classes need */
	void AdoptTableSet(UGSTN_CombatTableSet* Set);

	/** The tables of the frame being simulated, set at the top of every combat tick and read by everything that frame (the ability system's pre-tick included). Rewritten every tick, never state */
	const UGSTN_CombatTableSet* FrameTables = nullptr;

	/** The set of the previous frame's TableRevision: the registry a CancelTimeline request of a swap frame refers to. Same lifetime as FrameTables */
	const UGSTN_CombatTableSet* FramePrevTables = nullptr;

	/** Whether the frame ran with the gate closed. Same lifetime as FrameTables */
	bool bFrameGateClosed = false;

	int32 GateClosedFrames = 0;

	/** The set the frame is on, else the one of the latest state, else the empty one. Never null after BeginPlay */
	const UGSTN_CombatTableSet& Tables() const;

	/** The frame data of registry entry Index (its class default) of the set, null if the index is not one */
	static const FGSTN_CombatTimelineData* GetRegistryTimeline(const UGSTN_CombatTableSet& Set, int32 Index);

	/** Scratch of the combat tick, reused every frame (no allocation); the evaluation of the frame the gate and the commit read */
	FGSTN_TimelineOutput TimelineOutput;

	/** The state the gate (IsCommandAllowed) reads: set at the top of the combat tick, valid only inside it */
	const FGSTN_CombatSyncCore* GateCore = nullptr;

	// The assets InputConfig / SlotBindings resolved to at setup, strongly held because the table points into them and the
	// input source reads the config every frame. Never resolved in a tick
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_InputConfig> ActiveInputConfig;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CommandBindingSet> ActiveSlotBindings;

	FDelegateHandle PreTickHandle;

	// Written by the simulation tick on every machine, published and emptied in FinalizeFrame (authority only); not sync state
	FGSTN_SimEventOutbox EventOutbox;

	/**
	 * The aim of the command of the frame the simulation is on (a copy of three fields, not a pointer: the input buffer's memory
	 * is not ours). Written at the top of every combat tick, read by the ability system's tick of the same frame; never a
	 * source of state, so it needs no rollback
	 */
	struct FFrameAim
	{
		bool bHasAim = false;
		uint16 Yaw = 0;
		uint16 Pitch = 0;
	};
	FFrameAim FrameAim;

	/** The team of the owner or of its controller, NoTeam without one (read when registering and at every snapshot) */
	FGenericTeamId ResolveTeam() const;

	TWeakObjectPtr<UGSTN_CombatWorldSubsystem> WorldSubsystem;
	int32 RegisteredEntityId = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_HurtboxSetDefinition> ActiveHurtboxSet;

	/**
	 * Until the victim side exists (H9) the entries of the inbox are drained and dropped at the top of every tick, so the inbox never fills; under
	 * gstn.Combat.Debug.HitLog the drain is logged as "frame:attacker:hitbox:hurtbox:hurtState:ricochet;..." (GetInboxLog)
	 */
	void DrainInboxForNow(int32 Frame);
	TArray<FGSTN_HitInboxEntry> InboxScratch;
	TMap<int32, FString> InboxLogByFrame;

	/** The attacker side of the combat tick for the frame being simulated (GSTN_CombatSimAttacker.cpp). Writes Core and the victims' inboxes */
	void RunAttackerStep(const FNetSimTimeStep& TimeStep, FGSTN_CombatSyncCore& Core, const FGSTN_StepSnapshot* Snapshot, const FGSTN_HitGroupMask& GroupMask);

	/** The attacker's pose at the end of the previous frame from the Mover tracker (the actor's own without one). False without an owner */
	bool ReadOwnPose(FTransform& OutTransform, FVector& OutUp);

	/** A timeline that began this frame: its move origin is the pose the attacker is in now */
	void StampMoveOrigin(FGSTN_CombatSyncCore& Core, int32 Frame);

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CombatRules> CombatRules;

	/** Scratch of the attacker step, reused every frame */
	FGSTN_TimelineFrameResult AttackerScratch;
	TArray<FGSTN_AttackerHit> AttackerHits;
	TArray<FGSTN_AttackerDebugShape> AttackerDebugShapes;
	bool bLoggedTargetOverflow = false;

	/** Debug mirrors of the attacker step, see GetHitLog / GetHitboxTrace. Not state */
	TMap<int32, FString> HitLogByFrame;
	TMap<int32, FString> HitboxTraceByFrame;

	/** Hits waiting for this victim's next tick. Local data, not sync state: the attackers' re-simulation rebuilds it after a rollback */
	FGSTN_HitInbox HitInbox;

	// Input production only (owning machine), not simulation state
	uint32 ScriptedButtons = 0;
	uint32 PendingTaps = 0;
	bool bUseScriptedButtons = false;
};

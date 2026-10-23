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
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CommandSourceInterface.h"
#include "Simulation/GSTN_InputSimulator.h"
#include "Simulation/GSTN_WeaponStatsSwap.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "GSTN_CombatSimComponent.generated.h"

class UGSTN_CommandBindingSet;
class UGSTN_GA_BaseCombat;
class UGSTN_InputConfig;
class UGSTN_InputContextProvider;
class UGSTN_MoverSimStateTracker;
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

	/**
	 * Rewrites Mover's gravity override to the authored gravity scaled by the gravity phase of the jump state of the frame being
	 * simulated (hold, cut, fall, fast fall), along the local gravity whatever its direction. Called by the stats movement modifier
	 * from OnPreMovement, i.e. on every simulated substep. A pawn without the combat stats, or that never leaves scale 1, is left alone
	 */
	void ApplyJumpGravity(UMoverComponent& MoverComp);

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

	/** Button whose press opens a hitstop window. -1 = none */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "-1", ClampMax = "31"))
	int32 HitstopTestButton = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Spike", meta = (ClampMin = "1"))
	int32 HitstopTestFrames = 8;

	/**
	 * Server only, from outside the simulation (a plain game thread call, like the loadout component's apply): writes the revision
	 * of the loadout the tables were built from into the pending sync state (combat-m3 7.8). It travels to the clients with the next
	 * correction, and the combat tick compares it with the revision of its local table (FGSTN_CombatSyncCore::IsLoadoutCurrent).
	 * Returns false on a machine without authority or before the proxy has a pending state.
	 */
	bool ServerWriteLoadoutRevision(uint16 Revision);

protected:
	virtual void InitializeNetworkPredictionProxy() override;

private:
	/** Builds the command table and the button map from the assets (BeginPlay, before any simulation tick) */
	void CompileInput();

	void BindAbilitySystem();

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

	/** Per binding: EGSTN_PlatformJumpRole + 1 for a UGSTN_GA_PlatformJump binding, 0 for any other. Built with the table, read-only while simulating */
	TArray<uint8> BindingJumpRole;

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

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	// Compiled from the assets once: read-only while simulating, so neither is rolled back
	FGSTN_CommandTable CommandTable;
	FGSTN_ButtonMap ButtonMap;

	/**
	 * The timeline registry (combat-m3 7.1 "index in the timeline registry"): the timeline ability classes of the command table's
	 * bindings, in binding order, each once. The index is what the combat sync state stores. Built with the table, identical on
	 * every machine, read-only while simulating. BindingTimeline[b] is the registry index of binding b (INDEX_NONE for a plain
	 * ability) and TimelineTier[r] the priority tier of the first command bound to entry r (the running tier of the override rule)
	 */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UGSTN_GA_BaseCombat>> TimelineRegistry;

	/** The timeline assets and montages of the registry classes, loaded with it and kept alive here: the classes' default objects keep nothing */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> TimelineContent;
	TArray<int16> BindingTimeline;
	TArray<int32> TimelineTier;
	TArray<int32> TimelineDefaultOverride;

	/** The frame data of registry entry Index (its class default), null if the index is not one */
	const FGSTN_CombatTimelineData* GetRegistryTimeline(int32 Index) const;

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

	// Input production only (owning machine), not simulation state
	uint32 ScriptedButtons = 0;
	uint32 PendingTaps = 0;
	bool bUseScriptedButtons = false;
};

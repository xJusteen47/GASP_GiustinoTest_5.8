// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "Abilities/GSTN_MoveActions.h"
#include "Abilities/GSTN_MoveEvents.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_GA_BaseCombat.generated.h"

class UGSTN_CombatTimeline;

/**
 * A combat ability that runs a timeline (combat-m3 6.2): frame data (total and actionable frames, the startup override window,
 * cancel windows, a cancellable tail, a montage) the combat tick plays, so the ability itself holds almost no logic.
 *
 * The combat component owns the timeline clock. When a command starts a timeline ability the combat tick writes the timeline
 * state (which registry entry, from which frame) into the combat sync state and the ability system activates the ability in the same
 * frame; the ability finds itself in that state, plays its montage, and ends when the state no longer names it (the timeline reached
 * its end, was replaced by another command, or was cancelled). The cost is committed by the combat tick when the startup override
 * window ends (CommitTimeline request), so a timeline replaced inside the window leaves no trace but its turn.
 *
 * Anything that is not a command can start the ability too (a gameplay event or tag trigger, an effect, another ability, AI). It
 * finds no start in the combat state, so it only marks itself as waiting (the loose tag Combat.Ability.AwaitingAdoption and the sync
 * variable behind IsAwaitingAdoption) and the combat tick of the next frame adopts it: it writes the timeline state with source
 * Adopted, and the ability starts in its own tick of that frame (combat-m3 7.4). The class has to be in the component's registry
 * for that (UGSTN_CombatSimComponent::AdoptableTimelines, or a command binding); an ability the registry does not know is cancelled
 * after MaxAdoptionWaitFrames. The frame data comes from TimelineAsset when it is set,
 * else from Timeline; both are read from the class default object by the combat component, never from an instance, because the
 * component needs it before the ability runs.
 *
 * Ricochet (combat-m4-hits 3b, M4 H12): when the attacker side decides that a hit of the timeline bounced, the combat tick ends the timeline
 * into a recoil (FGSTN_RicochetResponse: the attack's own, else the weapon's, else the kit's). The combat state keeps naming this ability
 * while the recoil lasts, and the ability plays it: the recoil montage replaces the timeline's on the synced montage slot, the cue fires, and
 * the optional movement is a push back (the slide of the victim side, through the queued-move path). RecoilFrames are locked; the combatant is
 * actionable after them and the rest of the recoil montage plays on as the cancellable tail (the next synced montage replaces it, a hit stops it).
 * Nothing here knows about the server or about re-simulation: every read is the combat state of the frame, every variable a sync variable.
 *
 * Blueprint move events (CP1 of the Blueprint seams): a Blueprint child reacts to its own move with On Move Started, On Move Key (an Event key
 * of the timeline crossed), On Move Hit Confirmed, On Move Ended and, opt-in, On Move Frame. They are SIMULATION events: raised inside the
 * ability's simulation step on every machine and again in every re-simulation (never while a frame is being restored), each exactly once
 * per simulated path, because their cursors are sync variables. A handler may do what simulation code may do (read the combat state and the
 * input questions of the combat component, write FSync* variables, wait combat frames, apply effects, send gameplay events); it must not read
 * the world, the Mover component or anything that is not rolled back, and it must not use latent nodes (Delay, timers). Presentation belongs
 * to the cosmetic hooks (CP4)
 *
 * Event keys and move actions (CP2): when the timeline crosses an Event key the ability fires the key's cue and gameplay event, runs its move
 * actions (FGSTN_MoveAction: fire a projectile, send an event, apply an effect, open a tag window), then raises On Move Key, all in that order, on
 * the same cursor and with the same once-per-crossing rule as the event. A graph runs the same actions through the request nodes below (Send
 * Move Event, Apply Move Effect, Open Move Tag Window, Run Move Action); they do nothing outside the simulation step of an active move
 */
UCLASS(Abstract, Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_BaseCombat : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_BaseCombat(const FObjectInitializer& ObjectInitializer);

	/** The inline frame data; ignored when TimelineAsset is set */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timeline")
	FGSTN_CombatTimelineData Timeline;

	/** A shared timeline asset; wins over the inline one. Soft: loaded when the ability is granted, never in the simulation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timeline")
	TSoftObjectPtr<UGSTN_CombatTimeline> TimelineAsset;

	/** The frame data this class runs by default: the asset's when it is set and loaded, else the inline one */
	const FGSTN_CombatTimelineData& GetTimelineData() const;

	/**
	 * The frame data this activation runs (M11 L5b): the one of the timeline registry entry the combat state started it with, which
	 * may be a binding's TimelineOverride or a kit mod's derived timeline, else GetTimelineData. Everything the running ability reads
	 * (montage, rate, movement track) goes through this, so a modded move plays its own data, the same on every machine (the entry is
	 * a sync variable, the table set a pure function of the loadout revision)
	 */
	const FGSTN_CombatTimelineData& GetRunningTimelineData() const;

	/** The timeline registry entry this activation started with, INDEX_NONE before it started */
	int32 GetRunningTimelineEntry() const { return RunningEntry.Value; }

	/** Loads the soft references the simulation needs (the timeline asset, the montage) and keeps them in this instance. Call outside the simulation; the avatar being set does */
	void PreloadTimelineContent();

	/**
	 * Loads the same content and appends the loaded objects to OutKeep, for an owner that has to keep them alive itself: the combat
	 * component does this for every registry class, because it reads the class default object, which holds nothing. Not in a tick
	 */
	void LoadTimelineContent(TArray<TObjectPtr<UObject>>& OutKeep) const;

	/** The montage and the movement curve of a ricochet response, appended to OutKeep (the table set does it for the weapon and kit fallbacks). Not in a tick */
	static void LoadRecoilContent(const FGSTN_RicochetResponse& Response, TArray<TObjectPtr<UObject>>& OutKeep);

	/** The soft content of one timeline's data (montage, recoils of the move and of its hitbox groups), loaded into OutKeep. Not in a tick */
	static void LoadTimelineDataContent(const FGSTN_CombatTimelineData& Data, TArray<TObjectPtr<UObject>>& OutKeep);

	/**
	 * The commit of a timeline that just started (cost and cooldown): when the ability refuses it (the meter dropped since the
	 * activation) the ability is cancelled, so the timeline never runs for free and the clock returns to actionable. Returns whether
	 * the commit went through. Split out of the combat tick so a test can run it on a plain ability system
	 */
	static bool CommitOrCancel(UAbilitySystemComponent& AbilitySystem, const FGameplayAbilitySpec& Spec);

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * Owner rule (2026-11-15, all moves): when a timeline ends by itself the rest of its montage keeps playing until the montage ends (with its
	 * own blend-out) or a new action takes over; the ability only RELEASES it (data with EndBlendOutFrames >= 0 still stops it, see
	 * ResolveMontageStop). A new action is any UGSTN_GA_Base activation, which calls this before it does anything else, or any synced montage,
	 * which replaces the slot by itself. Stops the released montage of every combat ability of AbilitySystem that still plays one (the montage's
	 * own blend-out). Simulation code: the released flag is a sync variable and the playing montage is the montage player's synced state
	 */
	static void StopReleasedMontages(UAbilitySystemComponent& AbilitySystem);

	/** True while the montage this ability released at its natural end may still be on the synced slot (sync variable) */
	bool HasReleasedMontage() const { return bMontageReleased.Value; }

	/** Combat frames since the timeline started (the clock minus the clock of its start), 0 when not started */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	int32 GetTimelineFrame() const;

	/** Frames an activation without a command waits to be adopted before it is cancelled (another ability won the adoption) */
	static constexpr int32 MaxAdoptionWaitFrames = 2;

	/** True from an activation the combat state did not start until the combat tick adopts it (or it is cancelled). Read by the combat tick through the spec's instance */
	bool IsAwaitingAdoption() const { return bAwaitingAdoption.Value; }

	/** True once the ability found itself in the combat state and its timeline runs (false while it waits for adoption and after it ended) */
	bool HasTimelineStarted() const { return bStarted.Value; }

	/**
	 * Raises On Move Frame on every simulated frame the move runs (after the key and hit events of the frame). OFF by default: a Blueprint
	 * graph that runs every frame on every combatant costs on weak hardware, and most moves only need keys and hits
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Blueprint")
	bool bTickMoveInBlueprint = false;

	/**
	 * The timeline frame the combat tick evaluated for this frame (it stands still in hitstop and in a hold, unlike GetTimelineFrame, which
	 * counts the combat clock), -1 outside the move and while a recoil plays. The frame On Move Key compares keys with
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	int32 GetMoveTimelineFrame() const;

	/** The hit groups of this move that confirmed a hit so far (bit i = hit group i), 0 before the first */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	int32 GetMoveHitConfirmGroups() const;

	// ---- Move actions (CP2): requests a move makes, never direct state writes. All of them refuse (return false) outside the simulation step
	// of an active, started move: in a frame being restored, in presentation code, after the end

	/** Whether a move action may run now: the move is active and started, and the ability system is simulating (not restoring) a frame */
	bool CanRunMoveActions() const;

	/** Sends a gameplay event to this move's own ability system (abilities triggered by it start; timelines among them are adopted) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Move Actions", meta = (DisplayName = "Send Move Event"))
	bool SendMoveEvent(FGameplayTag EventTag, float Magnitude = 0.f);

	/** Applies an effect to the owner (a movement goes through an effect with UApplyMoveGameplayEffectComponent: a queued Mover move) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Move Actions", meta = (DisplayName = "Apply Move Effect"))
	bool ApplyMoveEffect(TSubclassOf<UGameplayEffect> Effect, float Level = 1.f);

	/** Grants Tag for Frames combat frames or until the move ends (MaxTagWindows at a time; the same tag again re-arms it) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Move Actions", meta = (DisplayName = "Open Move Tag Window"))
	bool OpenMoveTagWindow(FGameplayTag Tag, int32 Frames = 10);

	/** Closes the window of Tag now, if it is open */
	UFUNCTION(BlueprintCallable, Category = "Combat|Move Actions", meta = (DisplayName = "Close Move Tag Window"))
	bool CloseMoveTagWindow(FGameplayTag Tag);

	/** Runs any move action (Make Instanced Struct of a move action type: a projectile shot, an event...), as an Event key would */
	UFUNCTION(BlueprintCallable, Category = "Combat|Move Actions", meta = (DisplayName = "Run Move Action"))
	bool RunMoveAction(UPARAM(meta = (BaseStruct = "/Script/GSTNCombatNP.GSTN_MoveAction")) const FInstancedStruct& Action);

	/** A random number the same on every machine and in every re-simulation of this frame. Salt tells two draws of the same frame apart */
	UFUNCTION(BlueprintPure, Category = "Combat|Move Actions", meta = (DisplayName = "Get Move Random Float"))
	float GetMoveRandomFloat(int32 Salt, float Min = 0.f, float Max = 1.f) const;

	/** A random integer in [Min, Max], the same on every machine and in every re-simulation of this frame */
	UFUNCTION(BlueprintPure, Category = "Combat|Move Actions", meta = (DisplayName = "Get Move Random Integer"))
	int32 GetMoveRandomInteger(int32 Salt, int32 Min, int32 Max) const;

	/** Tag windows a move holds at a time (Open Move Tag Window) */
	static constexpr int32 MaxTagWindows = 2;

protected:
	/**
	 * Called once when the timeline starts: in ActivateAbility for a command, in the adoption tick for an activation that waited.
	 * The movement abilities apply their impulse here. Simulation code: deterministic, no server-only branches
	 */
	virtual void OnTimelineBegan(int32 StateStartFrame) {}

	// ---- Move events (CP1). The C++ virtuals call the Blueprint events; a C++ child overrides the virtual (and calls Super to keep the event)

	/** An Event key of the running timeline was crossed this frame. Key is the track (its KeyTag, cue, gameplay event), TrackIndex its index in Tracks */
	virtual void OnMoveKey(const FGSTN_TimelineTrack_Event& Key, int32 TrackIndex, int32 TimelineFrame);

	/** Hit groups of the move confirmed a hit this frame (NewHitGroups), AllHitGroups the groups confirmed so far */
	virtual void OnMoveHitConfirmed(int32 NewHitGroups, int32 AllHitGroups);

	/** The timeline ended. Raised from EndAbility while the ability is still active, after its montage was stopped */
	virtual void OnMoveEnded(EGSTN_MoveEndReason Reason);

	/** Every simulated frame the move runs, with bTickMoveInBlueprint */
	virtual void OnMoveFrame(int32 TimelineFrame);

	/** The timeline started (after its montage and OnTimelineBegan). MoveStartFrame is the simulation frame the combat state started it on */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Move", meta = (DisplayName = "On Move Started"))
	void K2_OnMoveStarted(int32 MoveStartFrame);

	/** An Event key was crossed (once, on the frame the timeline reaches it). Compare KeyTag; Key holds the rest of the track */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Move", meta = (DisplayName = "On Move Key"))
	void K2_OnMoveKey(FGameplayTag KeyTag, const FGSTN_TimelineTrack_Event& Key, int32 TimelineFrame);

	/** A hit of this move was confirmed (hit or blocked; a parry or a ricochet is not). NewHitGroups: the groups that confirmed this frame */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Move", meta = (DisplayName = "On Move Hit Confirmed"))
	void K2_OnMoveHitConfirmed(int32 NewHitGroups, int32 AllHitGroups);

	/** The move ended, and why */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Move", meta = (DisplayName = "On Move Ended"))
	void K2_OnMoveEnded(EGSTN_MoveEndReason Reason);

	/** Every simulated frame the move runs, only with bTickMoveInBlueprint. TimelineFrame stands still in hitstop */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Move", meta = (DisplayName = "On Move Frame"))
	void K2_OnMoveFrame(int32 TimelineFrame);

private:
	/** Starts the timeline whose start the combat state shows: sync variables, montage */
	void BeginTimeline(int32 StateStart);

	/** The combat state shows the recoil of a ricochet from this frame on: plays the montage, the cue and the push back, once */
	void BeginRecoil(const FGSTN_RicochetResponse& Response, float StepMs);

	/** The montage the recoil plays is still the one on the synced slot: stops it (a recoil that was cut short) */
	void StopRecoilMontage(bool bWasCancelled);

	/** Drops the waiting state and its loose tag, if it holds them */
	void ClearAwaitingAdoption();

	/** True while the combat state names this ability's class and the start frame it started on */
	bool IsStillTheRunningTimeline() const;

	/** Raises the key, hit and frame events of the frame being simulated, once per simulation frame (start frame included) */
	void RaiseMoveEvents();

	/**
	 * Hold Until Landed (FGSTN_TimelineTrack_HoldUntilLanded, the dropkick): the montage follows the clock's decision of this frame, before the
	 * montage ticks. While the timeline is in the hold range the montage loops its loop part; once the clock jumped to the land frame the montage
	 * jumps to its land part, as a new start (held this tick, drawn first). Stateless: the timeline frame and the synced montage time say it all
	 */
	void UpdateHoldUntilLandedMontage();

	/** Move events are simulation events: none while the ability system restores a frame */
	bool CanRaiseMoveEvents() const;

	/** An Event key was crossed: its cue, its gameplay event and its move actions (CP2), then On Move Key. Stops when the move stops running */
	void RunKey(const FGSTN_TimelineTrack_Event& Key, int32 TrackIndex, int32 TimelineFrame);

	/** The tag window slots (sync variables), by index */
	FSyncGameplayTag& TagWindowTag(int32 Slot) { return Slot == 0 ? TagWindowTag0 : TagWindowTag1; }
	FSyncInteger& TagWindowStart(int32 Slot) { return Slot == 0 ? TagWindowStart0 : TagWindowStart1; }
	FSyncInteger& TagWindowFrames(int32 Slot) { return Slot == 0 ? TagWindowFrames0 : TagWindowFrames1; }

	/** Removes the loose tag of a slot and frees it */
	void CloseTagWindowSlot(int32 Slot);

	/** Closes the windows whose frames are over (every simulated frame of the move), or all of them (the move ended) */
	void CloseTagWindows(bool bAll);

	/** True once the ability found itself in the combat state and started (sync var: rolled back with the instance) */
	UPROPERTY(Transient)
	FSyncBool bStarted;

	/** Timeline registry entry the combat state started this activation with (sync var), INDEX_NONE before the start */
	UPROPERTY(Transient)
	FSyncInteger RunningEntry;

	/**
	 * How the timeline's montage leaves when this ability ends with it still on the slot: the blend-out in seconds (-1 = the montage player's
	 * default) and whether the pose is held. The natural end uses the data's EndBlendOutFrames, a cancel inside the tail Tail.BlendOutFrames,
	 * anything earlier (a hit, a replacing move) the default. Presentation only
	 */
	void ResolveMontageStop(float& OutBlendOutSeconds, bool& bOutHoldPose) const;

	/** The montage BeginTimeline played (kept alive by the ability or the table set), to find it on the slot later. Not state: only read to stop it */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> TimelineMontagePlayed;

	/** The timeline ended by itself and left its montage playing (StopReleasedMontages). Sync variable: a rollback restores whether a release is pending */
	UPROPERTY(Transient)
	FSyncBool bMontageReleased;

	/** The montage of the last release. Not state: only compared with the synced slot's montage before it is stopped, so a stale one stops nothing */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ReleasedMontage;

	/** The timeline ends by itself on this frame and its data lets the montage play on (EndBlendOutFrames < 0): the owner rule above */
	bool ShouldReleaseMontageOnEnd(bool bWasCancelled) const;

	/** The timeline frame the end rules (release, stop blend) compare with the data: the combat clock's, or for a Hold Until Landed move the last evaluated one */
	int32 GetEndRuleFrame(const FGSTN_CombatTimelineData& Data) const;

	/** Combat clock at the start, the origin of GetTimelineFrame */
	UPROPERTY(Transient)
	FSyncInteger StartClock;

	/** Simulation frame the combat state said the timeline started on */
	UPROPERTY(Transient)
	FSyncInteger StartFrame;

	/** True once the recoil of a ricochet was started by this ability (sync var) */
	UPROPERTY(Transient)
	FSyncBool bRecoilPlayed;

	/** Combat clock at the start of the recoil, and its locked length: the recoil ended when the clock moved that far (a recoil cut short stops its montage) */
	UPROPERTY(Transient)
	FSyncInteger RecoilStartClock;

	UPROPERTY(Transient)
	FSyncInteger RecoilLength;

	/** The montage the recoil plays (kept alive by the ability or the table set), to find it on the slot when the ability ends. Not state: only read to stop it */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> RecoilMontagePlayed;

	/** The last timeline frame the move events saw evaluated (sync var): keys in (it, now] are raised. INDEX_NONE before the first */
	UPROPERTY(Transient)
	FSyncInteger EvaluatedFrame;

	/** The hit groups On Move Hit Confirmed already reported for this run (sync var) */
	UPROPERTY(Transient)
	FSyncByte ReportedHitGroups;

	/** Simulation frame the move events were last raised on (sync var): ActivateAbility and the tick of one frame raise them once */
	UPROPERTY(Transient)
	FSyncInteger MoveEventsFrame;

	/** The tag windows of Open Move Tag Window (sync vars): the tag (empty = free slot), the combat clock it opened on, its length */
	UPROPERTY(Transient)
	FSyncGameplayTag TagWindowTag0;

	UPROPERTY(Transient)
	FSyncGameplayTag TagWindowTag1;

	UPROPERTY(Transient)
	FSyncInteger TagWindowStart0;

	UPROPERTY(Transient)
	FSyncInteger TagWindowStart1;

	UPROPERTY(Transient)
	FSyncInteger TagWindowFrames0;

	UPROPERTY(Transient)
	FSyncInteger TagWindowFrames1;

	/** True from an activation that waits for adoption until it is adopted, cancelled or ended */
	UPROPERTY(Transient)
	FSyncBool bAwaitingAdoption;

	/** Simulation frame the wait began on */
	UPROPERTY(Transient)
	FSyncInteger AwaitSinceFrame;

	/** What PreloadTimelineContent loaded (the timeline asset and the montage): nothing else keeps them in memory, and a montage collected on one machine only would desynchronise the played montages */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedContent;
};

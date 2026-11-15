// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NetMontageSimulatorData.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "Animation/AnimMontage.h"
#include "UObject/Object.h"
#include "NetMontageSimulator.generated.h"

class UNpAbilitySystemComponent;
struct FAbilitySystemTimeStep;

/** What FinalizeFrame does to the montage instance before the mesh's animation update (ESyncedMontagePresentation::LandOnSimTime) */
struct FSyncedMontagePresentationStep
{
	/** Position to set on the instance, when bSetPosition (a teleport: no notifies fire for the skipped span) */
	float Position = 0.f;
	/** Play rate to give the instance for this frame's animation update */
	float PlayRate = 1.f;
	bool bSetPosition = false;
	/**
	 * GSTN fork (half-rate montage, 2026-11-15): the time the instance's montage sub-stepper still holds after the coming update. The engine adds
	 * every update's delta to it and only spends it while the play rate is not 0, all of it at once: an update at rate 0 (a hold) keeps its delta
	 * for the next update that moves (see ComputeLandOnSimTimeStep)
	 */
	float CarriedSecondsAfter = 0.f;
};

/**
 * GSTN fork (montage start jitter, 2026-11-15): what ComputePresentedTime reads. Times are montage seconds, Rate montage seconds per real second.
 * The render rate (60 Hz) is not the simulation rate (62.5 Hz), so a rendered frame holds 0, 1 or 2 ticks and the smoothed simulation time of the
 * first frames of a move depended on where in that cycle the press fell (captures: the first frame showed 0.00, the next 0.18 or 1.25 animation
 * frames, dead frames later). The presentation runs its own clock instead: the first rendered frame of a start shows the start, then every frame
 * advances by Rate times its delta, kept inside the last MaxLagSeconds before the latest simulated time (never ahead of the simulation)
 */
struct FSyncedMontagePresentationClockInput
{
	/** The montage time of the latest simulated tick */
	float LatestTime = 0.f;
	/** What the previous rendered frame showed of the same montage; ignored without bHasPrevious */
	float PreviousTime = 0.f;
	bool bHasPrevious = false;
	/** A start (or a restart: a land jump) to show on this frame; < 0 = none. Ignored when it is past LatestTime (a start a rollback undid) */
	float AnchorTime = -1.f;
	/** The montage's nominal rate, NOT zeroed by a pause: a paused simulation holds LatestTime still and the clamp stops the clock there */
	float Rate = 1.f;
	/** The real time of this rendered frame */
	float DeltaSeconds = 0.f;
	/** How far behind the simulation the clock may run: two simulation steps at Rate (one step is too tight for a 60/62.5 cadence) */
	float MaxLagSeconds = 0.f;
};
/**
 * This Class Is Used To play synced Montages in Ability System simulation using Network prediction
 * it also managed ticking and restoring synced notify states and events. and providing the root motion from the montage to mover.
 * finally in finalize frame, this component manages the actual playback and timing of montage on the animation blueprint to ensure smooth playback
 * in sync with the simulation.
 * Has its own synced data that is added to ability simulation synced data.
 */
UCLASS()
class ABILITYSYSTEMSIMULATION_API UNetMontageSimulator : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	
	void SimulationTick(const FAbilitySystemTimeStep& TimeStep,const FMontageSimSyncState& InputState , FMontageSimSyncState& OutputState);
	void RestoreFrame (const FMontageSimSyncState& AuthorityState);
	/**
	 * bSmoothedState (GSTN fork): SyncState is NP's interpolated state of the local simulation (fixed tick smoothing); the presentation clock then
	 * takes the latest simulated time from this simulator's own state. False: SyncState is the latest (no smoothing) or all there is (a proxy)
	 */
	void FinalizeFrame (const FMontageSimSyncState& SyncState, bool bSmoothedState = false);

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	UNpAbilitySystemComponent* GetNpAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	AActor* GetAvatarActor() const;

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	UAnimInstance* GetAnimInstance() const;

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	USkeletalMeshComponent* GetMeshComponent() const;
	
	static float GetMontageBlendOutTime(const UAnimMontage* Montage);

	/**
	 * GSTN fork (montage-exit, 2026-11-15): the blend a stopped synced instance leaves with is the montage's own blend-out (time, curve, mode,
	 * profile). FAnimMontageInstance::GetBlend() is the instance's CURRENT blend, which after the start is the blend-in: stopping with it gave
	 * every synced montage its blend-in time as blend-out (a move with no blend-in was cut in one frame, and the anim graph under it picked its
	 * next clip from that cut pose: the squat after the Overlord jab). BlendOutSeconds >= 0 replaces the time only
	 */
	static FMontageBlendSettings MakeMontageBlendOut(const UAnimMontage& Montage, float BlendOutSeconds = -1.f);
	static bool ShouldFinalizeMontage(const FMontageSimSyncState& SyncState);

	/**
	 * LandOnSimTime: the step that makes an instance at InstancePosition show TargetTime after the coming animation update, which advances
	 * it by AnimDeltaSeconds (the mesh's delta, already scaled by its GlobalAnimRateScale and the actor's time dilation) times its play rate.
	 * Ahead of the target: teleport back and hold (rate 0). Behind: keep the position and pick the rate that covers the gap, up to
	 * MaxRateScale times the nominal rate; a larger gap teleports to where the nominal rate lands on the target. No advance coming
	 * (AnimDeltaSeconds 0, e.g. a hit pause on the mesh): teleport onto the target.
	 * CarriedSeconds (GSTN fork, half-rate montage, 2026-11-15): time the instance's sub-stepper kept from earlier updates at rate 0. The
	 * coming update moves the instance by rate * (CarriedSeconds + AnimDeltaSeconds), not rate * AnimDeltaSeconds: ignoring it drew every move
	 * at 30 Hz for its first frames (start = a hold, the next frame moved two frames, landed on the target, held again: 0 2 2 4 4 6 in the
	 * captures) and for the whole dropkick hold loop. The returned step carries the stepper time left after the update (CarriedSecondsAfter)
	 */
	static FSyncedMontagePresentationStep ComputeLandOnSimTimeStep(float InstancePosition, float TargetTime, float NominalPlayRate,
		float AnimDeltaSeconds, float MaxRateScale = 4.f, float CarriedSeconds = 0.f);

	/**
	 * GSTN fork (montage start jitter, 2026-11-15): the montage time a rendered frame shows (LandOnSimTime then lands the instance on it). An anchor
	 * not past LatestTime: the anchor (the first frame of a start shows the move's first tick, whatever the number of ticks the frame held). No
	 * previous frame: LatestTime. LatestTime behind the previous frame (a loop, a section going back, a correction) or more than two lags ahead
	 * of where the clock gets to (a jump): LatestTime. Otherwise the previous time plus Rate * DeltaSeconds, clamped to
	 * [LatestTime - MaxLagSeconds, LatestTime]. Pure, for FinalizeFrame and the tests
	 */
	static float ComputePresentedTime(const FSyncedMontagePresentationClockInput& Input);

	/** The ability system this simulator serves. Set by UNpAbilitySystemComponent when it creates it; public for tests */
	void SetAbilitySystemComponent(UNpAbilitySystemComponent* InAbilitySystemComponent) { AbilitySystemComponent = InAbilitySystemComponent; }
	
	void PlayMontage(const FAbilityMontagePlayback& MontagePlayback);
	void StopMontage(const FAbilityMontageCancel& MontageCancel, bool bInterrupted = false);
	/**
	 * StopMontage, and how the mesh shows the stop (GSTN, 2026-11-13): the instance blends out over BlendOutSeconds (0 = cut) instead of reusing
	 * its own blend, and with bHoldPose it stops advancing, so nothing of the montage past the stop is drawn. Presentation only: the simulated
	 * state stops exactly as StopMontage does, and a simulated proxy (which never calls this) keeps the default blend. BlendOutSeconds < 0 = StopMontage.
	 */
	void StopMontageWithBlend(const FAbilityMontageCancel& MontageCancel, bool bInterrupted, float BlendOutSeconds, bool bHoldPose);
	/**
	 * Only jumps to section if we are already playing that specific montage
	 */
	void MontageJumpToSection(UAnimMontage* Montage,FName SectionName);
	/**
	 * GSTN fork (hold until landed, 2026-11-15): moves the playing Montage to Time (clamped to its length); nothing when another montage plays or
	 * a frame is being restored. bRestart: a new start of the presentation, as PlayMontage's: the montage holds Time through this tick (the start
	 * tick rule) and the first rendered frame after it shows Time (the dropkick's land clip starts on the touchdown frame). Without it the time is
	 * only moved (the loop wrap of a held move), the tick advances from it
	 */
	void SetMontageTime(UAnimMontage* Montage, float Time, bool bRestart);
	/**
	 * Pause Current Montage if playing
	 */
	void PauseCurrentMontage();
	/**
	 * Resume current montage
	 */
	void ResumeCurrentMontage();
	/**
	 * Set Current Montage Play rate if playing
	 */
	void SetMontagePlayRate(const float& PlayRate);
	
	UFUNCTION(Blueprintable, Category = "MontageSimulator")
	UAnimMontage* GetPlayingMontage(float& OutCurrentTime) const;


	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	static FTransform GetAttachedSocketWorldTransformFromMontage(UAnimMontage* Montage, float MontageTime,
		FTransform MeshRelativeTransform,FTransform ActorTransform,  const USkeletalMeshComponent* MeshComp , FName AttachementSocket,
		const USkeletalMeshComponent* AttachedMeshComponent,FName AttachedMeshSocket);
	
	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	static FTransform GetSocketWorldTransformFromMontage(UAnimMontage* Montage, float MontageTime,
		FTransform MeshRelativeTransform,FTransform ActorTransform,  const USkeletalMeshComponent* MeshComp , FName SocketName);
private:

	void StartSimMontage(const FAbilityMontagePlayback& Montage,FMontageSimSyncState& SyncState) const;
	void AdvanceSimMontage(const float& StepTime ,const FAbilitySystemTimeStep& TimeStep, FMontageSimSyncState& SyncState);
	void CompleteSimMontage(const FAbilitySystemTimeStep& TimeStep,FMontageSimSyncState& SyncState);
	void CancelSimMontage(const FAbilitySystemTimeStep& TimeStep,FMontageSimSyncState& SyncState, const bool& InterruptedByAnother);

	void RestoreMontage(const FMontageSimSyncState& CurrentSyncState,const FMontageSimSyncState& AuthoritySyncState);
	void ForceEndMontage(const FMontageSimSyncState& CurrentSyncState);
	void ForceStartMontage(const FMontageSimSyncState& AuthoritySyncState);
	//Synced Notifies
	void TickSyncedNotifies(const float& MontageStepTime,const FAbilitySystemTimeStep& TimeStep,const float& PreviousTime
		,FMontageSimSyncState& SyncOutput,OUT FTransform& ExtractedRootMotion);
	//void StartSyncedNotifies(FMontageSimSyncState& State);
	void EndSyncedNotifies(const FAbilitySystemTimeStep& TimeStep,FMontageSimSyncState& State);

	void FinalizeMontageFrame(const FMontageSimSyncState& SyncState, bool bSmoothedState);
	/** LandOnSimTime on one instance: the step from ComputeLandOnSimTimeStep with this frame's animation delta, onto TargetTime (the presented time) */
	void LandInstanceOnSimTime(const UAnimInstance& AnimInstance, FAnimMontageInstance& MontageInstance, const FMontageSimSyncState& SyncState,
		float RateMultiplier, float TargetTime);

	/** The presentation clock of this frame for DesiredMontage (ComputePresentedTime with this simulator's anchor and previous frame), and its bookkeeping */
	float PresentMontageTime(const UAnimInstance& AnimInstance, UAnimMontage* DesiredMontage, const FMontageSimSyncState& SyncState, bool bSmoothedState,
		float RateMultiplier);

	/** A start the next rendered frame shows (PlayMontage, SetMontageTime with bRestart); a re-simulated start already shown is not recorded again */
	void RecordPresentationAnchor(UAnimMontage* Montage, float Time);

	/** Forgets the presentation clock (no montage drawn) */
	void ResetPresentationClock();
	
	FAbilitySystemTimeStep LatestTimeStep;

	/**
	 * A montage was started (PlayMontage) since the last SimulationTick: that tick holds it at its start time instead of advancing it
	 * (unless UNpAbilitySystemComponent::bAdvanceSyncedMontageOnStartTick). Only lives inside one simulation tick (abilities tick before
	 * the montage), cleared by every SimulationTick and RestoreFrame, so it is never part of the rolled back state.
	 */
	bool bStartedSinceLastTick = false;

	// This Is Used To Step Montage to support looping. doesn't really do anything else!
	FAnimMontageInstance MontageAdvanceInstance;

	

	UPROPERTY()
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> LastPlayingMontage = nullptr;

	/** The stop presentation StopMontageWithBlend asked for, applied by FinalizeMontageFrame when it stops that montage's instance. Not state */
	struct FPendingStopPresentation
	{
		TWeakObjectPtr<UAnimMontage> Montage;
		float BlendOutSeconds = -1.f;
		bool bHoldPose = false;
	};
	FPendingStopPresentation PendingStop;

	/**
	 * The presentation clock (ComputePresentedTime). Not state: presentation only, never rolled back. The anchor is a start recorded by the
	 * simulation since the last rendered frame, keyed by the simulation frame it happened on; the consumed ones keep a re-simulated start (a
	 * rollback over the first frames of a move replays the same start on the same frame) from pulling the drawn montage back to its start
	 */
	struct FPresentationAnchor
	{
		TWeakObjectPtr<UAnimMontage> Montage;
		float Time = 0.f;
		int32 Frame = INDEX_NONE;
	};
	FPresentationAnchor PendingAnchor;
	bool bAnchorPending = false;
	FPresentationAnchor ConsumedAnchors[4];
	int32 NextConsumedAnchor = 0;
	TWeakObjectPtr<UAnimMontage> PresentedMontage;
	float PresentedTime = 0.f;
	/**
	 * GSTN fork (half-rate montage): the sub-stepper time the presented instance still holds (ComputeLandOnSimTimeStep's CarriedSeconds), and
	 * the instance it belongs to (FAnimMontageInstance::GetInstanceID; a new instance starts with none). Presentation only, not state.
	 * Estimated, not read (the engine keeps it private): the instance is assumed to update once per world tick by this frame's animation delta
	 */
	float CarriedStepperSeconds = 0.f;
	int32 CarriedStepperInstanceId = INDEX_NONE;

	UPROPERTY()
	FMontageSimSyncState SimulationState;
	
	friend class UNpAbilitySystemComponent;
};

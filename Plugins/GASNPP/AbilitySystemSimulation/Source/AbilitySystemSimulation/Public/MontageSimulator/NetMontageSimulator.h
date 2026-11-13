// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NetMontageSimulatorData.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
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
	void FinalizeFrame (const FMontageSimSyncState& SyncState);

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	UNpAbilitySystemComponent* GetNpAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	AActor* GetAvatarActor() const;

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	UAnimInstance* GetAnimInstance() const;

	UFUNCTION(BlueprintPure, Category = "MontageSimulator")
	USkeletalMeshComponent* GetMeshComponent() const;
	
	static float GetMontageBlendOutTime(const UAnimMontage* Montage);
	static bool ShouldFinalizeMontage(const FMontageSimSyncState& SyncState);

	/**
	 * LandOnSimTime: the step that makes an instance at InstancePosition show TargetTime after the coming animation update, which advances
	 * it by AnimDeltaSeconds (the mesh's delta, already scaled by its GlobalAnimRateScale and the actor's time dilation) times its play rate.
	 * Ahead of the target: teleport back and hold (rate 0). Behind: keep the position and pick the rate that covers the gap, up to
	 * MaxRateScale times the nominal rate; a larger gap teleports to where the nominal rate lands on the target. No advance coming
	 * (AnimDeltaSeconds 0, e.g. a hit pause on the mesh): teleport onto the target.
	 */
	static FSyncedMontagePresentationStep ComputeLandOnSimTimeStep(float InstancePosition, float TargetTime, float NominalPlayRate,
		float AnimDeltaSeconds, float MaxRateScale = 4.f);

	/** The ability system this simulator serves. Set by UNpAbilitySystemComponent when it creates it; public for tests */
	void SetAbilitySystemComponent(UNpAbilitySystemComponent* InAbilitySystemComponent) { AbilitySystemComponent = InAbilitySystemComponent; }
	
	void PlayMontage(const FAbilityMontagePlayback& MontagePlayback);
	void StopMontage(const FAbilityMontageCancel& MontageCancel, bool bInterrupted = false);
	/**
	 * Only jumps to section if we are already playing that specific montage
	 */
	void MontageJumpToSection(UAnimMontage* Montage,FName SectionName);
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

	void FinalizeMontageFrame(const FMontageSimSyncState& SyncState);
	/** LandOnSimTime on one instance: the step from ComputeLandOnSimTimeStep with this frame's animation delta */
	void LandInstanceOnSimTime(const UAnimInstance& AnimInstance, FAnimMontageInstance& MontageInstance, const FMontageSimSyncState& SyncState,
		float RateMultiplier) const;
	
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

	UPROPERTY()
	FMontageSimSyncState SimulationState;
	
	friend class UNpAbilitySystemComponent;
};

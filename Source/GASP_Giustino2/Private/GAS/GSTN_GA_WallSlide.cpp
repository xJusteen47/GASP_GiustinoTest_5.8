// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_GA_WallSlide.h"
#include "GAS/GSTN_AbilityTags.h"
#include "Mover/GSTN_MoverSimStateTracker.h"

UGSTN_GA_WallSlide::UGSTN_GA_WallSlide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetAssetTags(FGameplayTagContainer(GSTNAbilityTags::Ability_Movement_WallSlide));
	ActivationOwnedTags.AddTag(GSTNAbilityTags::State_Movement_WallSliding);
	ActivationBlockedTags.AddTag(GSTNAbilityTags::State_Movement_WallSlideBlocked);

	WallModeNames.Add(TEXT("WallSliding"));
	WallModeNames.Add(TEXT("WallSlidingV2"));
	WallModeNames.Add(TEXT("WallClimbingV2"));
}

void UGSTN_GA_WallSlide::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Instanced per actor: sync vars keep their value between activations
	FramesSinceActivation.Value = 0;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGSTN_GA_WallSlide::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);

	if (!IsActive())
	{
		return;
	}

	++FramesSinceActivation.Value;
	if (FramesSinceActivation.Value <= EntryGraceFrames || IsMoverInWallMode())
	{
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

bool UGSTN_GA_WallSlide::IsMoverInWallMode() const
{
	// The simulation state, not the Mover component's game frame state: right while re-simulating too
	const UGSTN_MoverSimStateTracker* MoverState = GetMoverSimState(GetCurrentActorInfo());
	return MoverState && WallModeNames.Contains(MoverState->GetMovementModeName());
}

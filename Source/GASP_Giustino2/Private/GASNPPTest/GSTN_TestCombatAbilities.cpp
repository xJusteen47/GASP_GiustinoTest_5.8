// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestCombatAbilities.h"
#include "Input/GSTN_CombatLockTags.h"
#include "NativeGameplayTags.h"
#include "Simulation/GSTN_CommandSourceInterface.h"

namespace
{
	// Declared here, not in a header: only these abilities grant them, and the harness reads them by name (Test.* is
	// logged by UGSTN_GASNPPTestComponent). They are not part of the project's public API.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Attacking, "Test.State.Attacking",
		"Owned while the test attack ability is active: the harness uses it to see that an attack command started");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Stance, "Test.State.Stance",
		"Owned while the test stance ability is active (held button): the harness uses it to see that the stance command started");
}

UGSTN_GA_TestAttack::UGSTN_GA_TestAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Attacking);
}

void UGSTN_GA_TestAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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

void UGSTN_GA_TestAttack::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);

	if (!IsActive())
	{
		return;
	}

	++FramesSinceActivation.Value;
	if (FramesSinceActivation.Value >= ActiveFrames)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

UGSTN_GA_TestStance::UGSTN_GA_TestStance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Stance);
	// The point of the stance: while it is held the attack commands are locked
	ActivationOwnedTags.AddTag(GSTNCombatLockTags::Attack);
}

void UGSTN_GA_TestStance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The activation context is valid on this frame only: keep the button in a sync var
	TriggerButton.Value = INDEX_NONE;
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (const IGSTN_CommandSourceInterface* CommandSource = IGSTN_CommandSourceInterface::Find(Avatar))
	{
		TriggerButton.Value = CommandSource->GetActivationContext().Button;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGSTN_GA_TestStance::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);

	if (!IsActive())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const IGSTN_CommandSourceInterface* CommandSource = IGSTN_CommandSourceInterface::Find(Avatar);
	if (CommandSource && TriggerButton.Value != INDEX_NONE && CommandSource->IsCommandButtonHeld(TriggerButton.Value))
	{
		return;
	}

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
}

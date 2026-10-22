// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_GA_Base.h"

#include "AbilitySystemComponent.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_InputHostUtils.h"

UGSTN_GA_Base::UGSTN_GA_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CommandIndex.Value = INDEX_NONE;
	PressFrame.Value = INDEX_NONE;
	TriggerButton.Value = INDEX_NONE;
	ActivationSource.Value = static_cast<uint8>(EGSTN_ActivationSource::Other);
}

UGSTN_CombatSimComponent* UGSTN_GA_Base::FindCombatComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar ? Avatar->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
}

UGSTN_CombatSimComponent* UGSTN_GA_Base::GetCombatComponent() const
{
	UGSTN_CombatSimComponent* Combat = CachedCombat.Get();
	if (!Combat)
	{
		Combat = FindCombatComponent(GetCurrentActorInfo());
		CachedCombat = Combat;
	}
	return Combat;
}

void UGSTN_GA_Base::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	CachedCombat = FindCombatComponent(ActorInfo);
}

FGSTN_ActivationContext UGSTN_GA_Base::GetActivationContext() const
{
	FGSTN_ActivationContext Context;
	Context.Source = static_cast<EGSTN_ActivationSource>(ActivationSource.Value);
	Context.PressFrame = PressFrame.Value;
	Context.Button = TriggerButton.Value;

	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	if (Combat && CommandIndex.Value != INDEX_NONE)
	{
		const FGSTN_CommandTable& Table = Combat->GetCommandTable();
		if (Table.Commands.IsValidIndex(CommandIndex.Value))
		{
			Context.Command = Table.Commands[CommandIndex.Value].Tag;
		}
	}
	return Context;
}

int32 UGSTN_GA_Base::GetCombatClock() const
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	return Combat ? Combat->GetCombatClock() : INDEX_NONE;
}

FRotator UGSTN_GA_Base::GetAimRotation() const
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	return Combat ? Combat->GetAimRotation() : FRotator::ZeroRotator;
}

void UGSTN_GA_Base::WaitCombatFrames(int32 Frames)
{
	WaitStartClock.Value = GetCombatClock();
	WaitFrames.Value = FMath::Max(Frames, 1);
}

void UGSTN_GA_Base::CancelCombatWait()
{
	WaitFrames.Value = 0;
}

bool UGSTN_GA_Base::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UGSTN_CombatSimComponent* Combat = FindCombatComponent(ActorInfo);
	return !(Combat && FGSTN_GARules::BlocksActivation(InterruptPolicy, Combat->IsInTimeline(), FGSTN_InputHostUtils::IsCommandActivation()));
}

void UGSTN_GA_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The instance is reused between activations: rewrite every variable
	CommandIndex.Value = INDEX_NONE;
	PressFrame.Value = INDEX_NONE;
	TriggerButton.Value = INDEX_NONE;
	ActivationSource.Value = static_cast<uint8>(EGSTN_ActivationSource::Other);
	WaitFrames.Value = 0;

	const bool bCommandActivation = FGSTN_InputHostUtils::IsCommandActivation();
	UGSTN_CombatSimComponent* Combat = FindCombatComponent(ActorInfo);
	CachedCombat = Combat;
	if (Combat)
	{
		// Only a command has a context; a trigger activating on the same frame as an unrelated request must not read that one
		if (bCommandActivation)
		{
			const FGSTN_ActivationContext Context = Combat->GetActivationContext();
			CommandIndex.Value = Combat->GetCommandTable().FindCommand(Context.Command);
			PressFrame.Value = Context.PressFrame;
			TriggerButton.Value = Context.Button;
			ActivationSource.Value = static_cast<uint8>(Context.IsValid() ? Context.Source : EGSTN_ActivationSource::Other);
		}

		if (FGSTN_GARules::ShouldInterrupt(InterruptPolicy, Combat->IsInTimeline(), bCommandActivation) && !InterruptTags.IsEmpty())
		{
			if (UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
			{
				AbilitySystem->CancelAbilities(&InterruptTags, nullptr, this);
			}
		}
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGSTN_GA_Base::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	WaitFrames.Value = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGSTN_GA_Base::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);

	if (!IsActive())
	{
		return;
	}

	if (bEndOnTriggerRelease && TriggerButton.Value != INDEX_NONE)
	{
		const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
		const bool bHeld = Combat && Combat->IsCommandButtonHeld(TriggerButton.Value);
		if (FGSTN_GARules::EndsOnRelease(bEndOnTriggerRelease, TriggerButton.Value, bHeld))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
			return;
		}
	}

	if (WaitFrames.Value > 0 && FGSTN_CombatClock::Elapsed(GetCombatClock(), WaitStartClock.Value) >= WaitFrames.Value)
	{
		WaitFrames.Value = 0;
		OnCombatFramesElapsed();
	}
}

void UGSTN_GA_Base::OnCombatFramesElapsed()
{
	K2_OnCombatFramesElapsed();
}

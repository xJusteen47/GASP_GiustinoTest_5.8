// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestProbeAbilities.h"
#include "GASNPPTest/GSTN_GASNPPTestComponent.h"
#include "GASNPPTest/GSTN_TestEffects.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "InputAction.h"

UGSTN_ProbeAbilityBase::UGSTN_ProbeAbilityBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGSTN_GASNPPTestComponent* UGSTN_ProbeAbilityBase::FindTestComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar ? Avatar->FindComponentByClass<UGSTN_GASNPPTestComponent>() : nullptr;
}

bool UGSTN_ProbeAbilityBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (UGSTN_GASNPPTestComponent* TestComponent = FindTestComponent(ActorInfo))
	{
		const UNpAbilitySystemComponent* NpASC = Cast<UNpAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
		TestComponent->RecordProbeAttempt(ProbeName, NpASC && NpASC->GetIsRestoringFrame());
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGSTN_ProbeAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (UGSTN_GASNPPTestComponent* TestComponent = FindTestComponent(ActorInfo))
	{
		const UNpAbilitySystemComponent* NpASC = Cast<UNpAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
		TestComponent->RecordProbeActivation(ProbeName, NpASC && NpASC->IsReSimulating(), NpASC && NpASC->GetIsRestoringFrame());
	}
}

void UGSTN_ProbeAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	const bool bWasActive = IsActive();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (bWasActive && !IsRestoringFrame())
	{
		if (UGSTN_GASNPPTestComponent* TestComponent = FindTestComponent(ActorInfo))
		{
			TestComponent->RecordProbeEnd(ProbeName);
		}
	}
}

void UGSTN_ProbeAbilityBase::ApplyProbeEffect(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, TSubclassOf<UGameplayEffect> Effect)
{
	if (Effect && !IsRestoringFrame())
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, Effect.GetDefaultObject(), GetAbilityLevel(Handle, ActorInfo));
	}
}

UGSTN_GA_ProbeEventRaiser::UGSTN_GA_ProbeEventRaiser(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProbeName = TEXT("EventRaiser");
	AddActivationInput(TEXT("/Game/GASNPPTest/Input/IA_GASNPP_Buff.IA_GASNPP_Buff"), ETriggerEvent::Completed);
}

void UGSTN_GA_ProbeEventRaiser::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Inside the simulation tick of the ability system: the triggered ability activates in this frame or in the next one,
	// depending on its place in the spec array (what scenario_spike_triggers measures)
	if (!IsRestoringFrame() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = GSTNTestTags::Probe_Event.GetTag();
		Payload.Instigator = ActorInfo->AvatarActor.Get();
		Payload.Target = ActorInfo->AvatarActor.Get();
		ActorInfo->AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGSTN_GA_ProbeTagRaiser::UGSTN_GA_ProbeTagRaiser(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProbeName = TEXT("TagRaiser");
	AddActivationInput(TEXT("/Game/GASNPPTest/Input/IA_GASNPP_Buff.IA_GASNPP_Buff"), ETriggerEvent::Completed);
}

void UGSTN_GA_ProbeTagRaiser::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyProbeEffect(Handle, ActorInfo, ActivationInfo, UGSTN_GE_ProbeTagGrant::StaticClass());
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGSTN_ProbeTargetBase::UGSTN_ProbeTargetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGSTN_ProbeTargetBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Instanced per actor: sync vars keep their value between activations
	FramesSinceActivation.Value = 0;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyProbeEffect(Handle, ActorInfo, ActivationInfo, CountEffect);
}

void UGSTN_ProbeTargetBase::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
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

UGSTN_GA_ProbeEventTarget::UGSTN_GA_ProbeEventTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProbeName = TEXT("EventTarget");
	CountEffect = UGSTN_GE_ProbeCountEvent::StaticClass();

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GSTNTestTags::Probe_Event.GetTag();
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

UGSTN_GA_ProbeTagTarget::UGSTN_GA_ProbeTagTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProbeName = TEXT("TagTarget");
	CountEffect = UGSTN_GE_ProbeCountTag::StaticClass();

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GSTNTestTags::Probe_TagTrigger.GetTag();
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(Trigger);
}

UGSTN_GA_ProbeGrantBase::UGSTN_GA_ProbeGrantBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGSTN_GA_ProbeGrantServerOnlyBase::UGSTN_GA_ProbeGrantServerOnlyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

const TArray<TSubclassOf<UGameplayAbility>>& GSTNProbe::GetGrantClasses()
{
	static const TArray<TSubclassOf<UGameplayAbility>> Classes = {
		UGSTN_GA_ProbeGrant00::StaticClass(), UGSTN_GA_ProbeGrant01::StaticClass(), UGSTN_GA_ProbeGrant02::StaticClass(), UGSTN_GA_ProbeGrant03::StaticClass(),
		UGSTN_GA_ProbeGrant04::StaticClass(), UGSTN_GA_ProbeGrant05::StaticClass(), UGSTN_GA_ProbeGrant06::StaticClass(), UGSTN_GA_ProbeGrant07::StaticClass(),
		UGSTN_GA_ProbeGrant08::StaticClass(), UGSTN_GA_ProbeGrant09::StaticClass(), UGSTN_GA_ProbeGrant10::StaticClass(), UGSTN_GA_ProbeGrant11::StaticClass(),
		UGSTN_GA_ProbeGrant12::StaticClass(), UGSTN_GA_ProbeGrant13::StaticClass(), UGSTN_GA_ProbeGrant14::StaticClass(), UGSTN_GA_ProbeGrant15::StaticClass(),
		UGSTN_GA_ProbeGrant16::StaticClass(), UGSTN_GA_ProbeGrant17::StaticClass(), UGSTN_GA_ProbeGrant18::StaticClass(), UGSTN_GA_ProbeGrant19::StaticClass(),
		UGSTN_GA_ProbeGrant20::StaticClass(), UGSTN_GA_ProbeGrant21::StaticClass(), UGSTN_GA_ProbeGrant22::StaticClass(), UGSTN_GA_ProbeGrant23::StaticClass(),
		UGSTN_GA_ProbeGrant24::StaticClass(), UGSTN_GA_ProbeGrant25::StaticClass(), UGSTN_GA_ProbeGrant26::StaticClass(), UGSTN_GA_ProbeGrant27::StaticClass(),
		UGSTN_GA_ProbeGrant28::StaticClass(), UGSTN_GA_ProbeGrant29::StaticClass(), UGSTN_GA_ProbeGrant30::StaticClass(), UGSTN_GA_ProbeGrant31::StaticClass(),
		UGSTN_GA_ProbeGrant32::StaticClass(), UGSTN_GA_ProbeGrant33::StaticClass(), UGSTN_GA_ProbeGrant34::StaticClass(), UGSTN_GA_ProbeGrant35::StaticClass(),
		UGSTN_GA_ProbeGrant36::StaticClass(), UGSTN_GA_ProbeGrant37::StaticClass(), UGSTN_GA_ProbeGrant38::StaticClass(), UGSTN_GA_ProbeGrant39::StaticClass(),
	};
	return Classes;
}

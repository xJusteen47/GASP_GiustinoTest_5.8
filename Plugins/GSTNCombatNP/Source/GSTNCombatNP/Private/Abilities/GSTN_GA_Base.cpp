// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_GA_Base.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "AbilitySystemComponent.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_InputHostUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNGABase, Log, All);

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
	if (Frames < 1)
	{
		UE_LOG(LogGSTNGABase, Warning, TEXT("%s: WaitCombatFrames(%d) waits at least one frame"), *GetName(), Frames);
	}
	// Before the first simulated frame the clock is INDEX_NONE and a wait started then would never elapse: that wait starts at 0
	WaitStartClock.Value = FMath::Max(GetCombatClock(), 0);
	WaitFrames.Value = FMath::Max(Frames, 1);
}

void UGSTN_GA_Base::CancelCombatWait()
{
	WaitFrames.Value = 0;
}

bool UGSTN_GA_Base::StartCombatMove(TSubclassOf<UGSTN_GA_BaseCombat> Move)
{
	UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	const UGSTN_CombatTableSet* Set = Combat ? Combat->GetActiveTableSet() : nullptr;
	if (!Move || !Set || !AbilitySystem || !IsActive())
	{
		return false;
	}
	// Everything that can refuse is checked before the state is written: a refusal must leave the running move as it was
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(Move);
	const int32 Entry = Set->FindMoveEntry(Move);
	if (!Spec || Entry == INDEX_NONE || UGSTN_CombatTableSet::IsHostClass(Move))
	{
		UE_LOG(LogGSTNGABase, Warning, TEXT("%s: Start Combat Move %s refused: %s"), *GetName(), *GetNameSafe(Move.Get()),
			!Spec ? TEXT("the ability is not granted") : Entry == INDEX_NONE ? TEXT("the move is not in the registry (bind it, or add it to Adoptable Timelines)")
			: TEXT("a host plays no timeline of its own: activate it instead"));
		return false;
	}
	const FGameplayAbilitySpecHandle Handle = Spec->Handle;

	bool bCommitDue = false;
	if (Combat->StartTimelineFromAbility(Entry, bCommitDue) != EGSTN_TimelineStartRefusal::None)
	{
		return false;
	}

	// Activated as the model's own request (the ActivateEntry path of the pre-tick, which ran already this frame): the ability finds its timeline
	// running from this frame and begins it in its activation. A running instance of the same class is retriggered
	bool bActivated = false;
	{
		const FGSTN_CommandActivationScope CommandScope(Handle);
		bActivated = AbilitySystem->TryActivateAbility(Handle);
	}
	if (!bActivated)
	{
		// The state names a timeline whose ability is not active: the combat tick of the next frame ends it (step 2)
		UE_LOG(LogGSTNGABase, Warning, TEXT("%s: Start Combat Move %s: the ability refused to activate (tags, cost, cooldown)"), *GetName(), *GetNameSafe(Move.Get()));
		return false;
	}
	if (bCommitDue)
	{
		if (const FGameplayAbilitySpec* Started = AbilitySystem->FindAbilitySpecFromHandle(Handle))
		{
			UGSTN_GA_BaseCombat::CommitOrCancel(*AbilitySystem, *Started);
		}
	}
	return true;
}

bool UGSTN_GA_Base::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UGSTN_CombatSimComponent* Combat = FindCombatComponent(ActorInfo);
	return !(Combat && FGSTN_GARules::BlocksActivation(InterruptPolicy, Combat->IsInTimeline(), FGSTN_InputHostUtils::IsCommandActivation(Handle)));
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

	// A new action ends the rest of a finished move's montage (owner rule 2026-11-15, UGSTN_GA_BaseCombat::StopReleasedMontages)
	if (UAbilitySystemComponent* ReleasingSystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		UGSTN_GA_BaseCombat::StopReleasedMontages(*ReleasingSystem);
	}

	const bool bCommandActivation = FGSTN_InputHostUtils::IsCommandActivation(Handle);
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

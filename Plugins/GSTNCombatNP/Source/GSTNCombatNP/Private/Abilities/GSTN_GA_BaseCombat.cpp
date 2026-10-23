// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_GA_BaseCombat.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GSTN_CombatTags.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_InputHostUtils.h"
#include "Timeline/GSTN_CombatTimeline.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNBaseCombat, Log, All);

UGSTN_GA_BaseCombat::UGSTN_GA_BaseCombat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(GSTNCombatTags::Ability_Timeline);

	// The cost is committed by the combat tick, and a cancel window into the same ability starts it again
	bRetriggerInstancedAbility = true;

	// Another timeline ability that interrupts this kind (InterruptCurrent) cancels the running ones of the kind
	InterruptTags.AddTag(GSTNCombatTags::Ability_Timeline);

	bStarted.Value = 0;
	StartClock.Value = INDEX_NONE;
	StartFrame.Value = INDEX_NONE;
	bAwaitingAdoption.Value = 0;
	AwaitSinceFrame.Value = INDEX_NONE;
}

const FGSTN_CombatTimelineData& UGSTN_GA_BaseCombat::GetTimelineData() const
{
	if (!TimelineAsset.IsNull())
	{
		if (const UGSTN_CombatTimeline* Asset = LoadedAsset ? LoadedAsset.Get() : TimelineAsset.Get())
		{
			return Asset->Data;
		}
	}
	return Timeline;
}

void UGSTN_GA_BaseCombat::PreloadTimelineContent()
{
	if (!TimelineAsset.IsNull())
	{
		LoadedAsset = TimelineAsset.LoadSynchronous();
	}
	// The synced montage is played from a plain pointer: it has to be in memory before the first frame of the timeline
	const FGSTN_CombatTimelineData& Data = GetTimelineData();
	if (!Data.Montage.IsNull())
	{
		Data.Montage.LoadSynchronous();
	}
}

void UGSTN_GA_BaseCombat::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	PreloadTimelineContent();
}

bool UGSTN_GA_BaseCombat::IsStillTheRunningTimeline() const
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	int32 StateStart = INDEX_NONE;
	return Combat && Combat->IsTimelineRunning(GetClass(), StateStart) && StateStart == StartFrame.Value;
}

void UGSTN_GA_BaseCombat::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// A retrigger of an instance that still waits: its tag goes before the variables are rewritten
	ClearAwaitingAdoption();

	// Captures the activation context and applies the interrupt policy
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bStarted.Value = 0;
	StartClock.Value = INDEX_NONE;
	StartFrame.Value = INDEX_NONE;
	AwaitSinceFrame.Value = INDEX_NONE;

	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	if (!Combat || !AbilitySystem)
	{
		UE_LOG(LogGSTNBaseCombat, Log, TEXT("%s: no combat component or ability system on the avatar: ended"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// A command: the combat tick wrote this timeline's start into its state in this frame, before the ability system ticked
	int32 StateStart = INDEX_NONE;
	if (Combat->IsTimelineRunning(GetClass(), StateStart) && StateStart == Combat->GetSimulationFrame())
	{
		BeginTimeline(StateStart);
		return;
	}

	// Anything else (a trigger, an effect, another ability, AI): wait for the combat tick of the next frame to adopt this
	// activation (combat-m3 7.4). The loose tag is what lets that tick skip the scan on every frame nothing waits
	bAwaitingAdoption.Value = 1;
	AwaitSinceFrame.Value = Combat->GetSimulationFrame();
	AbilitySystem->AddLooseGameplayTag(GSTNCombatTags::Ability_AwaitingAdoption);
}

void UGSTN_GA_BaseCombat::BeginTimeline(int32 StateStart)
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	bStarted.Value = 1;
	StartFrame.Value = StateStart;
	StartClock.Value = Combat ? Combat->GetCombatClock() : INDEX_NONE;

	const FGSTN_CombatTimelineData& Data = GetTimelineData();
	UAnimMontage* Montage = Data.Montage.Get();
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	if (Montage && AbilitySystem)
	{
		AbilitySystem->PlaySyncedMontage(Montage, 0.f, Data.MontagePlayRate, Data.MontageSection, 1.f);
	}
}

void UGSTN_GA_BaseCombat::ClearAwaitingAdoption()
{
	if (bAwaitingAdoption.Value != 0)
	{
		bAwaitingAdoption.Value = 0;
		if (UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent())
		{
			AbilitySystem->RemoveLooseGameplayTag(GSTNCombatTags::Ability_AwaitingAdoption);
		}
	}
}

void UGSTN_GA_BaseCombat::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);

	if (!IsActive())
	{
		return;
	}

	if (bAwaitingAdoption.Value != 0)
	{
		const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
		const int32 Frame = Combat ? Combat->GetSimulationFrame() : INDEX_NONE;
		int32 StateStart = INDEX_NONE;
		if (Combat && Combat->IsTimelineRunning(GetClass(), StateStart) && StateStart == Frame)
		{
			// The combat tick adopted this activation in this frame: from here it is a timeline like any other
			ClearAwaitingAdoption();
			SetActivationSource(EGSTN_ActivationSource::Adopted);
			BeginTimeline(StateStart);
		}
		else if (Frame - AwaitSinceFrame.Value > MaxAdoptionWaitFrames)
		{
			// Another ability was adopted in its place, or the combat model does not know this class
			UE_LOG(LogGSTNBaseCombat, Log, TEXT("%s: not adopted within %d frames (waiting since %d, frame %d): cancelled"), *GetName(), MaxAdoptionWaitFrames, AwaitSinceFrame.Value, Frame);
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		}
		return;
	}

	if (bStarted.Value == 0)
	{
		return;
	}

	// The combat state no longer names this timeline: it ended, was replaced or was cancelled
	if (!IsStillTheRunningTimeline())
	{
		const UGSTN_CombatSimComponent* CombatNow = GetCombatComponent();
		int32 StateStartNow = INDEX_NONE;
		const bool bRunningNow = CombatNow && CombatNow->IsTimelineRunning(GetClass(), StateStartNow);
		UE_LOG(LogGSTNBaseCombat, Log, TEXT("%s: the combat state no longer names this timeline: ended (running %d, state start %d, own start %d, frame %d, step frame %d)"),
			*GetName(), bRunningNow, StateStartNow, StartFrame.Value, CombatNow ? CombatNow->GetSimulationFrame() : INDEX_NONE, TimeStep.ServerFrame);
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UGSTN_GA_BaseCombat::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearAwaitingAdoption();
	if (bStarted.Value != 0)
	{
		// Stop the montage only if it is still this ability's: another timeline may have started its own in the same frame
		const FGSTN_CombatTimelineData& Data = GetTimelineData();
		UAnimMontage* Montage = Data.Montage.Get();
		UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
		float Time = 0.f;
		if (Montage && AbilitySystem && AbilitySystem->GetPlayingSyncedMontage(Time) == Montage)
		{
			AbilitySystem->StopSyncedMontage(Montage, bWasCancelled);
		}
		bStarted.Value = 0;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

int32 UGSTN_GA_BaseCombat::GetTimelineFrame() const
{
	return bStarted.Value != 0 ? FGSTN_CombatClock::Elapsed(GetCombatClock(), StartClock.Value) : 0;
}

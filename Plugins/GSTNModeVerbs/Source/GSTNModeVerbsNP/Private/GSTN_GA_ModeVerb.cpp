// Mode verbs, Network Prediction half: the verb ability (game-modes 2.4).

#include "GSTN_GA_ModeVerb.h"

#include "GameFramework/Actor.h"
#include "GSTN_ModeVerbTags.h"
#include "GSTN_VerbSubsystem.h"
#include "Simulation/GSTN_CombatSimComponent.h"

UGSTN_GA_ModeVerb::UGSTN_GA_ModeVerb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One tag stops every verb (CancelAbilities by tag); the specific verb tag is the designer's, set with VerbSpec
	AbilityTags.AddTag(GSTNModeVerbTags::Ability_Mode);
	// A channel the player keeps up is cancelled by releasing the button that started it. With HoldFrames 0 the ability ends in its first frame anyway
	bEndOnTriggerRelease = true;
	HoldStartClock.Value = INDEX_NONE;
	VerbState.Value = Idle;
}

float UGSTN_GA_ModeVerb::GetHoldProgress() const
{
	if (VerbState.Value == Confirmed)
	{
		return 1.f;
	}
	if (VerbState.Value != Holding || VerbSpec.HoldFrames <= 0)
	{
		return 0.f;
	}
	const int32 Elapsed = GetCombatClock() - HoldStartClock.Value;
	return FMath::Clamp(static_cast<float>(Elapsed) / static_cast<float>(VerbSpec.HoldFrames), 0.f, 1.f);
}

bool UGSTN_GA_ModeVerb::IsVerbConfirmed() const
{
	return VerbState.Value == Confirmed;
}

void UGSTN_GA_ModeVerb::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The parent first: it captures the activation context the rest may read
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	VerbState.Value = Holding;
	HoldStartClock.Value = FMath::Max(GetCombatClock(), 0);

	if (VerbSpec.HoldFrames > 0)
	{
		WaitCombatFrames(VerbSpec.HoldFrames);
		return;
	}
	ConfirmVerb();
}

void UGSTN_GA_ModeVerb::OnCombatFramesElapsed()
{
	ConfirmVerb();
}

void UGSTN_GA_ModeVerb::ConfirmVerb()
{
	if (VerbState.Value != Holding)
	{
		return;
	}
	VerbState.Value = Confirmed;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (UGSTN_VerbSubsystem* Verbs = UGSTN_VerbSubsystem::Get(GetWorld()))
	{
		FGSTN_VerbConfirmRequest Request;
		Request.Spec = VerbSpec;
		Request.Payload.VerbTag = VerbSpec.VerbTag;
		Request.Payload.Instigator = Avatar;
		Request.Payload.Magnitude = VerbMagnitude;
		Request.InstigatorId = Avatar ? Avatar->GetUniqueID() : 0;

		// The frame is part of the once-only key, so it must be the simulation's and the same in a re-simulation; the subsystem's last tick plus one
		// is the fallback before the combat model has a frame
		const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
		const int32 Frame = Combat ? Combat->GetSimulationFrame() : INDEX_NONE;
		Request.Payload.ConfirmFrame = Frame != INDEX_NONE ? Frame : Verbs->GetSimFrame() + 1;

		// TargetKey stays 0 until the interact seam stamps the focus id into the activation context (interaction.md 3.3): the host then resolves the
		// nearest prop in reach that takes this verb
		if (VerbSpec.TargetSource == EGSTN_VerbTargetSource::AimPoint)
		{
			Request.Aim = GetAimRotation();
			Request.bHasAim = true;
		}
		// Unconditional, on every machine and in every re-simulation: the queue refuses a repeat and only the host's flush publishes
		Verbs->QueueConfirm(Request);
	}

	K2_OnVerbConfirmed();
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UGSTN_GA_ModeVerb::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Ended before the confirmation: the one cancel. Once, because the state moves out of Holding
	if (VerbState.Value == Holding)
	{
		VerbState.Value = Cancelled;
		K2_OnVerbCancelled();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

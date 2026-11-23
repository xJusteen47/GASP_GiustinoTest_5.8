// Defence, Network Prediction half.

#include "Sim/GSTN_DIWiring.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_DefenceRules.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Math/GSTN_DefenceMath.h"
#include "Math/GSTN_DIRulesMath.h"
#include "Pipeline/GSTN_DefenceHitOutcome.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceSnapshotExt.h"
#include "Simulation/GSTN_CombatTickExtension.h"

uint8 FGSTN_DIWiring::MakeHitFlags(const FGSTN_HitContextView& View)
{
	const FGSTN_DefenceSnapshotExt* Ext = nullptr;
	const UGSTN_DefenceRules* Rules = nullptr;
	if (!FGSTN_DefenceStepUtils::Find(View.Victim, Ext, Rules))
	{
		return 0;
	}
	uint8 Flags = 0;
	if (View.Incoming.Hit.bDisableDI)
	{
		Flags |= GSTNDefenceHitFlags::DIDisabled;
	}
	const FGSTN_SDIRules& SDI = Rules->SDI;
	const bool bThrow = UGSTN_DefenceMath::IsGrabLikeHitType(View.Incoming.Hit.HitType);
	const bool bArmored = View.HurtState == EGSTN_HurtState::Armor && !SDI.bSDIAllowedWhenArmored;
	const bool bInPlace = View.Outcome.bHasKnockback && View.Outcome.Knockback.Reaction == EGSTN_HitReaction::StaggerInPlace && !SDI.bSDIInStaggerInPlace;
	if (bThrow || bArmored || bInPlace)
	{
		Flags |= GSTNDefenceHitFlags::SDIBlocked;
	}
	return Flags;
}

FVector FGSTN_DIWiring::ApplyRulesDI(const UGSTN_DefenceRules& Rules, const FGSTN_DefenceFragment* Fragment, const FVector& Launch, const FVector& StickOnPlane, const FVector& Up,
	const FVector& YawFallback, const FGameplayTagContainer& VictimTags)
{
	const bool bDisabled = Fragment && (Fragment->HitFlags & GSTNDefenceHitFlags::DIDisabled) != 0;
	return UGSTN_DIRulesMath::ApplyDI(Launch, StickOnPlane, Up, YawFallback, Launch.Size(), VictimTags, Rules.DI, bDisabled);
}

FGameplayTagContainer FGSTN_DIWiring::ReadVictimTags(const FGSTN_CombatTickContext& Context)
{
	FGameplayTagContainer Tags;
	if (Context.AbilitySystem)
	{
		Context.AbilitySystem->GetOwnedGameplayTags(Tags);
	}
	return Tags;
}

bool FGSTN_DIWiring::OverrideDI(const FGSTN_CombatTickContext& Context, const FVector& Launch, const FVector& StickOnPlane, const FVector& Up, const FVector& YawFallback, FVector& OutVelocity)
{
	const FGSTN_DefenceEntityData* Data = FGSTN_DefenceAttach::Find(Context.Owner);
	if (!Data || !Data->Rules)
	{
		return false;
	}
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	OutVelocity = ApplyRulesDI(*Data->Rules, Fragment, Launch, StickOnPlane, Up, YawFallback, ReadVictimTags(Context));
	return true;
}

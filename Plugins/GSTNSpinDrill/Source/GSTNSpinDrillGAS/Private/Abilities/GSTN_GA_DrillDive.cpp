// Spin and drill, ability-system half.

#include "Abilities/GSTN_GA_DrillDive.h"

#include "Data/GSTN_SpinTimelines.h"
#include "Effects/GSTN_SpinDrillEffects.h"
#include "GameplayEffect.h"
#include "GSTN_SpinDrillTags.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_SpinDrillAttach.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"

UGSTN_GA_DrillDive::UGSTN_GA_DrillDive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Timeline = FGSTN_SpinTimelines::MakeDive(FGSTN_DiveTimelineParams());
	PlungeEffect = UGSTN_GE_DrillPlunge::StaticClass();
	BounceEffect = UGSTN_GE_DrillBounce::StaticClass();
	// Releasing crouch, the drill's trigger, stops the dive (spec 1.4)
	bEndOnTriggerRelease = true;
	SetAssetTags(FGameplayTagContainer(GSTNSpinDrillTags::Ability_Combat_Drill_Dive.GetTag()));
}

void UGSTN_GA_DrillDive::OnTimelineBegan(int32 StateStartFrame)
{
	Super::OnTimelineBegan(StateStartFrame);
	if (PlungeEffect)
	{
		ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), PlungeEffect.GetDefaultObject(), GetAbilityLevel());
	}
}

int32 UGSTN_GA_DrillDive::DecideFromFragment(const FGSTN_DrillFragment* Fragment, int32 Frame, int32 GroundLagFrames)
{
	if (!Fragment)
	{
		return 0;
	}
	if ((Fragment->Flags & GSTNDrillFlags::Bounced) != 0)
	{
		return 1;
	}
	return Fragment->Phase == EGSTN_DrillPhase::Lag && Fragment->FramesInPhase(Frame) >= GroundLagFrames ? 2 : 0;
}

void UGSTN_GA_DrillDive::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	if (!IsActive() || !HasTimelineStarted())
	{
		return;
	}
	// The combat tick of this frame ran before the ability system: its state is the frame's
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	const FGSTN_DrillFragment* Fragment = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Sync->Fragments) : nullptr;
	const int32 GroundLagFrames = FGSTN_SpinDrillAttach::FindOrDefault(GetAvatarActorFromActorInfo()).GroundLagFrames;
	switch (DecideFromFragment(Fragment, Combat ? Combat->GetSimulationFrame() : INDEX_NONE, GroundLagFrames))
	{
	case 1:
		if (BounceEffect)
		{
			ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), BounceEffect.GetDefaultObject(), GetAbilityLevel());
		}
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		break;
	case 2:
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		break;
	default:
		break;
	}
}

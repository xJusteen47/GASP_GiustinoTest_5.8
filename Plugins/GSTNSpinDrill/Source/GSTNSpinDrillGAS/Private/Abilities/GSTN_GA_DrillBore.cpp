// Spin and drill, ability-system half.

#include "Abilities/GSTN_GA_DrillBore.h"

#include "Data/GSTN_SpinTimelines.h"
#include "Effects/GSTN_SpinDrillEffects.h"
#include "GameplayEffect.h"
#include "GSTN_SpinDrillTags.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Sync/GSTN_SyncFragment.h"

UGSTN_GA_DrillBore::UGSTN_GA_DrillBore(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Timeline = FGSTN_SpinTimelines::MakeBore(FGSTN_BoreTimelineParams());
	LaunchEffect = UGSTN_GE_DrillLaunch::StaticClass();
	PopEffect = UGSTN_GE_DrillPop::StaticClass();
	// The release is read by the phase machine (the bore track's crouch stamp), not by the ability: ending here would race the decision
	bEndOnTriggerRelease = false;
	// Added to the base's tags, never replacing them: Ability.Timeline (UGSTN_GA_BaseCombat) is how the loadout compile knows a timeline
	// ability; without it the command activates the ability as a plain one and the combat tick never adopts it (SD-05)
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(GSTNSpinDrillTags::Ability_Combat_Drill_Bore.GetTag());
	SetAssetTags(Tags);
}

int32 UGSTN_GA_DrillBore::DecideFromFragment(const FGSTN_DrillFragment* Fragment, int32 Frame)
{
	if (!Fragment)
	{
		return 0;
	}
	if (Fragment->Phase == EGSTN_DrillPhase::Launching && Fragment->FramesInPhase(Frame) == 0)
	{
		return 1;
	}
	return (Fragment->Flags & GSTNDrillFlags::Popped) != 0 ? 2 : 0;
}

void UGSTN_GA_DrillBore::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
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
	const int32 Decision = DecideFromFragment(Fragment, Combat ? Combat->GetSimulationFrame() : INDEX_NONE);
	const TSubclassOf<UGameplayEffect> Effect = Decision == 1 ? LaunchEffect : (Decision == 2 ? PopEffect : nullptr);
	if (Effect)
	{
		ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), Effect.GetDefaultObject(), GetAbilityLevel());
	}
}

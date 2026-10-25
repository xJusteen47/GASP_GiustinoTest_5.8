// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsTickExtension.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Effects/GSTN_GE_PartStats.h"
#include "Effects/GSTN_PartBreakEffectApplier.h"
#include "Math/GSTN_PartMath.h"
#include "Projection/GSTN_PartTagProjection.h"
#include "Sim/GSTN_PartsAttach.h"
#include "State/GSTN_PartsFragment.h"
#include "Sync/GSTN_SyncFragment.h"

uint8 FGSTN_PartsTickExtension::GetTypeId() const
{
	return GSTNFeatureIds::Parts;
}

void FGSTN_PartsTickExtension::SetStatsEffectClass(TSubclassOf<UGSTN_GE_PartStats> InClass)
{
	StatsEffectClass = InClass;
}

void FGSTN_PartsTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	if (Point != EGSTN_CombatHookPoint::BeforeCandidates && Point != EGSTN_CombatHookPoint::BeforeTagProjection)
	{
		return;
	}
	const UGSTN_PartSetDefinition* Set = FGSTN_PartsAttach::FindSet(Context.Owner);
	if (!Set)
	{
		return;
	}

	if (Point == EGSTN_CombatHookPoint::BeforeCandidates)
	{
		// The state the frame started from: the fragment at the end of F-1
		const FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Context.In.Fragments);
		if (!Fragment || Fragment->BrokenMask == 0)
		{
			return;
		}
		UGSTN_PartMath::ProjectDerived(Set->GetParts(), Fragment->ToState(), Set->GrabGate, Scratch);
		for (uint8 Group = 0; Group < 8; ++Group)
		{
			if (Scratch.HitGroupMask & (1u << Group))
			{
				Context.HitGroupMask.Set(Group);
			}
		}
		return;
	}

	// BeforeTagProjection: the state the frame wrote, so a break of this frame is visible to Mover in it
	UAbilitySystemComponent* ASC = Context.AbilitySystem;
	if (!ASC)
	{
		return;
	}
	const FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Context.Out.Fragments);
	const FGSTN_PartsState State = Fragment ? Fragment->ToState() : FGSTN_PartsState();
	UGSTN_PartMath::ProjectDerived(Set->GetParts(), State, Set->GrabGate, Scratch);
	FGSTN_PartTagProjection::Apply(*ASC, Set->GetParts(), Scratch, FGameplayTagContainer());

	if (StatsEffectClass)
	{
		const uint16 Breakable = Set->GetBreakableMask();
		FGSTN_PartBreakEffectApplier::ApplyStats(*ASC, StatsEffectClass, Set->GetParts(), State.BrokenMask);
		FGSTN_PartBreakEffectApplier::RemoveStats(*ASC, StatsEffectClass, static_cast<uint16>(Breakable & ~State.BrokenMask));
	}
}

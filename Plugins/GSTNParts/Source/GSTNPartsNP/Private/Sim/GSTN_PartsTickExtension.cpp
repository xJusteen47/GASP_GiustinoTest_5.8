// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsTickExtension.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Effects/GSTN_GE_PartStats.h"
#include "Effects/GSTN_PartBreakEffectApplier.h"
#include "Events/GSTN_PartEvents.h"
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
	if (Point != EGSTN_CombatHookPoint::AfterClocks && Point != EGSTN_CombatHookPoint::BeforeCandidates && Point != EGSTN_CombatHookPoint::BeforeTagProjection)
	{
		return;
	}
	const UGSTN_PartSetDefinition* Set = FGSTN_PartsAttach::FindSet(Context.Owner);
	if (!Set)
	{
		return;
	}

	if (Point == EGSTN_CombatHookPoint::AfterClocks)
	{
		ConsumeRestores(Context, *Set);
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

void FGSTN_PartsTickExtension::ConsumeRestores(FGSTN_CombatTickContext& Context, const UGSTN_PartSetDefinition& Set)
{
	UAbilitySystemComponent* ASC = Context.AbilitySystem;
	if (!ASC)
	{
		return;
	}
	FGSTN_PartRestoreQueue::Collect(*ASC, PendingRestores);
	if (PendingRestores.IsEmpty())
	{
		return;
	}

	// Written like the outcome applier writes it: the frame's output fragment, from the state the hits of this frame left
	FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Context.Out.Fragments);
	const bool bKnockedOut = Context.Out.Core.ActionState == EGSTN_ActionState::KO;
	const bool bPlayerSet = Set.GetParts().Num() == 1;
	if (Fragment)
	{
		FGSTN_PartsState State = Fragment->ToState();
		for (const FGSTN_PendingPartRestore& Pending : PendingRestores)
		{
			if (bKnockedOut && bPlayerSet)
			{
				// The single part of a player is its life: restoring it would revive a knocked out player behind the KO's back
				continue;
			}
			const uint16 Restored = UGSTN_PartMath::RestoreParts(Set.GetParts(), State, Pending.Request.GetMask16(), Pending.Request.DamageFraction);
			if (Restored != 0 && Context.Outbox)
			{
				FGSTN_PartEvents::AppendRestored(Context.Owner, Pending.Instigator.Get(), Context.Frame, Restored, State.BreakSerial, *Context.Outbox);
			}
		}
		Fragment->SetState(State, Fragment->PartCount);
	}
	FGSTN_PartRestoreQueue::Consume(*ASC, PendingRestores);
}

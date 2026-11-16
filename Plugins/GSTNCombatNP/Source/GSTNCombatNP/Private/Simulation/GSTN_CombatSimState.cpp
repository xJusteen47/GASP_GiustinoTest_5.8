// Combat simulation (GAS-NPP fork): the Blueprint state queries and events of the combat component (bp-exposure P1, owner 1A 2A 2026-11-16).
// Everything here runs OUTSIDE the simulation (FinalizeFrame, Blueprint reads): it reads the presented state and never writes sync state.

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "GameFramework/Actor.h"
#include "Movement/GSTN_RunStageSyncState.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"

namespace GSTNCombatStateEvents
{
	/** The root every combat event type sits under; the input events (Input.Event.CommandExecuted, one per command) are not sent */
	static bool IsCombatEvent(const FGameplayTag& EventType)
	{
		static const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("Combat.Event"), false);
		return Root.IsValid() && EventType.MatchesTag(Root);
	}
}

FGSTN_CombatStateView UGSTN_CombatSimComponent::BuildStateView(const FGSTN_CombatSyncCore& Core) const
{
	FGSTN_CombatStateView View = FGSTN_CombatStateView::FromCore(Core);
	if (!View.bValid)
	{
		return View;
	}
	if (View.bInMove)
	{
		const UGSTN_CombatTableSet& Set = Tables();
		if (Set.TimelineRegistry.IsValidIndex(View.MoveEntry))
		{
			View.MoveAbility = Set.TimelineRegistry[View.MoveEntry];
		}
		if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Set, View.MoveEntry))
		{
			View.MoveTotalFrames = Data->TotalFrames;
			View.MoveFirstActionableFrame = Data->FirstActionableFrame;
		}
	}

	if (!CachedMover.IsValid() && GetOwner())
	{
		CachedMover = GetOwner()->FindComponentByClass<UMoverComponent>();
	}
	if (const UMoverComponent* Mover = CachedMover.Get())
	{
		// The movement component's presented state: the floor is Mover's, a launch by a hit (no jump phase) is airborne too
		const FMoverSyncState& MoverState = Mover->GetSyncState();
		View.bAirborne = !Mover->HasGameplayTagInState(MoverState, Mover_IsOnGround, true);
		if (View.bAirborne && View.AirState == EGSTN_CombatAirState::Grounded)
		{
			View.AirState = EGSTN_CombatAirState::Falling;
		}
		else if (!View.bAirborne)
		{
			// The one frame before the jump state notices a landing: the floor wins
			View.AirState = EGSTN_CombatAirState::Grounded;
		}
		if (const FGSTN_RunStageState* Run = FGSTN_RunStageMover::Find(MoverState))
		{
			View.bHasRunStages = true;
			View.RunStage = Run->Stage;
		}
	}
	return View;
}

FGSTN_CombatStateView UGSTN_CombatSimComponent::GetCombatStateView() const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	return Sync ? BuildStateView(Sync->Core) : FGSTN_CombatStateView();
}

int32 UGSTN_CombatSimComponent::GetFramesSinceLanded() const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	return Sync ? StateWatcher.GetFramesSinceLanded(Sync->Core.Input.History.GetLatestFrame()) : INDEX_NONE;
}

FGameplayTagContainer UGSTN_CombatSimComponent::GetCurrentMoveTags() const
{
	FGameplayTagContainer Tags;
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	if (Sync && Sync->Core.ActionState == EGSTN_ActionState::InTimeline)
	{
		const UGSTN_CombatTableSet& Set = Tables();
		const int32 Entry = Sync->Core.ActiveTimeline;
		if (Set.TimelineRegistry.IsValidIndex(Entry) && Set.TimelineRegistry[Entry])
		{
			Tags = Set.TimelineRegistry[Entry]->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetAssetTags();
		}
	}
	return Tags;
}

bool UGSTN_CombatSimComponent::GetLastHitReceived(FGSTN_CombatHitEventData& OutHit) const
{
	if (bHasLastHitReceived)
	{
		OutHit = LastHitReceived;
	}
	return bHasLastHitReceived;
}

bool UGSTN_CombatSimComponent::GetLastHitLanded(FGSTN_CombatHitEventData& OutHit) const
{
	if (bHasLastHitLanded)
	{
		OutHit = LastHitLanded;
	}
	return bHasLastHitLanded;
}

void UGSTN_CombatSimComponent::SendConfirmedEvents()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		// A predicting client: what its predictions and re-simulations wrote is dropped, like the outbox (the server's copy arrives by multicast)
		HitEventOutbox.Reset();
		return;
	}

	for (const FGSTN_CombatHitEventData& Hit : HitEventOutbox)
	{
		if (!Hit.bRicochet)
		{
			DispatchHit(Hit, true);
		}
		// On Hit Landed fires on the attacker's own component: the victim's tick made the hit, the attacker's Blueprint listens for it
		UGSTN_CombatSimComponent* AttackerCombat = Hit.Attacker ? Hit.Attacker->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
		if (AttackerCombat)
		{
			AttackerCombat->DispatchHit(Hit, false);
		}
	}
	HitEventOutbox.Reset();

	for (const FGSTN_SimEvent& Event : EventOutbox.Events)
	{
		if (!GSTNCombatStateEvents::IsCombatEvent(Event.EventType))
		{
			continue;
		}
		// PublishOutbox names the owner when the event names nobody; the Blueprint copy says the same
		FGSTN_SimEvent Copy = Event;
		if (!Copy.Instigator.IsValid())
		{
			Copy.Instigator = Owner;
		}
		FGSTN_CombatKOEventData KnockOut;
		if (FGSTN_CombatKOEventData::FromSimEvent(Copy, KnockOut))
		{
			// The victim of an outbox KO is this entity; the lookup keeps it right if a plugin ever writes a KO of another one
			UGSTN_CombatSimComponent* VictimCombat = KnockOut.Victim ? KnockOut.Victim->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
			(VictimCombat ? VictimCombat : this)->DispatchKnockOut(KnockOut);
		}
		DispatchCombatEvent(Copy);
	}
}

// One path for the three kinds: the multicast runs on the server too (and alone in a standalone game), so every machine fires from the same
// _Implementation; with the switch off the server fires locally and sends nothing
void UGSTN_CombatSimComponent::DispatchHit(const FGSTN_CombatHitEventData& Hit, bool bReceived)
{
	if (bSendConfirmedEventsToClients)
	{
		MulticastConfirmedHit(Hit, bReceived);
	}
	else
	{
		HandleConfirmedHit(Hit, bReceived);
	}
}

void UGSTN_CombatSimComponent::DispatchKnockOut(const FGSTN_CombatKOEventData& KnockOut)
{
	if (bSendConfirmedEventsToClients)
	{
		MulticastConfirmedKnockOut(KnockOut);
	}
	else
	{
		OnKnockedOut.Broadcast(KnockOut);
	}
}

void UGSTN_CombatSimComponent::DispatchCombatEvent(const FGSTN_SimEvent& Event)
{
	if (bSendConfirmedEventsToClients)
	{
		MulticastConfirmedEvent(Event);
	}
	else
	{
		OnCombatEvent.Broadcast(Event);
	}
}

void UGSTN_CombatSimComponent::MulticastConfirmedHit_Implementation(const FGSTN_CombatHitEventData& Hit, bool bReceived)
{
	HandleConfirmedHit(Hit, bReceived);
}

void UGSTN_CombatSimComponent::MulticastConfirmedKnockOut_Implementation(const FGSTN_CombatKOEventData& KnockOut)
{
	OnKnockedOut.Broadcast(KnockOut);
}

void UGSTN_CombatSimComponent::MulticastConfirmedEvent_Implementation(const FGSTN_SimEvent& Event)
{
	OnCombatEvent.Broadcast(Event);
}

void UGSTN_CombatSimComponent::HandleConfirmedHit(const FGSTN_CombatHitEventData& Hit, bool bReceived)
{
	if (bReceived)
	{
		LastHitReceived = Hit;
		bHasLastHitReceived = true;
		OnHitReceived.Broadcast(Hit);
	}
	else
	{
		LastHitLanded = Hit;
		bHasLastHitLanded = true;
		OnHitLanded.Broadcast(Hit);
	}
}

void UGSTN_CombatSimComponent::UpdateStateEvents(const FGSTN_CombatSyncState& Sync)
{
	const FGSTN_CombatStateView Now = BuildStateView(Sync.Core);
	if (!Now.bValid)
	{
		return;
	}
	const int32 OldStage = StateWatcher.GetPrevious().RunStage;
	TArray<EGSTN_CombatStateChange> Changes;
	StateWatcher.Update(Now, Changes);
	for (const EGSTN_CombatStateChange Change : Changes)
	{
		switch (Change)
		{
		case EGSTN_CombatStateChange::MoveStarted: OnMoveStarted.Broadcast(Now); break;
		case EGSTN_CombatStateChange::MoveEnded: OnMoveEnded.Broadcast(Now); break;
		case EGSTN_CombatStateChange::HitstunStarted: OnHitstunStarted.Broadcast(Now); break;
		case EGSTN_CombatStateChange::HitstunEnded: OnHitstunEnded.Broadcast(Now); break;
		case EGSTN_CombatStateChange::TumbleStarted: OnTumbleStarted.Broadcast(Now); break;
		case EGSTN_CombatStateChange::TumbleEnded: OnTumbleEnded.Broadcast(Now); break;
		case EGSTN_CombatStateChange::Landed: OnLanded.Broadcast(Now); break;
		case EGSTN_CombatStateChange::LeftGround: OnLeftGround.Broadcast(Now); break;
		case EGSTN_CombatStateChange::RunStageChanged: OnRunStageChanged.Broadcast(Now.RunStage, OldStage); break;
		}
	}
}

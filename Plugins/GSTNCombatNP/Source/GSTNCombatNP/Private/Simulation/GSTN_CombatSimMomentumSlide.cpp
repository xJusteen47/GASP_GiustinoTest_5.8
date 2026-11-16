// The momentum slide of ground moves started while running (owner 2026-11-13).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Movement/GSTN_LayeredMove_MomentumSlide.h"

void UGSTN_CombatSimComponent::ExecuteMomentumSlide()
{
	// The move reads the running velocity, the facing and the move's data on its own first frame (Mover's tick of this frame), so the
	// effect carries nothing: one code path for every machine and every re-simulation
	if (!AbilitySystem || !MomentumSlideEffect)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(MomentumSlideEffect, 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

bool UGSTN_CombatSimComponent::ReadRunningSlide(int32& OutStartFrame, const FGSTN_MomentumSlide*& OutSlide) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || Sync->Core.RecoilStartFrame != INDEX_NONE)
	{
		return false;
	}
	const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Tables(), Sync->Core.ActiveTimeline);
	if (!Data)
	{
		return false;
	}
	OutStartFrame = Sync->Core.TimelineStartFrame;
	OutSlide = &Data->MomentumSlide;
	return true;
}

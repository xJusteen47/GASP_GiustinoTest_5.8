// The momentum slide of ground moves started while running (owner 2026-11-13), the ledge guard of the Ledge Check track (owner 2026-11-16) and the
// dodge landing with its belly slide (owner 2026-11-17).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Movement/GSTN_LayeredMove_MomentumSlide.h"
#include "Simulation/GSTN_CombatTableSet.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNDodgeLanding, Log, All);

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

int32 UGSTN_CombatSimComponent::OpenDodgeLanding(int32 LandedFrame, int32 MoveLandingLagFrames)
{
	const FGSTN_DodgeLandingKitExtension& Kit = Tables().KitDodgeLanding;
	const int32 EndLag = FGSTN_DodgeLandingLogic::ResolveEndLag(Kit, MoveLandingLagFrames);
	const FGSTN_CombatSyncState* Pending = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!AbilitySystem || !Pending || !AbilitySystem->IsInSimulationTick() || AbilitySystem->GetIsRestoringFrame())
	{
		UE_LOG(LogGSTNDodgeLanding, Log, TEXT("%s: dodge landing on frame %d not written (outside the simulation step)"), *GetNameSafe(GetOwner()), LandedFrame);
		return EndLag;
	}

	// The combat tick of this frame ran: its pending state is this frame's output, the one the next frame's tick and gate read
	const FGSTN_DodgeLandingState Opened = FGSTN_DodgeLandingState::Open(LandedFrame, EndLag, Kit.RedodgeLockFrames);
	const FGSTN_CombatSyncState* Written = NetworkPredictionProxy.WriteSyncState<FGSTN_CombatSyncState>([&](FGSTN_CombatSyncState& Sync)
	{
		Sync.Core.DodgeLanding = Opened;
	}, "GSTN DodgeLanding");
	if (!Written)
	{
		return EndLag;
	}
	UE_LOG(LogGSTNDodgeLanding, Verbose, TEXT("%s: dodge landed on frame %d: lag %d, lock %d"), *GetNameSafe(GetOwner()), LandedFrame, EndLag, Kit.RedodgeLockFrames);
	// The pre-tick projected this frame's tags before the write
	ProjectStateTags(Written->Core, LandedFrame);

	// The belly slide: the move reads the landing velocity and the kit's data on its own first frame (Mover's tick of this frame)
	if (Opened.IsEndLag(LandedFrame) && Kit.LandingSlide.bEnabled && DodgeLandingSlideEffect)
	{
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(DodgeLandingSlideEffect, 1.f, AbilitySystem->MakeEffectContext());
		if (Spec.IsValid())
		{
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	return EndLag;
}

bool UGSTN_CombatSimComponent::ReadLandingSlide(int32& OutLandedFrame, const FGSTN_MomentumSlide*& OutSlide) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || !Sync->Core.DodgeLanding.IsEndLag(Sync->Core.Input.History.GetLatestFrame()))
	{
		return false;
	}
	OutLandedFrame = Sync->Core.DodgeLanding.LandedFrame;
	OutSlide = &Tables().KitDodgeLanding.LandingSlide;
	return true;
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

void UGSTN_CombatSimComponent::ExecuteLedgeGuard()
{
	// Like the slide: the move latches its timeline on its own first frame and reads the clock's decision every frame, so the effect carries nothing
	if (!AbilitySystem || !LedgeGuardEffect)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(LedgeGuardEffect, 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

bool UGSTN_CombatSimComponent::ReadLedgeStop(int32& OutStartFrame, bool& bOutStop) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || Sync->Core.RecoilStartFrame != INDEX_NONE)
	{
		return false;
	}
	OutStartFrame = Sync->Core.TimelineStartFrame;
	bOutStop = Sync->Core.bLedgeStop;
	return true;
}

// The run skid of the Mario-style run (docs/notes/corsa-mario-piano.md, P2): the combat tick's half (the decision), the move does the rest.

#include "Simulation/GSTN_CombatSimComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Movement/GSTN_LayeredMove_RunSkid.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "Movement/GSTN_RunStageSyncState.h"
#include "Movement/GSTN_StatsMovementModifier.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "HAL/IConsoleManager.h"

namespace GSTNRunSkidSim
{
	// skid-walkturn-2: 0 keeps our sim skid from starting, so GASP's stock pivot (Run/Sprint Loco Transitions, IsPivoting) shows on a
	// reverse. A test switch: it changes the simulation, so set it the same on server and clients or they mispredict
	TAutoConsoleVariable<int32> CVarSkidSim(TEXT("gstn.Skid.Sim"), 1, TEXT("1 our run skid (layered move) starts on a reverse, 0 never (stock GASP pivot only)"));
}

bool UGSTN_CombatSimComponent::IsFreeToMove() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync && Sync->Core.ActionState == EGSTN_ActionState::Actionable;
}

void UGSTN_CombatSimComponent::StepRunSkid(const FGSTN_CombatSyncCore& Core)
{
	if (GSTNRunSkidSim::CVarSkidSim.GetValueOnAnyThread() == 0)
	{
		return;
	}
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(GetOwner());
	if (!Locomotion || !Locomotion->RunStages.IsEnabled() || !Locomotion->RunStages.Skid.bEnabled || !AbilitySystem || !RunSkidEffect)
	{
		return;
	}
	// Only a free run skids: an attack, a reaction, a jump (squatting or airborne) take the frame
	if (Core.ActionState != EGSTN_ActionState::Actionable || Core.Jump.SquatFramesLeft > 0 || Core.Jump.Phase != EGSTN_JumpPhase::None)
	{
		return;
	}
	UGSTN_MoverSimStateTracker* Tracker = GetJumpTracker();
	if (!Tracker || !Tracker->HasGameplayTag(Mover_IsOnGround, true) || FGSTN_LayeredMove_RunSkid::FindActive(Tracker->GetSyncState()))
	{
		return;
	}
	const FGSTN_RunStageState* Run = FGSTN_RunStageMover::Find(Tracker->GetSyncState());
	float Factor = 1.f;
	if (!Run || !FGSTN_StatsMovementModifier::ResolveSpeedFactor(*AbilitySystem, Factor))
	{
		return;
	}

	const FVector Up = Tracker->GetUpDirection();
	const FVector Velocity = Tracker->GetVelocity();
	const FVector Ground = Velocity - Up * Velocity.Dot(Up);
	FVector2D Stick;
	float CameraYaw = 0.f;
	GetFrameStick(Stick, CameraYaw);
	const double StickSize = FMath::Min(Stick.Size(), 1.0);
	const FVector StickWorld = StickSize > 0.0 ? FGSTN_CombatVictim::StickToPlane(Stick, CameraYaw, Up).GetSafeNormal() * StickSize : FVector::ZeroVector;

	if (!FGSTN_RunSkidMath::ShouldStart(Locomotion->RunStages.Skid, Run->Stage, Ground, StickWorld, Factor))
	{
		return;
	}
	// The move reads the velocity and the stick on its own first frame (Mover's tick of this frame): the effect carries nothing
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(RunSkidEffect, 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

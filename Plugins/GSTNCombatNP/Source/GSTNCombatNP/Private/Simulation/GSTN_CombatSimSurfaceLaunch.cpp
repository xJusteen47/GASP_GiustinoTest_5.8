// The surface launch of a non-tumbling combatant (M4 H11b).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_CombatNPTags.h"
#include "Movement/GSTN_LayeredMove_Launch.h"
#include "Movement/GSTN_SurfaceLaunch.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"

void UGSTN_CombatSimComponent::StepSurfaceLaunch(const FAbilitySystemTimeStep& TimeStep, const FGSTN_CombatSyncCore& Core)
{
	const int32 Frame = TimeStep.ServerFrame;

	// A tumbling combatant meets a trampoline through the impact table (a bounce); a frozen one (hitstop) does not move at all
	if (!AbilitySystem || !SurfaceLaunchEffect || Core.ActionState == EGSTN_ActionState::Tumble || Core.IsInHitstop(Frame))
	{
		return;
	}

	UGSTN_MoverSimStateTracker* Tracker = GetJumpTracker();
	const AActor* Owner = GetOwner();
	const UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	if (!Tracker || !Mover)
	{
		return;
	}

	FGSTN_SurfaceLaunchDecision Decision;
	if (!FGSTN_SurfaceLaunch::Probe(Tracker->GetSyncState(), *Mover, Tracker->GetUpDirection(), TimeStep.StepMs * 0.001, Decision))
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(SurfaceLaunchEffect, 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, Decision.Velocity.X);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, Decision.Velocity.Y);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, Decision.Velocity.Z);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_LaunchResetAirJumps, Decision.bResetAirJumps ? 1.f : 0.f);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_LaunchResetCoyote, Decision.bResetCoyote ? 1.f : 0.f);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	++SurfaceLaunchCount;
	LastSurfaceLaunchTag = Decision.LaunchTag;
	LastSurfaceLaunchInfo = FString::Printf(TEXT("%d,%d,%.1f,%.1f,%.1f,%d,%.1f"), SurfaceLaunchCount, Frame, Decision.Velocity.X, Decision.Velocity.Y, Decision.Velocity.Z,
		Decision.SurfaceTypeIndex, Decision.IncomingSpeed);
}

FString UGSTN_CombatSimComponent::GetSurfaceLaunchInfo() const
{
	return LastSurfaceLaunchInfo;
}

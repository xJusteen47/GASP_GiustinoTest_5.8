// Combat simulation (GAS-NPP fork): the combat state of any actor, for Blueprints (bp-exposure P1).

#include "Presentation/GSTN_CombatStateLibrary.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_CombatSimComponent.h"

UGSTN_CombatSimComponent* UGSTN_CombatStateLibrary::GetCombatComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
}

FGSTN_CombatStateView UGSTN_CombatStateLibrary::GetCombatState(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent(Actor);
	return Combat ? Combat->GetCombatStateView() : FGSTN_CombatStateView();
}

EGSTN_CombatStatus UGSTN_CombatStateLibrary::GetCombatStatus(const AActor* Actor)
{
	return GetCombatState(Actor).Status;
}

int32 UGSTN_CombatStateLibrary::GetHitstunFramesLeft(const AActor* Actor)
{
	return GetCombatState(Actor).HitstunFramesLeft;
}

int32 UGSTN_CombatStateLibrary::GetHitstopFramesLeft(const AActor* Actor)
{
	return GetCombatState(Actor).HitstopFramesLeft;
}

bool UGSTN_CombatStateLibrary::IsInHitstun(const AActor* Actor)
{
	return GetCombatState(Actor).bInHitstun;
}

bool UGSTN_CombatStateLibrary::IsInTumble(const AActor* Actor)
{
	return GetCombatState(Actor).bInTumble;
}

bool UGSTN_CombatStateLibrary::IsKnockedOut(const AActor* Actor)
{
	return GetCombatState(Actor).bKnockedOut;
}

bool UGSTN_CombatStateLibrary::GetLastHitReceived(const AActor* Actor, FGSTN_CombatHitEventData& Hit)
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent(Actor);
	return Combat && Combat->GetLastHitReceived(Hit);
}

bool UGSTN_CombatStateLibrary::GetLastHitLanded(const AActor* Actor, FGSTN_CombatHitEventData& Hit)
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent(Actor);
	return Combat && Combat->GetLastHitLanded(Hit);
}

bool UGSTN_CombatStateLibrary::GetCurrentMove(const AActor* Actor, TSubclassOf<UGSTN_GA_BaseCombat>& MoveAbility, int32& MoveFrame, int32& TotalFrames)
{
	const FGSTN_CombatStateView State = GetCombatState(Actor);
	MoveAbility = State.MoveAbility;
	MoveFrame = State.MoveFrame;
	TotalFrames = State.MoveTotalFrames;
	return State.bInMove;
}

FGameplayTagContainer UGSTN_CombatStateLibrary::GetCurrentMoveTags(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent(Actor);
	return Combat ? Combat->GetCurrentMoveTags() : FGameplayTagContainer();
}

int32 UGSTN_CombatStateLibrary::GetRunStage(const AActor* Actor)
{
	return GetCombatState(Actor).RunStage;
}

int32 UGSTN_CombatStateLibrary::GetCoyoteFramesLeft(const AActor* Actor)
{
	return GetCombatState(Actor).CoyoteFramesLeft;
}

bool UGSTN_CombatStateLibrary::IsCombatAirborne(const AActor* Actor)
{
	return GetCombatState(Actor).bAirborne;
}

EGSTN_CombatAirState UGSTN_CombatStateLibrary::GetCombatAirState(const AActor* Actor)
{
	return GetCombatState(Actor).AirState;
}

int32 UGSTN_CombatStateLibrary::GetFramesSinceLanded(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent(Actor);
	return Combat ? Combat->GetFramesSinceLanded() : INDEX_NONE;
}

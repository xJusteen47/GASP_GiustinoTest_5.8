// Held objects, world side (needs the GAS-NPP fork).

#include "GSTN_HeldAbilities.h"

#include "Engine/World.h"
#include "GSTN_HeldObjectComponent.h"
#include "GSTN_HeldSubsystem.h"

void UGSTN_GA_HeldPickup::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Host side effect only (the host never resimulates, so one request per activation)
	UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	if (Held && Held->HasAuthority())
	{
		Held->RequestPickup(GetAvatarActorFromActorInfo());
	}
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

UGSTN_GA_HeldPickupServer::UGSTN_GA_HeldPickupServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

UGSTN_GA_HeldThrow::UGSTN_GA_HeldThrow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// TEST VALUES: released from where the object is held, a little upwards
	MuzzleOffset = FVector(45.0, 0.0, 70.0);
	LaunchPitchDeg = 15.f;
}

bool UGSTN_GA_HeldThrow::PrepareShot(FGSTN_ProjectileParams& InOutParams, bool bHost)
{
	const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	const UGSTN_HeldObjectComponent* Object = Held ? Held->FindHeldBy(GetAvatarActorFromActorInfo()) : nullptr;
	if (!Object)
	{
		if (bHost)
		{
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldThrow: %s holds nothing, no throw"), *GetNameSafe(GetAvatarActorFromActorInfo()));
		}
		return false;
	}
	InOutParams = Object->GetTuning().Throw;
	return true;
}

void UGSTN_GA_HeldThrow::OnShotFired(AGSTN_Projectile& Shot)
{
	if (UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()))
	{
		Held->NoteThrown(GetAvatarActorFromActorInfo(), Shot);
	}
}

UGSTN_GA_HeldThrowServer::UGSTN_GA_HeldThrowServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

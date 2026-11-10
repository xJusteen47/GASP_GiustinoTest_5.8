// Held objects, world side (needs the GAS-NPP fork).

#include "GSTN_HeldAbilities.h"
#include "Hits/GSTN_CombatHitTags.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTN_HeldSettings.h"
#include "Presentation/GSTN_PrecisionAimComponent.h"
#include "Simulation/GSTN_CombatSimComponent.h"
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

void UGSTN_GA_HeldDrop::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Host side effect only (the host never resimulates, so one request per activation)
	UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	if (Held && Held->HasAuthority())
	{
		Held->RequestDrop(GetAvatarActorFromActorInfo());
	}
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

UGSTN_GA_HeldDropServer::UGSTN_GA_HeldDropServer(const FObjectInitializer& ObjectInitializer)
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
	PressClock.Value = INDEX_NONE;
	ReleaseAim.Value = static_cast<uint8>(EGSTN_HeldThrowAim::Stick);
}

void UGSTN_GA_HeldThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	PressClock.Value = INDEX_NONE;
	ReleaseAim.Value = static_cast<uint8>(EGSTN_HeldThrowAim::Stick);
	// The parent captures the context, then asks DeferFire: with a button it fires nothing now and the release fires from the tick below
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (IsActive() && DeferFire())
	{
		PressClock.Value = FMath::Max(GetCombatClock(), 0);
	}
}

bool UGSTN_GA_HeldThrow::DeferFire() const
{
	return GetActivationContext().Button != INDEX_NONE;
}

void UGSTN_GA_HeldThrow::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	if (!IsActive() || PressClock.Value == INDEX_NONE)
	{
		return;
	}
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	const FGSTN_HeldThrowInput& Input = UGSTN_HeldSettings::Get().ThrowInput;
	const int32 HeldFrames = FMath::Max(GetCombatClock() - PressClock.Value, 0);
	const bool bHeld = Combat && Combat->IsCommandButtonHeld(GetActivationContext().Button);
	if (bHeld && HeldFrames < Input.MaxAimFrames)
	{
		ShowAimReticle(FGSTN_HeldRules::IsAiming(HeldFrames, Input));
		return;
	}
	ReleaseAim.Value = static_cast<uint8>(FGSTN_HeldRules::ChooseThrowAim(HeldFrames, Combat && Combat->IsPrecisionAimFrame(), Input));
	PressClock.Value = INDEX_NONE;
	ShowAimReticle(false);
	Fire();
	FinishFire();
}

void UGSTN_GA_HeldThrow::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Cancelled while aiming (a hit, a forced move): no throw, and the reticle goes away; a hit also breaks the hold on the host
	PressClock.Value = INDEX_NONE;
	ShowAimReticle(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

EGSTN_ProjectileAim UGSTN_GA_HeldThrow::GetShotAim() const
{
	if (Aim == EGSTN_ProjectileAim::EventTarget || !DeferFire())
	{
		return Aim;
	}
	return static_cast<EGSTN_HeldThrowAim>(ReleaseAim.Value) == EGSTN_HeldThrowAim::Stick ? EGSTN_ProjectileAim::Stick : EGSTN_ProjectileAim::CommandAim;
}

float UGSTN_GA_HeldThrow::GetShotLaunchPitchDeg() const
{
	// Shoulder: the converged aim already points at the crosshair, so only the setting's elevation is added (config, the same on every machine)
	if (DeferFire() && static_cast<EGSTN_HeldThrowAim>(ReleaseAim.Value) == EGSTN_HeldThrowAim::Shoulder)
	{
		return UGSTN_HeldSettings::Get().ThrowInput.ShoulderAimPitchDeg;
	}
	return Super::GetShotLaunchPitchDeg();
}

void UGSTN_GA_HeldThrow::OnGhostSpawned(AGSTN_Projectile& Ghost)
{
	// M10 G8: the thrower sees the object leave the hand at once, drawn at the ghost until the host's projectile arrives
	if (UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()))
	{
		Held->NotePredictedThrow(GetAvatarActorFromActorInfo(), Ghost);
	}
}

void UGSTN_GA_HeldThrow::ShowAimReticle(bool bShown) const
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		if (UGSTN_PrecisionAimComponent* Reticle = Pawn->FindComponentByClass<UGSTN_PrecisionAimComponent>())
		{
			const FGSTN_HeldThrowInput& Input = UGSTN_HeldSettings::Get().ThrowInput;
			Reticle->SetHoldAimShownWithView(bShown, Input.AimView == EGSTN_HeldAimView::Shoulder ? Input.AimViewRequestTags : FGameplayTagContainer());
		}
	}
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
	// M10 G6: a heavier class throws slower (TEST VALUES in the size classes)
	if (const FGSTN_HeldSizeClass* SizeClass = UGSTN_HeldSettings::Get().FindSizeClass(Object->GetTuning().SizeTag))
	{
		InOutParams.Speed *= SizeClass->ThrowSpeedScale;
	}
	// Its hits' events say a thrown object made them (M10 G3)
	InOutParams.SourceTag = GSTNCombatHitTags::Source_Thrown;
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

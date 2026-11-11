// Held objects, world side (needs the GAS-NPP fork).

#include "GSTN_HeldAbilities.h"
#include "Hits/GSTN_CombatHitTags.h"

#include "AbilitySystemComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTN_HeldSettings.h"
#include "Presentation/GSTN_PrecisionAimComponent.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "GSTN_HeldObjectComponent.h"
#include "GSTN_HeldSubsystem.h"
#include "GSTN_HeldTags.h"

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
	WindupClock.Value = INDEX_NONE;
	WindupFrames.Value = 0;
}

void UGSTN_GA_HeldThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	PressClock.Value = INDEX_NONE;
	ReleaseAim.Value = static_cast<uint8>(EGSTN_HeldThrowAim::Stick);
	WindupClock.Value = INDEX_NONE;
	WindupFrames.Value = 0;
	// The parent captures the context, then asks DeferFire (always true here): nothing fires in the parent, the throw fires from BeginWindup
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}
	if (HasButton())
	{
		PressClock.Value = FMath::Max(GetCombatClock(), 0);
		return;
	}
	// No button (an event, the harness): no aiming, the wind-up starts now (instant = the shot in the activation frame, as before)
	BeginWindup();
}

bool UGSTN_GA_HeldThrow::DeferFire() const
{
	return true;
}

bool UGSTN_GA_HeldThrow::HasButton() const
{
	return GetActivationContext().Button != INDEX_NONE;
}

void UGSTN_GA_HeldThrow::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	if (!IsActive())
	{
		return;
	}
	if (WindupClock.Value != INDEX_NONE)
	{
		TickWindup();
		return;
	}
	if (PressClock.Value == INDEX_NONE)
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
	BeginWindup();
}

const FGSTN_HeldThrowWindup* UGSTN_GA_HeldThrow::FindWindup() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	const UGSTN_HeldObjectComponent* Object = Held ? Held->FindHeldBy(Avatar) : nullptr;
	const UGSTN_HeldSettings& Settings = UGSTN_HeldSettings::Get();
	float LoadRatio = 0.f;
	const bool bEnemy = !Object && UGSTN_HeldSubsystem::FindLiftedEnemy(Avatar, &LoadRatio, nullptr);
	return FGSTN_HeldRules::ResolveWindup(Object ? &Object->GetTuning() : nullptr, bEnemy ? &Settings.ThrownEnemies : nullptr, Settings.DefaultThrowWindup);
}

void UGSTN_GA_HeldThrow::BeginWindup()
{
	const FGSTN_HeldThrowWindup* Windup = FindWindup();
	if (!Windup || Windup->IsInstant())
	{
		ShowAimReticle(false);
		Fire();
		FinishFire();
		return;
	}
	WindupClock.Value = FMath::Max(GetCombatClock(), 0);
	WindupFrames.Value = Windup->ReleaseFrames;
	// A camera aim keeps its reticle (and the shoulder view) until the object leaves: the camera of the release frame aims it
	ShowAimReticle(HasButton() && static_cast<EGSTN_HeldThrowAim>(ReleaseAim.Value) != EGSTN_HeldThrowAim::Stick);
	// Presentation: the placeholder montage is loaded at its first wind-up
	WindupMontage = Windup->Montage.IsNull() ? nullptr : Windup->Montage.LoadSynchronous();
	if (UNpAbilitySystemComponent* AbilitySystem = WindupMontage ? GetNpAbilitySystemComponent() : nullptr)
	{
		AbilitySystem->PlaySyncedMontage(WindupMontage, 0.f, Windup->MontagePlayRate, NAME_None, Windup->MontageRootMotionScale);
	}
	LogWindup(TEXT("start"));
}

void UGSTN_GA_HeldThrow::TickWindup()
{
	if (IsWindupInterrupted())
	{
		LogWindup(TEXT("stop"));
		StopWindupMontage();
		WindupClock.Value = INDEX_NONE;
		ShowAimReticle(false);
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}
	if (!FGSTN_HeldRules::IsWindupReleased(WindupClock.Value, GetCombatClock(), WindupFrames.Value))
	{
		return;
	}
	LogWindup(TEXT("release"));
	// The montage plays on into its follow-through: only a stopped wind-up cuts it
	WindupClock.Value = INDEX_NONE;
	ShowAimReticle(false);
	Fire();
	FinishFire();
}

bool UGSTN_GA_HeldThrow::IsWindupInterrupted() const
{
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem)
	{
		return false;
	}
	const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	const UGSTN_HeldObjectComponent* Object = Held ? Held->FindHeldBy(GetAvatarActorFromActorInfo()) : nullptr;
	const FGSTN_HeldBreakRules& Rules = Object ? Object->GetBreakRules() : UGSTN_HeldSettings::Get().DefaultBreakRules;
	FGameplayTagContainer Owned;
	AbilitySystem->GetOwnedGameplayTags(Owned);
	return FGSTN_HeldRules::BreaksOnState(Owned, Rules);
}

void UGSTN_GA_HeldThrow::StopWindupMontage()
{
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	float Time = 0.f;
	if (WindupMontage && AbilitySystem && AbilitySystem->GetPlayingSyncedMontage(Time) == WindupMontage)
	{
		AbilitySystem->StopSyncedMontage(WindupMontage, true);
	}
}

void UGSTN_GA_HeldThrow::LogWindup(const TCHAR* Phase) const
{
	const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	if (Held && Held->HasAuthority())
	{
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldWindup phase=%s thrower=%s start=%d clock=%d frames=%d montage=%s"), Phase,
			*GetNameSafe(GetAvatarActorFromActorInfo()), WindupClock.Value, GetCombatClock(), WindupFrames.Value, *GetNameSafe(WindupMontage));
	}
}

void UGSTN_GA_HeldThrow::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Cancelled while aiming or winding up (a hit, a forced move): no throw, the reticle goes away, a wind-up montage stops; a hit also breaks
	// the hold on the host
	if (WindupClock.Value != INDEX_NONE)
	{
		StopWindupMontage();
		WindupClock.Value = INDEX_NONE;
	}
	PressClock.Value = INDEX_NONE;
	ShowAimReticle(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

EGSTN_ProjectileAim UGSTN_GA_HeldThrow::GetShotAim() const
{
	if (Aim == EGSTN_ProjectileAim::EventTarget || !HasButton())
	{
		return Aim;
	}
	return static_cast<EGSTN_HeldThrowAim>(ReleaseAim.Value) == EGSTN_HeldThrowAim::Stick ? EGSTN_ProjectileAim::Stick : EGSTN_ProjectileAim::CommandAim;
}

float UGSTN_GA_HeldThrow::GetShotLaunchPitchDeg() const
{
	// Shoulder: the converged aim already points at the crosshair, so only the setting's elevation is added (config, the same on every machine)
	if (HasButton() && static_cast<EGSTN_HeldThrowAim>(ReleaseAim.Value) == EGSTN_HeldThrowAim::Shoulder)
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
		// M10 G9: a lifted enemy is thrown the same way; its hitbox is the shot, its body leaves the grab with the shot's velocity (host)
		float LoadRatio = 0.f;
		FString WhyNot;
		if (UGSTN_HeldSubsystem::FindLiftedEnemy(GetAvatarActorFromActorInfo(), &LoadRatio, bHost ? &WhyNot : nullptr))
		{
			const FGSTN_EnemyThrowTuning& Enemy = UGSTN_HeldSettings::Get().ThrownEnemies;
			InOutParams = Enemy.Throw;
			InOutParams.Speed *= FGSTN_EnemyLiftRules::SpeedScale(Enemy, LoadRatio);
			InOutParams.SourceTag = GSTNHeldTags::Source_ThrownEnemy;
			// The thrown body is seeded as already hit by its own shot (NoteEnemyThrown): one more so MaxHits still counts the targets
			InOutParams.MaxHits = static_cast<uint8>(FMath::Min(static_cast<int32>(InOutParams.MaxHits) + 1, 255));
			return true;
		}
		if (bHost)
		{
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldThrow: %s holds nothing, no throw (%s)"), *GetNameSafe(GetAvatarActorFromActorInfo()), *WhyNot);
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
		if (Held->FindHeldBy(GetAvatarActorFromActorInfo()))
		{
			Held->NoteThrown(GetAvatarActorFromActorInfo(), Shot);
		}
		else
		{
			Held->NoteEnemyThrown(GetAvatarActorFromActorInfo(), Shot);
		}
	}
}

UGSTN_GA_HeldThrowServer::UGSTN_GA_HeldThrowServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

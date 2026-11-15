// Projectile, world side (needs the GAS-NPP fork).

#include "GSTN_GA_FireProjectile.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Engine/World.h"
#include "GSTNProjectileCoreModule.h"
#include "GSTN_ProjectileActor.h"
#include "GSTN_ProjectileSubsystem.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

UGSTN_GA_FireProjectile::UGSTN_GA_FireProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TargetEntityId.Value = INDEX_NONE;
	Projectile.Shape.Type = EGSTN_ShapeType::Sphere;
	Projectile.Shape.Extent = FVector(15.0);
	Projectile.Hit.Damage = 8.f;
	Projectile.Hit.BaseKnockback = 30.f;
	Projectile.Hit.AngleDeg = 30.f;
}

UGSTN_GA_FireProjectileServer::UGSTN_GA_FireProjectileServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

UGSTN_GA_AimedHitscanShot::UGSTN_GA_AimedHitscanShot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Delivery = EGSTN_ShotDelivery::Hitscan;
	Aim = EGSTN_ProjectileAim::Reticle;
	ReticleOffAim = EGSTN_ProjectileAim::Facing;
	// 5 combat frames (80 ms at 62.5 Hz): the press reads as a trigger pull, not an instant beam
	FireDelayFrames = 5;
	Hitscan.RangeCm = 5000.f;
	Hitscan.RadiusCm = 5.f;
	Projectile.Hit.Damage = 6.f;
	Projectile.Hit.BaseKnockback = 20.f;
	Projectile.Hit.AngleDeg = 20.f;
}

void UGSTN_GA_FireProjectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The event payload exists only in the activation call: keep the target as its entity id (an int, rolled back with the ability)
	TargetEntityId.Value = INDEX_NONE;
	if (Aim == EGSTN_ProjectileAim::EventTarget && TriggerEventData && TriggerEventData->Target)
	{
		if (const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld()))
		{
			TargetEntityId.Value = Projectiles->FindEntityIdOf(TriggerEventData->Target.Get());
		}
	}

	if (DeferFire())
	{
		return;
	}
	if (FireDelayFrames <= 0)
	{
		Fire();
		FinishFire();
		return;
	}
	WaitCombatFrames(FireDelayFrames);
}

void UGSTN_GA_FireProjectile::OnCombatFramesElapsed()
{
	Super::OnCombatFramesElapsed();
	Fire();
	FinishFire();
}

void UGSTN_GA_FireProjectile::FinishFire()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

FGSTN_ProjectileShotSpec UGSTN_GA_FireProjectile::MakeShotSpec() const
{
	FGSTN_ProjectileShotSpec Spec;
	Spec.Projectile = Projectile;
	Spec.MuzzleOffset = MuzzleOffset;
	Spec.Delivery = Delivery;
	Spec.Hitscan = Hitscan;
	Spec.Aim = GetShotAim();
	Spec.LaunchPitchDeg = GetShotLaunchPitchDeg();
	Spec.StickAimDeadzone = StickAimDeadzone;
	Spec.bHighArc = bHighArc;
	Spec.TargetAimOffset = TargetAimOffset;
	Spec.ReticleOffAim = ReticleOffAim;
	Spec.MinReticlePitchDeg = MinReticlePitchDeg;
	Spec.MaxReticlePitchDeg = MaxReticlePitchDeg;
	return Spec;
}

bool UGSTN_GA_FireProjectile::Fire()
{
	// The builder is shared with the move actions (CP2); the hooks of this class keep their order: the template first, then the aim
	const FGSTN_ProjectileShot::FResult Result = FGSTN_ProjectileShot::Fire(MakeShotSpec(), *this, TargetEntityId.Value, FGSTN_ProjectileShotAdjust(),
		[this](FGSTN_ProjectileParams& InOutParams, bool bHost) { return PrepareShot(InOutParams, bHost); });
	if (Result.Ghost)
	{
		OnGhostSpawned(*Result.Ghost);
	}
	if (Result.Shot)
	{
		OnShotFired(*Result.Shot);
	}
	return Result.Shot != nullptr;
}

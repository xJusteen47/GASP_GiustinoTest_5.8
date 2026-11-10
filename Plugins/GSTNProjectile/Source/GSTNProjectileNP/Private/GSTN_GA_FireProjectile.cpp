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

bool UGSTN_GA_FireProjectile::Fire()
{
	UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Projectiles || !Avatar || Projectiles->GetLastFrame() == INDEX_NONE)
	{
		return false;
	}
	// A client never spawns the projectile: the host's spawn is the only one, and it reaches the client once. The client that predicts
	// its own shot draws the same shot as a ghost instead (once per frame, so a resimulation does not add one), adopted on arrival
	const bool bHost = Projectiles->CanFire();
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!bHost && !(Info && Info->IsLocallyControlled()))
	{
		return false;
	}

	FGSTN_ProjectileParams Params = Projectile;
	if (!PrepareShot(Params, bHost))
	{
		return false;
	}
	Params.OwnerEntityId = Projectiles->FindEntityIdOf(Avatar);

	// The pose hits are judged against: the owner's entry in the combat snapshot; the actor only when it has no combat entity
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	const FGSTN_SnapshotEntry* OwnerEntry = Combat && Params.OwnerEntityId != INDEX_NONE ? Combat->GetSnapshot().Find(Params.OwnerEntityId) : nullptr;
	const FTransform OwnerTransform = OwnerEntry ? OwnerEntry->Transform : Avatar->GetActorTransform();
	Params.Origin = OwnerTransform.TransformPosition(MuzzleOffset);

	FVector Direction = OwnerTransform.GetUnitAxis(EAxis::X);
	const EGSTN_ProjectileAim ShotAim = GetShotAim();
	const FGSTN_SnapshotEntry* TargetEntry = ShotAim == EGSTN_ProjectileAim::EventTarget && Combat && TargetEntityId.Value != INDEX_NONE
		? Combat->GetSnapshot().Find(TargetEntityId.Value) : nullptr;
	if (TargetEntry)
	{
		const FVector TargetPoint = TargetEntry->Transform.TransformPosition(TargetAimOffset);
		if (!FGSTN_BallisticMode::SolveLaunchDirection(Params.Origin, TargetPoint, Params.Speed, Params.GravityAccel, bHighArc, Direction))
		{
			// Out of reach: the farthest throw towards it (45 degrees against gravity in the target's direction)
			const FVector Up = Params.GravityAccel.IsNearlyZero() ? FVector::UpVector : -Params.GravityAccel.GetSafeNormal();
			const FVector Delta = TargetPoint - Params.Origin;
			const FVector Across = (Delta - Up * (Delta | Up)).GetSafeNormal(UE_SMALL_NUMBER, Direction);
			Direction = (Across + Up).GetSafeNormal();
			UE_LOG(LogGSTNProjectile, Log, TEXT("FireProjectile ability: target %d out of reach at %.0f cm/s, farthest throw instead"), TargetEntityId.Value, Params.Speed);
		}
	}
	else
	{
		if (ShotAim == EGSTN_ProjectileAim::CommandAim)
		{
			const FRotator AimRotation = GetAimRotation();
			if (!AimRotation.IsNearlyZero())
			{
				Direction = AimRotation.Vector();
			}
		}
		else if (ShotAim == EGSTN_ProjectileAim::Stick)
		{
			// The frame's stick on the shooter's plane (its snapshot up), as the stick moves the character; centred = the facing
			const UGSTN_CombatSimComponent* CombatComponent = Avatar->FindComponentByClass<UGSTN_CombatSimComponent>();
			if (CombatComponent)
			{
				FVector2D Stick;
				float CameraYaw = 0.f;
				CombatComponent->GetFrameStick(Stick, CameraYaw);
				const FVector OnPlane = FGSTN_CombatVictim::StickToPlane(Stick, CameraYaw, OwnerTransform.GetUnitAxis(EAxis::Z));
				if (OnPlane.SizeSquared() > FMath::Square(StickAimDeadzone))
				{
					Direction = OnPlane.GetSafeNormal();
				}
			}
		}
		FRotator Launch = Direction.Rotation();
		Launch.Pitch = FMath::Clamp(Launch.Pitch + LaunchPitchDeg, -89.0, 89.0);
		Direction = Launch.Vector();
	}
	Params.Direction = Direction;
	if (!bHost)
	{
		Projectiles->SpawnGhost(Params, Avatar);
		return false;
	}
	AGSTN_Projectile* Shot = Projectiles->FireProjectileAtFrame(Params, INDEX_NONE, Avatar);
	if (Shot)
	{
		OnShotFired(*Shot);
	}
	return Shot != nullptr;
}

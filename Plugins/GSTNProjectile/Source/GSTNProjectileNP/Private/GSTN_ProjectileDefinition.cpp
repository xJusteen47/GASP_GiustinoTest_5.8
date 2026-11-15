// Projectile, world side (needs the GAS-NPP fork).

#include "GSTN_ProjectileDefinition.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/World.h"
#include "GSTNProjectileCoreModule.h"
#include "GSTN_ProjectileActor.h"
#include "GSTN_ProjectileSubsystem.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

#define LOCTEXT_NAMESPACE "GSTNProjectileDefinition"

FGSTN_ProjectileShot::FResult FGSTN_ProjectileShot::Fire(const FGSTN_ProjectileShotSpec& Spec, const UGSTN_GA_Base& Ability, int32 TargetEntityId,
	const FGSTN_ProjectileShotAdjust& Adjust, TFunctionRef<bool(FGSTN_ProjectileParams&, bool)> Prepare)
{
	FResult Result;
	UWorld* World = Ability.GetWorld();
	UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(World);
	AActor* Avatar = Ability.GetAvatarActorFromActorInfo();
	if (!Projectiles || !Avatar || Projectiles->GetLastFrame() == INDEX_NONE)
	{
		return Result;
	}
	// A client never spawns the projectile: the host's spawn is the only one, and it reaches the client once. The client that predicts
	// its own shot draws the same shot as a ghost instead (once per frame, so a resimulation does not add one), adopted on arrival
	const bool bHost = Projectiles->CanFire();
	const FGameplayAbilityActorInfo* Info = Ability.GetCurrentActorInfo();
	if (!bHost && !(Info && Info->IsLocallyControlled()))
	{
		return Result;
	}

	FGSTN_ProjectileParams Params = Spec.Projectile;
	if (!Prepare(Params, bHost))
	{
		return Result;
	}
	Params.Speed *= FMath::Max(Adjust.SpeedScale, 0.01f);
	Params.OwnerEntityId = Projectiles->FindEntityIdOf(Avatar);

	// The pose hits are judged against: the owner's entry in the combat snapshot; the actor only when it has no combat entity
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
	const FGSTN_SnapshotEntry* OwnerEntry = Combat && Params.OwnerEntityId != INDEX_NONE ? Combat->GetSnapshot().Find(Params.OwnerEntityId) : nullptr;
	const FTransform OwnerTransform = OwnerEntry ? OwnerEntry->Transform : Avatar->GetActorTransform();
	Params.Origin = OwnerTransform.TransformPosition(Spec.MuzzleOffset);

	FVector Direction = OwnerTransform.GetUnitAxis(EAxis::X);
	const EGSTN_ProjectileAim ShotAim = Spec.Aim;
	const FGSTN_SnapshotEntry* TargetEntry = ShotAim == EGSTN_ProjectileAim::EventTarget && Combat && TargetEntityId != INDEX_NONE
		? Combat->GetSnapshot().Find(TargetEntityId) : nullptr;
	if (TargetEntry)
	{
		const FVector TargetPoint = TargetEntry->Transform.TransformPosition(Spec.TargetAimOffset);
		if (!FGSTN_BallisticMode::SolveLaunchDirection(Params.Origin, TargetPoint, Params.Speed, Params.GravityAccel, Spec.bHighArc, Direction))
		{
			// Out of reach: the farthest throw towards it (45 degrees against gravity in the target's direction)
			const FVector Up = Params.GravityAccel.IsNearlyZero() ? FVector::UpVector : -Params.GravityAccel.GetSafeNormal();
			const FVector Delta = TargetPoint - Params.Origin;
			const FVector Across = (Delta - Up * (Delta | Up)).GetSafeNormal(UE_SMALL_NUMBER, Direction);
			Direction = (Across + Up).GetSafeNormal();
			UE_LOG(LogGSTNProjectile, Log, TEXT("FireProjectile: target %d out of reach at %.0f cm/s, farthest throw instead"), TargetEntityId, Params.Speed);
		}
	}
	else
	{
		// Reticle (M11 L9): the fire frame's reticle bit picks the 3D reticle aim or the flat fallback; the bit is in the predicted command, so
		// the host and the predicting client choose alike
		const UGSTN_CombatSimComponent* AimComponent = Avatar->FindComponentByClass<UGSTN_CombatSimComponent>();
		const bool bReticleShot = ShotAim == EGSTN_ProjectileAim::Reticle;
		const bool bReticleOn = bReticleShot && AimComponent && AimComponent->IsPrecisionAimFrame();
		EGSTN_ProjectileAim FlatAim = ShotAim;
		if (bReticleShot)
		{
			FlatAim = Spec.ReticleOffAim == EGSTN_ProjectileAim::Stick ? EGSTN_ProjectileAim::Stick : EGSTN_ProjectileAim::Facing;
		}
		if (FlatAim == EGSTN_ProjectileAim::CommandAim)
		{
			const FRotator AimRotation = Ability.GetAimRotation();
			if (!AimRotation.IsNearlyZero())
			{
				Direction = AimRotation.Vector();
			}
		}
		else if (FlatAim == EGSTN_ProjectileAim::Stick && !bReticleOn)
		{
			// The frame's stick on the shooter's plane (its snapshot up), as the stick moves the character; centred = the facing
			if (AimComponent)
			{
				FVector2D Stick;
				float CameraYaw = 0.f;
				AimComponent->GetFrameStick(Stick, CameraYaw);
				const FVector OnPlane = FGSTN_CombatVictim::StickToPlane(Stick, CameraYaw, OwnerTransform.GetUnitAxis(EAxis::Z));
				if (OnPlane.SizeSquared() > FMath::Square(Spec.StickAimDeadzone))
				{
					Direction = OnPlane.GetSafeNormal();
				}
			}
		}
		Direction = FGSTN_ShotAim::ReticleDirection(bReticleOn, bReticleOn ? Ability.GetAimRotation() : FRotator::ZeroRotator, Direction,
			Spec.MinReticlePitchDeg, Spec.MaxReticlePitchDeg, Spec.LaunchPitchDeg);
	}
	// A move's own turn of this shot (CP2), around the shooter's up; none for the fire ability, whose direction stays bit for bit what it was
	if (!FMath::IsNearlyZero(Adjust.YawOffsetDeg) || !FMath::IsNearlyZero(Adjust.PitchOffsetDeg))
	{
		Direction = FGSTN_ShotAim::ApplyAngleOffset(Direction, OwnerTransform.GetUnitAxis(EAxis::Z), Adjust.YawOffsetDeg, Adjust.PitchOffsetDeg);
	}
	Params.Direction = Direction;
	if (Spec.Delivery == EGSTN_ShotDelivery::Hitscan)
	{
		FGSTN_ShotAim::MakeHitscan(Params, Spec.Hitscan);
	}
	if (!bHost)
	{
		Result.Ghost = Projectiles->SpawnGhost(Params, Avatar);
		return Result;
	}
	Result.Shot = Projectiles->FireProjectileAtFrame(Params, INDEX_NONE, Avatar);
	return Result;
}

bool FGSTN_MoveAction_FireProjectile::Execute(const FGSTN_MoveActionContext& Context) const
{
	return UGSTN_ProjectileMoveLibrary::FireMoveProjectile(Context.Ability, Definition, Adjust);
}

FText FGSTN_MoveAction_FireProjectile::Validate() const
{
	return Definition ? FText::GetEmpty() : LOCTEXT("FireNoDefinition", "Fire Projectile has no projectile definition: it fires nothing.");
}

bool UGSTN_ProjectileMoveLibrary::FireMoveProjectile(UGSTN_GA_BaseCombat* Move, UGSTN_ProjectileDefinition* Definition, FGSTN_ProjectileShotAdjust Adjust)
{
	// A request like every move action: only inside the simulation step of an active move (never in a restored frame or in presentation)
	if (!Move || !Definition || !Move->CanRunMoveActions())
	{
		return false;
	}
	// A move has no event target of its own: an EventTarget shot from a move goes along the facing (TargetEntityId INDEX_NONE)
	const FGSTN_ProjectileShot::FResult Result = FGSTN_ProjectileShot::Fire(Definition->Shot, *Move, INDEX_NONE, Adjust,
		[](FGSTN_ProjectileParams&, bool) { return true; });
	return Result.Shot != nullptr || Result.Ghost != nullptr;
}

#undef LOCTEXT_NAMESPACE

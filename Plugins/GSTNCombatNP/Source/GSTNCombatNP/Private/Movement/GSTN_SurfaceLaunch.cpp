#include "Movement/GSTN_SurfaceLaunch.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceUtils.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"

FVector FGSTN_SurfaceLaunch::ComposeVelocity(const FGSTN_SurfaceLaunchParams& Params, const FTransform& SurfaceTransform, const FVector& ContactNormal,
	const FVector& CurrentVelocity, const FVector& SurfaceVelocity)
{
	const FVector Normal = ContactNormal.GetSafeNormal(UE_SMALL_NUMBER, SurfaceTransform.GetRotation().GetUpVector());
	const FVector Direction = UGSTN_SurfaceUtils::ComputeLaunchDirection(Params, SurfaceTransform, Normal);
	const double Incoming = FMath::Max(0.0, -CurrentVelocity.Dot(Normal));
	FVector Out = Direction * UGSTN_SurfaceUtils::ComputeLaunchSpeed(Params, static_cast<float>(Incoming));
	if (Params.bKeepTangentialVelocity)
	{
		// The part along the face the combatant touched; the part into the face is what the launch replaces
		Out += CurrentVelocity - Normal * CurrentVelocity.Dot(Normal);
	}
	if (Params.bAddSurfaceVelocity)
	{
		Out += SurfaceVelocity;
	}
	return Out;
}

bool FGSTN_SurfaceLaunch::Probe(const FMoverSyncState& SyncState, const UMoverComponent& Mover, const FVector& Up, double StepSeconds, FGSTN_SurfaceLaunchDecision& Out)
{
	const FMoverDefaultSyncState* Sync = SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Mover.GetUpdatedComponent());
	UWorld* World = Primitive ? Primitive->GetWorld() : nullptr;
	if (!Sync || !World || StepSeconds <= 0.0)
	{
		return false;
	}

	// The start pose is the tracked sync state's, not the component's, so a resimulated frame sweeps from where the original did.
	// The margin along gravity makes a combatant standing on the surface register it as well as one that falls onto it
	const FVector Velocity = Sync->GetVelocity_WorldSpace();
	const FVector Start = Sync->GetLocation_WorldSpace();
	const FVector End = Start + Velocity * StepSeconds - Up * ProbeMarginCm;
	if ((End - Start).SquaredLength() < 1.0)
	{
		return false;
	}

	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNSurfaceLaunch), false, Mover.GetOwner());
	Query.bReturnPhysicalMaterial = true;
	FCollisionResponseParams Response;
	Primitive->InitSweepCollisionParams(Query, Response);
	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, Start, End, Sync->GetOrientation_WorldSpace().Quaternion(), Primitive->GetCollisionObjectType(), Primitive->GetCollisionShape(), Query, Response)
		|| !Hit.bBlockingHit || Hit.bStartPenetrating)
	{
		return false;
	}

	const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::Resolve(Hit);
	if (!View.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch))
	{
		return false;
	}

	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const double Along = Velocity.Dot(Normal);
	if (Normal.IsNearlyZero() || Along > MovingAwayToleranceCmPerSec)
	{
		return false;
	}

	const UPrimitiveComponent* Collider = Hit.GetComponent();
	const FTransform SurfaceTransform = Collider ? Collider->GetComponentTransform() : FTransform::Identity;
	Out.bLaunch = true;
	Out.Velocity = ComposeVelocity(View.Launch, SurfaceTransform, Normal, Velocity, View.SurfaceVelocity);
	Out.bResetAirJumps = View.Launch.bResetAirJumps;
	Out.bResetCoyote = View.Launch.bResetCoyote;
	Out.LaunchTag = View.Launch.LaunchTag;
	Out.SurfaceTypeIndex = View.SurfaceTypeIndex;
	Out.IncomingSpeed = FMath::Max(0.0, -Along);
	Out.ColliderName = Collider ? Collider->GetFName() : NAME_None;
	return true;
}

FGSTN_JumpResetRules FGSTN_SurfaceLaunch::ResetRulesFor(const FGSTN_JumpResetRules& Kit, bool bResetAirJumps, bool bResetCoyote)
{
	FGSTN_JumpResetRules Out = Kit;
	Out.AirJumpsOnExternalLaunch = bResetAirJumps ? EGSTN_AirJumpsOnLaunch::Refill : EGSTN_AirJumpsOnLaunch::Keep;
	if (bResetCoyote)
	{
		// The jump step sets the window to its full length after the launch has been applied; Keep stops the rule from clearing it first
		Out.CoyoteOnExternalLaunch = EGSTN_CoyoteOnLaunch::Keep;
	}
	return Out;
}

// The surface launch of a non-tumbling combatant (M4 H11b): the contact probe and the launch math.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_SurfaceTypes.h"
#include "Jump/GSTN_JumpTypes.h"

class UMoverComponent;
struct FMoverSyncState;

/** What the probe decided for one frame */
struct GSTNCOMBATNP_API FGSTN_SurfaceLaunchDecision
{
	bool bLaunch = false;

	/** World velocity the launch gives (replaces the whole velocity) */
	FVector Velocity = FVector::ZeroVector;

	bool bResetAirJumps = true;
	bool bResetCoyote = false;

	/** Tag the surface names for kits; local data, never stored in a sync state */
	FGameplayTag LaunchTag;

	/** The surface type index of the surface (GSTNSurface table), 0 for none */
	uint8 SurfaceTypeIndex = 0;

	/** Speed into the face that the frame would have had, cm/s */
	double IncomingSpeed = 0.0;

	/** Name of the collider, for the harness */
	FName ColliderName;
};

/**
 * The launch a launching surface gives a combatant that is not tumbling (a tumbling one bounces by the impact table). Stateless: the probe
 * reads Mover's tracked sync state of the end of the previous frame (position, velocity, up) and the static world, like Mover's own movement
 * does, so a resimulated frame decides the same on every machine. Nothing is stored; the launch is queued as an instant effect whose layered
 * move (FGSTN_LayeredMove_Launch) is what Mover keeps in its sync state.
 */
struct GSTNCOMBATNP_API FGSTN_SurfaceLaunch
{
	/** How far past the frame's movement the sweep reaches along gravity, so a combatant resting on the surface registers it. Matches the knockback probe's margin */
	static constexpr double ProbeMarginCm = 2.0;

	/** Velocity into the surface that still counts as arriving: a combatant already moving away faster than this is not launched again */
	static constexpr double MovingAwayToleranceCmPerSec = 5.0;

	/**
	 * The velocity a launch gives. Direction from the surface's own frame (ComputeLaunchDirection), speed from the arrival speed into the
	 * touched face, plus the velocity along the face when bKeepTangentialVelocity and the surface's own velocity when bAddSurfaceVelocity.
	 * Pure: no world access
	 */
	static FVector ComposeVelocity(const FGSTN_SurfaceLaunchParams& Params, const FTransform& SurfaceTransform, const FVector& ContactNormal,
		const FVector& CurrentVelocity, const FVector& SurfaceVelocity);

	/**
	 * The reset rules of the jump state for one launch: the kit's rules with the launch's own flags laid over them. bResetAirJumps refills the air jumps
	 * (off: the count stays), bResetCoyote keeps the window the jump state restarts at full length (off: the kit's rule, which clears it by default).
	 * Pure, so the jump state stays a function of the sync state
	 */
	static FGSTN_JumpResetRules ResetRulesFor(const FGSTN_JumpResetRules& Kit, bool bResetAirJumps, bool bResetCoyote);

	/** The probe: sweeps the combatant's collision from its tracked pose and decides. False when nothing launches this frame */
	static bool Probe(const FMoverSyncState& SyncState, const UMoverComponent& Mover, const FVector& Up, double StepSeconds, FGSTN_SurfaceLaunchDecision& Out);
};

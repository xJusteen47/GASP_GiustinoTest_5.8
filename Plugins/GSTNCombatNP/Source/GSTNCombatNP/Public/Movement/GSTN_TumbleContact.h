// The contact record of a tumbling victim (combat-m4-hits 7b, M4 H11).

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GSTN_TumbleContact.generated.h"

/** The face a tumbling victim ran into, by its normal against the victim's up */
UENUM()
enum class EGSTN_TumbleContactKind : uint8
{
	None,
	/** A face turned away from up (a wall); the surface impacts of H11 resolve these */
	Wall,
	/** A face turned along up; landing is the end of the tumble, a surface impact only when the surface forces one (a trampoline) */
	Floor,
	/** A face turned against up */
	Ceiling
};

/**
 * The contact a launched victim made, written by the probe of its knockback move (FGSTN_LayeredMove_Knockback::ProbeContact) and read by
 * the combat tick one frame later (combat-m4-hits 7b, "the combat tick reads it at the end of F-1"). It lives in the layered move, which
 * Mover keeps in its sync state, so a rollback restores it with everything else and a resimulation writes it again.
 *
 * Everything in it is quantised, so equal contacts are equal bits on every machine: the normal in 1/32767 per axis, the impact speed in
 * whole cm/s. No tag and no pointer: the surface is its GSTNSurface type index (UGSTN_SurfaceUtils::GetSurfaceTypeIndex). M6's tech modifier
 * reads this same record; it does not write a second one.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_TumbleContactSyncState
{
	GENERATED_BODY()

	UPROPERTY()
	EGSTN_TumbleContactKind Kind = EGSTN_TumbleContactKind::None;

	UPROPERTY()
	int16 NormalQ[3] = { 0, 0, 0 };

	/** Speed into the face along its normal, whole cm/s, at most 65535 */
	UPROPERTY()
	uint16 ImpactSpeedQ = 0;

	/** The Mover frame whose step ran into the face (FMoverTimeStep::ServerFrame, the frame number of the combat tick as well) */
	UPROPERTY()
	int32 ContactFrame = INDEX_NONE;

	/** GSTNSurface type index of the surface (0 = no type), resolved from the hit at the time of the contact */
	UPROPERTY()
	uint8 SurfaceTypeIndex = 0;

	bool IsSet() const { return Kind != EGSTN_TumbleContactKind::None; }

	FVector GetNormal() const { return FVector(NormalQ[0], NormalQ[1], NormalQ[2]) / 32767.0; }

	/** The face kind of a normal against up: within 45 degrees of up is a floor, within 45 degrees of down a ceiling, the rest a wall */
	static EGSTN_TumbleContactKind Classify(const FVector& Normal, const FVector& Up);

	/**
	 * The record of a blocking hit made while moving with Velocity: the surface is resolved through UGSTN_SurfaceUtils (an IGSTN_SurfaceProvider
	 * on the collider or its actor, else the physical material mapping, else the default of the surface settings). A hit that is not blocking or
	 * moves away from its face gives an unset record
	 */
	static FGSTN_TumbleContactSyncState FromHit(const FHitResult& Hit, const FVector& Velocity, const FVector& Up, int32 Frame);

	void NetSerialize(FArchive& Ar);

	bool operator==(const FGSTN_TumbleContactSyncState& Other) const;
};

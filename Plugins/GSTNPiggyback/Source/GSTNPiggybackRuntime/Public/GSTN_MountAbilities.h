// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_MountAbilities.generated.h"

/**
 * The mount's three moves (combat-m7-piggyback.md 4 and 7), as timeline abilities whose frame data is built in the constructor, so the plugin
 * ships them without an asset. Blueprintable: a kit subclasses one in Blueprint for its own timing, animation and cues, or names its own class in
 * FGSTN_MountKitExtension. Geometry is relative to the mover's root facing +X.
 *
 * None of them sets a state: the mount is a hit that names the mount link type (GSTNLink's LinkRequest step forms it on the carrier, the rider
 * accepts optimistically at PendingFrames), the dismount and the drop are Release keys of a link outcome track (the role that plays it names the
 * reason: Dismount for the rider, Drop for the carrier). Every exit is placed by GSTNLink's watchdog on the authority (server-given, G5).
 */

/**
 * Command.Link.Mount (requires Interact.Target.Mount): 24 frames, actionable at 16. One interaction hitbox on frame 3 only (one request per move), sphere r 70 at 60 cm in
 * front: hit type Combat.HitType.Interaction.Mount, link type Combat.Link.Type.Mount, one target per execution, no hitstop. Its relationship
 * variants hit allies only: Ally is forced hittable (the match rules do not hit allies), Enemy and Neutral are ignored. No target in reach = the
 * move plays out and nothing happens.
 */
UCLASS(Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_Mount : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_Mount(const FObjectInitializer& ObjectInitializer);
};

/** Command.Link.Dismount (requires Combat.Link.Role.Rider): 12 frames, actionable at 8, a Release key on frame 1 (reason Dismount, the rider role's) */
UCLASS(Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_Dismount : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_Dismount(const FObjectInitializer& ObjectInitializer);
};

/** Command.Link.Drop (Interact + crouch held, requires Combat.Link.Role.Carrier): 14 frames, actionable at 10, a Release key on frame 2 (reason Drop) */
UCLASS(Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_Drop : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_Drop(const FObjectInitializer& ObjectInitializer);
};

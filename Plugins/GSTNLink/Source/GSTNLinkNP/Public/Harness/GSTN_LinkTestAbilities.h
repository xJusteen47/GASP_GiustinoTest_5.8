// Links, Network Prediction half: the harness's link moves.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_LinkTestAbilities.generated.h"

/**
 * The link moves of the network harness (F12, link_scenarios.py), as C++ timeline abilities whose frame data is built in the constructor, so the
 * plugin ships them without an asset and no shared Blueprint is edited. UGSTN_LinkContentLibrary::AddLinkTestBindings binds them, in memory, to a
 * kit with the standard link slots (UGSTN_LinkSlots). Geometry is relative to an attacker at the origin facing +X. Not for game content: a kit
 * authors its own moves with the same tracks.
 */

/** Command.Link.Grab: 24 frames, a grab hitbox (sphere r 60 at 90 cm, frames 4..7, hit type Grab, link type Combat.Link.Type.Grab) and an OnHit cancel window [6, 24) into Command.Link.* */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkTestGrab : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkTestGrab(const FObjectInitializer& ObjectInitializer);
};

/** Command.Link.Pummel: 18 frames, one hitbox (sphere r 130 at 40 cm, frames 3..5, hit type Pummel, LinkFilter LinkedPartnerOnly, damage 3, no reaction) */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkTestPummel : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkTestPummel(const FObjectInitializer& ObjectInitializer);
};

/** Command.Link.Throw.*: 30 frames, a link outcome key Throw on frame 8 (damage 8, knockback 1600 at 40 degrees, hit type Throw, not parryable) */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkTestThrow : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkTestThrow(const FObjectInitializer& ObjectInitializer);
};

/**
 * Command.Link.Release (any role): 10 frames, a link outcome key Release on frame 1. The role that plays it names the reason (FGSTN_LinkRoleDef::
 * ReleaseBreakReason): the grabber releases, the latcher lets go, the latched enemy shakes the latcher off
 */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkTestRelease : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkTestRelease(const FObjectInitializer& ObjectInitializer);
};

/** Command.Link.Latch.Attack: 20 frames, one hitbox (sphere r 160 around the root, frames 4..8, LinkFilter LinkedPartnerOnly, damage 5, no reaction) */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkTestLatchAttack : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkTestLatchAttack(const FObjectInitializer& ObjectInitializer);
};

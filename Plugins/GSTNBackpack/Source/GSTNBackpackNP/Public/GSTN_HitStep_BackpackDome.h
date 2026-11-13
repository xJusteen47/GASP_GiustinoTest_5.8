// Backpacks, Network Prediction half (M11 L7c: the dome blocks hits).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitStep_BackpackDome.generated.h"

/**
 * What the BackpackDome step hands on when a dome blocked a hit: the attacker outcome reads it (Ricochet), the event applier raises
 * Backpack.Event.DomeBlocked from it. No state is written for it: a blocked hit only did nothing
 */
USTRUCT()
struct GSTNBACKPACKNP_API FGSTN_BackpackDomeBlock : public FGSTN_HitExtension
{
	GENERATED_BODY()

	/** The entity holding the dome that blocked */
	UPROPERTY()
	int32 DomeOwnerId = INDEX_NONE;
};

/**
 * The BackpackDome step of the hit resolution pipeline (M11 L7c, owner 2026-11-13: the dome blocks EVERY hit from outside; a hit never counts
 * on both sides). Right after TeamFilter, so before Parry and Shield: a dome stops a hit before any defence of the victim sees it.
 *
 * Blocked when the hit landed on a Backpack-source extra hurtbox (the shell, which an attacker inside never touches), or when the victim's
 * snapshot says it stands in a dome the attacker's does not (no attacker extension = outside). Blocked = no damage, no knockback, no hitstun,
 * no flinch, no hitstop, bBlocked, a FGSTN_BackpackDomeBlock outcome extension; Stops. A melee attacker recoils through the attacker outcome
 * Backpack.Dome (Ricochet); a projectile is simply spent.
 *
 * Pure: the view in, the result out, the same on the attacker's prediction and the victim (both read the snapshot of the end of the previous frame).
 */
UCLASS(meta = (DisplayName = "Backpack Dome"))
class GSTNBACKPACKNP_API UGSTN_HitStep_BackpackDome : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;

	/** The dome that blocks this hit (its owner's entity id), INDEX_NONE when none: the shell's owner first, else the first dome holding the victim but not the attacker */
	static int32 FindBlockingDome(const FGSTN_HitContextView& View);
};

// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "GSTN_ImpactMath.generated.h"

/** Stateless resolution of a surface contact (combat-m4-hits 7b) */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_ImpactMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The outcome of a contact, three layers (attack, victim, surface):
	 *  - a row with bForceDefault is the physical truth: its default happens whatever the attack or the victim prefer;
	 *  - otherwise the attack's Force wins over the rest, unless it is also in Forbid, and only if the surface allows any outcome
	 *    at all or the profile has bForceIgnoresSurface;
	 *  - otherwise the surface's default, which Forbid (attack) and the victim's Allowed (and the surface's own Allowed) can remove;
	 *  - the speed (ImpactSpeedQ x SpeedScale) must exceed the larger of the surface's and the victim's MinImpactSpeed, else None.
	 * Pure and deterministic: the same inputs give the same bits. A missing table or an unknown surface index gives None.
	 */
	static FGSTN_ImpactResult Resolve(const FGSTN_ImpactInput& Input, const UGSTN_SurfaceImpactTable* Table, const FGSTN_ImpactResponse& Response, const FGSTN_ImpactProfile& Profile);
};

// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PartsContentLibrary.generated.h"

class UGSTN_CombatRules;
class UGSTN_PartSetDefinition;

/**
 * Editor-time helpers that author parts content from a script or a Blueprint utility (setup_gas_assets.py, an editor tool). They exist
 * because a polymorphic FInstancedStruct (a break effect) or a pipeline of soft step classes is awkward to edit from Python, and they
 * are idempotent: running one twice leaves the asset as running it once. None of them is used by the simulation.
 */
UCLASS()
class GSTNPARTSCORE_API UGSTN_PartsContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Makes a rules asset use parts: HealthWeight and ApplyDamage are removed (PartResolve replaces both, see UGSTN_HitStep_PartResolve) and
	 * PartResolve is inserted right before ComputeKnockback, or at the end when the pipeline has none. Does nothing to a pipeline that already
	 * has PartResolve. The pipeline is resolved afterwards. Returns true when PartResolve is in the pipeline at the end.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Content")
	static bool ConvertRulesToParts(UGSTN_CombatRules* Rules);

	/** Gives a part a DisableHitGroups break effect with this group mask, replacing the one it has. False for an invalid index */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Content")
	static bool SetBreakDisableHitGroups(UGSTN_PartSetDefinition* Set, int32 PartIndex, int32 GroupMask);

	/** Gives a part a MainDamage break effect (extra percent on the main pool, and whether it kills), replacing the one it has. False for an invalid index */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Content")
	static bool SetBreakMainDamage(UGSTN_PartSetDefinition* Set, int32 PartIndex, float AddPercent, bool bKill);

	/** Gives a part a Launch break effect (speed in cm/s, share of it along the entity's up), replacing the one it has. False for an invalid index */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Content")
	static bool SetBreakLaunch(UGSTN_PartSetDefinition* Set, int32 PartIndex, float Speed, float UpFraction);

	/** Gives a part a Stagger break effect (minimum hitstun frames), replacing the one it has. False for an invalid index */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Content")
	static bool SetBreakStagger(UGSTN_PartSetDefinition* Set, int32 PartIndex, int32 Frames);
};

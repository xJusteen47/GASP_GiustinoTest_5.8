// Backpacks, Network Prediction half (M11 L7c).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_BackpackContentLibrary.generated.h"

class UGSTN_CombatRules;

/**
 * Editor-time helpers of the backpack plugin (Tools/GAS/create_backpack_assets.py): the hit pipeline is an array of soft classes that is
 * awkward to edit from Python. Idempotent; none of it is used by the simulation.
 */
UCLASS()
class GSTNBACKPACKNP_API UGSTN_BackpackContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Puts the BackpackDome step into a combat rules pipeline right after TeamFilter (first when the pipeline has none), so before Parry and
	 * Shield. Idempotent (a rules asset that holds it is left untouched and not dirtied); resolves the pipeline again. Returns whether the
	 * rules now hold the step
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Backpack|Content")
	static bool AddDomeStepToRules(UGSTN_CombatRules* Rules);

	/** Removes the BackpackDome step again; true when one was removed */
	UFUNCTION(BlueprintCallable, Category = "Combat|Backpack|Content")
	static bool RemoveDomeStepFromRules(UGSTN_CombatRules* Rules);
};

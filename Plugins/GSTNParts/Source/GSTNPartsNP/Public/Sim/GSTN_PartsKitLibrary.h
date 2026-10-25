// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PartsKitLibrary.generated.h"

class UGSTN_PartSetDefinition;
class UGSTN_PrimaryKitDefinition;

/**
 * Editor-time helper that gives a kit a part set (setup_gas_assets.py, an editor tool): the FGSTN_PartsKitExtension entry of the kit's
 * extension array is awkward to edit from Python. Idempotent; not used by the simulation.
 */
UCLASS()
class GSTNPARTSNP_API UGSTN_PartsKitLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Sets (adds or replaces) the part set of the kit's FGSTN_PartsKitExtension; a null Set removes the entry. False for a null kit */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Content")
	static bool SetKitPartSet(UGSTN_PrimaryKitDefinition* Kit, UGSTN_PartSetDefinition* Set);
};

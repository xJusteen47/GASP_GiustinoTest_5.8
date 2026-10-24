// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_HitExtensions.generated.h"

/**
 * Base of the data an optional plugin attaches to a hit (FGSTN_HitData::ExtensionData, modularity-and-tags 5b). Core never
 * reads an extension: it only carries it, and lets a relationship variant replace one by its struct type. The appliers and
 * the registry arrive with the pipeline (H4).
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNCOMBATCORE_API FGSTN_HitExtension
{
	GENERATED_BODY()

	virtual ~FGSTN_HitExtension() = default;
};

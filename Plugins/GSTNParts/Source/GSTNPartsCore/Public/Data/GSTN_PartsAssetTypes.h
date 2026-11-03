// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

/**
 * Primary asset types of the parts plugin. A part set returns one from GetPrimaryAssetId so a game can find and swap sets by id.
 * A project registers the type under PrimaryAssetTypesToScan if it wants the asset manager to scan it (a project decision: the
 * plugin does not edit the project's DefaultGame.ini).
 */
namespace GSTNPartsAssetTypes
{
	inline FPrimaryAssetType PartSet() { return FPrimaryAssetType(TEXT("GSTN_PartSet")); }
}

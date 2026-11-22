// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

/**
 * Primary asset types of the kit data layer. Loadout data stores asset ids of these types, and each definition asset returns
 * one from GetPrimaryAssetId, so the names live in one place. A project registers the types it uses under
 * PrimaryAssetTypesToScan (a project decision: the plugin does not edit the project's DefaultGame.ini).
 */
namespace GSTNCombatAssetTypes
{
	inline FPrimaryAssetType PrimaryKit() { return FPrimaryAssetType(TEXT("GSTN_PrimaryKit")); }
	inline FPrimaryAssetType Weapon() { return FPrimaryAssetType(TEXT("GSTN_Weapon")); }
	inline FPrimaryAssetType Super() { return FPrimaryAssetType(TEXT("GSTN_Super")); }
	inline FPrimaryAssetType SuperInputScheme() { return FPrimaryAssetType(TEXT("GSTN_SuperInputScheme")); }
	inline FPrimaryAssetType Skin() { return FPrimaryAssetType(TEXT("GSTN_Skin")); }

	/** Most supers a kit can have (SuperSlotCount is 0 to this) */
	constexpr int32 MaxSuperSlots = 8;
}

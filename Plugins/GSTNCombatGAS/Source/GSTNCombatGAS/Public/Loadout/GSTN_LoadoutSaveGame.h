// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "GSTN_LoadoutSaveGame.generated.h"

/**
 * The loadouts a local player saved, by name. Holds ids only (FGSTN_LoadoutData), so a content change cannot break the file: ids
 * that no longer exist are dropped when a loadout is read back, not when the file is read. FGSTN_LoadoutData writes itself with its
 * own versioned format, so this object only needs its own version for the map around it.
 */
UCLASS(BlueprintType)
class GSTNCOMBATGAS_API UGSTN_LoadoutSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Version of this object's own layout */
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Loadout")
	int32 SaveVersion = CurrentVersion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Loadout")
	TMap<FString, FGSTN_LoadoutData> Loadouts;

	/** The object as bytes, the form the subsystem stores. False if the engine could not serialize it */
	bool ToBytes(TArray<uint8>& OutBytes);

	/** Reads bytes written by ToBytes. Null if they are not a loadout save game */
	static UGSTN_LoadoutSaveGame* FromBytes(const TArray<uint8>& Bytes);

	/**
	 * Removes from Data every asset id IsKnown refuses (a kit, weapon, super or skin that was deleted or renamed), each with a
	 * Warning that names Context. A dropped kit or skin clears the field, a dropped super empties its slot (the slot positions
	 * mean something), a dropped additional weapon leaves the list and the customisations of the weapons after it follow their
	 * new index; a customisation of a dropped weapon is dropped. Returns how many things were dropped. Not an error: a saved loadout
	 * outliving a piece of content is expected.
	 */
	static int32 DropUnknownAssets(FGSTN_LoadoutData& Data, TFunctionRef<bool(const FPrimaryAssetId&)> IsKnown, const FString& Context);
};

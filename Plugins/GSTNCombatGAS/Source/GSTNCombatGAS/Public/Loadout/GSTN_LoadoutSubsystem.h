// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "GSTN_LoadoutSubsystem.generated.h"

class UGSTN_LoadoutComponent;
class UGSTN_LoadoutSaveGame;

/**
 * The loadouts a local player keeps between sessions (combat-m3 10): named presets in a save slot of the local user. Client side
 * only; it never decides anything for the match, the server validates whatever the player sends (UGSTN_LoadoutComponent).
 *
 * The file holds ids, so content can change under it. Reading a preset back checks every id with the Asset Manager and drops the
 * ones that no longer exist with a Warning (UGSTN_LoadoutSaveGame::DropUnknownAssets); the file itself is left as saved, so a piece
 * of content that comes back restores the preset. A dropped kit leaves a loadout the server's rules will refuse, which is the
 * right outcome: the UI shows the finding and the player picks again.
 *
 * Presets load lazily on first use, so the subsystem works the moment it exists (and in a test that builds one by hand). The
 * storage is two virtual functions, so a game with its own save backend (cloud, a profile service) replaces just those.
 */
UCLASS(BlueprintType)
class GSTNCOMBATGAS_API UGSTN_LoadoutSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Name of the save slot all presets of a local user share */
	static const TCHAR* GetSaveSlotName() { return TEXT("GSTNCombatLoadouts"); }

	/** Stores Data under PresetName (replacing a preset of that name) and writes the file. False for an empty name or a failed write */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool SaveLoadout(const FString& PresetName, const FGSTN_LoadoutData& Data);

	/**
	 * Reads a preset. Ids of assets that no longer exist are dropped with a Warning. False (OutData untouched) if there is no
	 * such preset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool LoadLoadout(const FString& PresetName, FGSTN_LoadoutData& OutData);

	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool DeleteLoadout(const FString& PresetName);

	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool HasLoadout(const FString& PresetName);

	/** The names of the saved presets, sorted */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	TArray<FString> ListLoadouts();

	/** Loads PresetName and sends it to the server through Component (the player's own loadout component). False if there is no such preset */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool RequestLoadout(const FString& PresetName, UGSTN_LoadoutComponent* Component);

protected:
	/** Whether an asset with this id still exists. Default: the Asset Manager knows the id */
	virtual bool IsAssetIdKnown(const FPrimaryAssetId& Id) const;

	/** Reads the bytes of the save slot. False if there are none */
	virtual bool ReadStorage(TArray<uint8>& OutBytes) const;

	/** Writes the bytes of the save slot */
	virtual bool WriteStorage(const TArray<uint8>& Bytes) const;

private:
	/** Reads the file the first time it is needed */
	void EnsureLoaded();

	/** Writes the presets to the storage */
	bool Flush();

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_LoadoutSaveGame> Save;
};

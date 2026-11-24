// Levels mode: the ordered list of maps.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_LevelSetDefinition.generated.h"

class UWorld;

/** One map of the set. The map names its own Experience and GameRules in its world settings; ExtraOptions can override them for this entry */
USTRUCT(BlueprintType)
struct GSTNMODE_LEVELSRUNTIME_API FGSTN_LevelEntry
{
	GENERATED_BODY()

	/** Stable name for UI and analytics; empty = the map's short name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName LevelId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> Map;

	/** Appended to the travel URL, e.g. "?GameRules=/Game/GAS/GR_Level02" (a leading "?" is added when missing) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FString ExtraOptions;

	bool IsValidEntry() const { return !Map.IsNull(); }
	FName GetId() const;
};

/**
 * The levels of Levels mode, in play order (owner GM.3, 2026-11-22: an ordered list of maps, no persistent save in v1; the hub/lobby that picks
 * a set comes later). Data only. Entries without a map are skipped, so a half-authored set never travels to nowhere.
 */
UCLASS(BlueprintType)
class GSTNMODE_LEVELSRUNTIME_API UGSTN_LevelSetDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Levels")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TArray<FGSTN_LevelEntry> Levels;

	/** After the last level, go back to the first (an endless run). Off = the set ends there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Levels")
	bool bLoop = false;

	/** Where the team goes after the last level (the hub). Empty = stay on the results screen and publish Message.Levels.LevelSetCompleted */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Levels", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> ReturnMap;

	UFUNCTION(BlueprintPure, Category = "Levels")
	int32 GetNumLevels() const { return Levels.Num(); }

	/** The entry, null when Index is out of range */
	const FGSTN_LevelEntry* GetEntry(int32 Index) const { return Levels.IsValidIndex(Index) ? &Levels[Index] : nullptr; }

	/**
	 * The index of the entry whose map is MapName (a package name or the short map name; a PIE prefix is ignored), INDEX_NONE if the set does not
	 * list it. Short names are compared, so two maps of one set must not share a name in different folders
	 */
	UFUNCTION(BlueprintPure, Category = "Levels")
	int32 FindIndexByMap(const FString& MapName) const;

	/** The next valid level after Current (INDEX_NONE as Current = the first valid one); INDEX_NONE at the end of a set that does not loop */
	UFUNCTION(BlueprintPure, Category = "Levels")
	int32 GetNextIndex(int32 Current) const;

	/** "/Game/Maps/L_X[?listen][?extra]": what ServerTravel takes. Empty for an invalid entry */
	UFUNCTION(BlueprintPure, Category = "Levels")
	FString BuildTravelURL(int32 Index, bool bListen) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("GSTN_LevelSet"), GetFName()); }
};

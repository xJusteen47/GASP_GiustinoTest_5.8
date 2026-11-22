#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_ExperienceDefinition.generated.h"

class UGameFeatureAction;
class UGSTN_ExperienceActionSet;
class UGSTN_PawnData;

/**
 * One mode's Experience (game-modes.md 2.2, modularity-and-tags.md 3.4): the Game Features to turn on, the default pawn data, the
 * action sets and own actions, and a tag naming the mode. Chosen by the game mode on the host (map settings or URL option, never a
 * client's) and replicated to clients by UGSTN_ExperienceManagerComponent.
 *
 * Data only. The Experience never contains code: a mode adds or removes behaviour by listing features and actions, and "disable
 * the Game Feature" must leave the framework state untouched (game-modes.md 2.4).
 *
 * Const: the asset is shared by every machine in the match and must not change at runtime (simulation data is final before the
 * first Network Prediction tick of any pawn, game-modes.md 2.5).
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "GSTN Experience Definition"))
class GSTNEXPERIENCE_API UGSTN_ExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static FPrimaryAssetType GetAssetType() { return FPrimaryAssetType(TEXT("GSTN_Experience")); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Names the mode (`Experience.Mode.<Name>`, an ini tag owned by the mode's Game Feature). Frameworks may key rules off it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience", meta = (Categories = "Experience.Mode"))
	FGameplayTag ModeTag;

	/** Game Feature plugin names (the plugin's name, not a URL) activated before the actions run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Features")
	TArray<FString> GameFeaturesToEnable;

	/** Pawn data used when a player has no pawn data of their own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Pawn")
	TObjectPtr<const UGSTN_PawnData> DefaultPawnData;

	/** Shared action bundles, executed after this asset's own Actions, in array order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Actions")
	TArray<TObjectPtr<UGSTN_ExperienceActionSet>> ActionSets;

	/** Actions specific to this Experience, executed first. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Experience|Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** Game Feature names of the Experience, then of each action set, first occurrence wins, empty names dropped. */
	void CollectGameFeatureNames(TArray<FString>& OutNames) const;

	/** Actions in execution order: own Actions, then each action set's, null entries skipped. */
	void CollectActions(TArray<UGameFeatureAction*>& OutActions) const;

	/** Primary assets whose bundles the load must resolve: this asset, the default pawn data and the action sets. */
	void CollectPrimaryAssetIds(TArray<FPrimaryAssetId>& OutIds) const;
};

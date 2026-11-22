#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_PawnData.generated.h"

class APawn;

/**
 * What a mode's player pawn is made of (modularity-and-tags.md 3.4, step S-J): the pawn class, the ability sets granted to it and
 * the input config. Referenced by the Experience as its default pawn data; applied by the pawn-extension code after the Experience
 * is Loaded and before the pawn simulates.
 *
 * All members are soft references: loading is done by the Experience (asset bundles "Client" and "Server"), so a client never loads
 * a server-only set and a dedicated server never loads presentation assets.
 *
 * AbilitySets and InputConfig are typed as UPrimaryDataAsset on purpose: the typed assets (ability set with command tags, input
 * config) belong to GSTNModeVerbs / GSTNInput (G2), and this plugin must not depend on GAS or on the input plugin. A consumer casts
 * what it knows and ignores the rest.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "GSTN Pawn Data"))
class GSTNEXPERIENCE_API UGSTN_PawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static FPrimaryAssetType GetAssetType() { return FPrimaryAssetType(TEXT("GSTN_PawnData")); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Class to spawn for a player of this mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TSoftClassPtr<APawn> PawnClass;

	/** Ability sets granted to the pawn, in order. Later sets may override command bindings of earlier ones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn|Abilities", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<UPrimaryDataAsset>> AbilitySets;

	/** Input action to button mapping for this pawn. Client only: a dedicated server has no local input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn|Input", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UPrimaryDataAsset> InputConfig;

	/** Synchronous load of the pawn class, for the game mode's spawn path (the Experience has already loaded the bundles). */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	TSubclassOf<APawn> LoadPawnClass() const;
};

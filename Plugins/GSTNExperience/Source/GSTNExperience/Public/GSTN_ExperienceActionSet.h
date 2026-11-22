#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_ExperienceActionSet.generated.h"

class UGameFeatureAction;

/**
 * A reusable bundle of Game Feature actions (add components, grant abilities, add input mappings...) that several Experiences
 * share, plus the Game Features it needs. Lyra's ULyraExperienceActionSet. Actions run in array order after every Game Feature
 * of the Experience is active, so they can rely on the content those features bring.
 */
UCLASS(BlueprintType, NotBlueprintable, meta = (DisplayName = "GSTN Experience Action Set"))
class GSTNEXPERIENCE_API UGSTN_ExperienceActionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static FPrimaryAssetType GetAssetType() { return FPrimaryAssetType(TEXT("GSTN_ExperienceActionSet")); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Actions performed when the Experience is executing its actions. Null entries are skipped. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** Game Feature plugin names this set needs, merged (without duplicates) with the Experience's own list. */
	UPROPERTY(EditAnywhere, Category = "Feature Dependencies")
	TArray<FString> GameFeaturesToEnable;
};

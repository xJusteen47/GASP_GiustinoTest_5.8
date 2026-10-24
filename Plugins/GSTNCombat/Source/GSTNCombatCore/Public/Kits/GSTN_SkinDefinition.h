// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_SkinDefinition.generated.h"

class UAnimInstance;
class UMaterialInterface;
class USkeletalMesh;
class UTexture2D;

/**
 * The looks a loadout can pick: a mesh, material overrides and an animation class. Presentation only: no simulation code reads
 * a skin, it is applied outside the simulation on spawn and when the applied loadout changes. Every content reference is soft,
 * so a skin costs nothing until it is applied, and a game replaces a skin by data. Primary asset type GSTN_Skin.
 *
 * Which kits a skin fits is the kit's CompatibleSkins query against SkinTags.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_SkinDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Identity and classification of the skin (a leaf under the skin root, plus any classifying tags) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin", meta = (Categories = "Skin"))
	FGameplayTagContainer SkinTags;

	/** Replaces the character's mesh. Empty = keep the kit's mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Look")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Material per slot, in slot order. An empty entry keeps the mesh's own material of that slot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Look")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	/** Replaces the animation class. Empty = keep the character's */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Look")
	TSoftClassPtr<UAnimInstance> AnimClass;

	/** True when the skin changes at least one thing */
	bool ChangesAnything() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

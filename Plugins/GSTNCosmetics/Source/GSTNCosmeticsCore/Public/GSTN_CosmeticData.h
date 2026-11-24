// Modular cosmetics, pure layer.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_CosmeticTypes.h"
#include "GSTN_CosmeticData.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

namespace GSTNCosmeticAssetTypes
{
	inline FPrimaryAssetType Theme() { return FPrimaryAssetType(TEXT("GSTN_CosmeticTheme")); }
	inline FPrimaryAssetType Host() { return FPrimaryAssetType(TEXT("GSTN_CosmeticHost")); }
}

/**
 * A cosmetic theme: HOW it decorates, on any conforming item (skin-cosmetici-modulari.md §5). Sold once as a design style and usable on
 * every item, like a Fortnite wrap but with 3D pieces, so the art cost grows as themes + items, never themes x items. Presentation only.
 * Primary asset type GSTN_CosmeticTheme.
 */
UCLASS(BlueprintType)
class GSTNCOSMETICSCORE_API UGSTN_CosmeticThemeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme", meta = (Categories = "Cosmetic.Theme"))
	FGameplayTag ThemeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * Bumped whenever the theme's look changes: part of the cache key of every item built with it, so a patch rebuilds only the
	 * combinations with this theme
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme", meta = (ClampMin = "1"))
	int32 Version = 1;

	/** The one material of the theme (a layer over the item's own, plus the trim sheet on UV1). Always applied: the T0 fallback */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Material")
	TSoftObjectPtr<UMaterialInterface> LayerMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Material")
	FGSTN_CosmeticPalette Palette;

	/** At most one per slot class: the plan picks the exact class, then walks up to the parent class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Pieces")
	TArray<FGSTN_CosmeticPiece> Pieces;

	/** At most one per spline class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Pieces")
	TArray<FGSTN_CosmeticSplineModule> SplineModules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Theme|Styles")
	TArray<FGSTN_CosmeticStyleChannel> StyleChannels;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * A decoratable item (weapon, shield, kit piece): WHERE a theme may decorate it. Authored once per item, inside Unreal (sockets,
 * curves, vertex colours, UV1), 30-60 minutes per item. Primary asset type GSTN_CosmeticHost.
 */
UCLASS(BlueprintType)
class GSTNCOSMETICSCORE_API UGSTN_CosmeticHostData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The item's base mesh, pivot at the main grip, X along the blade, Z on the back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Host")
	TSoftObjectPtr<UStaticMesh> BaseMesh;

	/** Bounds of the base mesh in mesh space, cm: the silhouette the decoration may not outgrow. Copied from the mesh (editor tool, phase 2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Host")
	FBox BaseBounds = FBox(FVector(-5.0), FVector(5.0));

	/** Bumped whenever slots, splines or the mesh change: part of the cache key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Host", meta = (ClampMin = "1"))
	int32 Version = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Host")
	TArray<FGSTN_CosmeticSlot> Slots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Host")
	TArray<FGSTN_CosmeticSpline> Splines;

	/** A different piece for one theme and one slot class on this item only. Keep rare: more than one in ten means the slots are wrong */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Host")
	TArray<FGSTN_CosmeticPieceOverride> Overrides;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

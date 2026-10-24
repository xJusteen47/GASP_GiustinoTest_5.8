// Depth marker style: the replaceable look (depth-marker.md §5, §6.5).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "GSTN_DepthMarkerStyle.generated.h"

class UGSTN_DepthMarkerPresenter;

/**
 * What a marker looks like: a list of presenter templates plus the fade and tint data. Swap the asset to change the look
 * (settings default < component override < local override pushed on the subsystem). It holds no reference to any asset by
 * itself: the built-in presenters name their meshes and materials as soft references, filled from the plugin ini.
 */
UCLASS(BlueprintType)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthMarkerStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Presenter templates; each marker duplicates them. Empty draws nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "DepthMarker|Style")
	TArray<TObjectPtr<UGSTN_DepthMarkerPresenter>> Presenters;

	/**
	 * The look of the approach marker B (the platform plane hanging beyond a nearby higher platform's edge): its own slot, so it can
	 * be another colour or effect. Its presenters are forced onto the approach layer. Empty = the built-in approach look.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Style")
	TSoftObjectPtr<UGSTN_DepthMarkerStyle> ApproachStyle;

	/** Fade and visibility thresholds over height (§3) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Style")
	FGSTN_DepthFadeParams Fade;

	/** Tint by team id (§4.4); colour alone is not enough: add a distinguishing presenter per team for colour-blind players */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Style")
	TMap<int32, FLinearColor> TeamTints;

	/** Used when no team tint, no component override and no adapter tint apply */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Style")
	FLinearColor DefaultTint = FLinearColor(1.f, 0.95f, 0.6f, 1.f);

	/** Alpha of the dashed line and ring where the scene hides them (reserved for the depth-aware materials of §4.3) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OccludedAlpha = 0.3f;

	/** Tint comes from the settings' player palette (colour rank by player name) instead of DefaultTint; a component override still wins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Style")
	bool bUsePlayerPalette = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Style")
	EGSTN_DepthLinkPolicy LinkPolicy = EGSTN_DepthLinkPolicy::Hide;

	/** The built-in look: ground decal, no-ground ring, above ring and the two dashed lines, all from engine primitives */
	UFUNCTION(BlueprintCallable, Category = "DepthMarker")
	void PopulateDefaultPresenters();

	/** The built-in approach look: a ring and a dashed line in another colour, drawn in the Approach state of the approach layer */
	UFUNCTION(BlueprintCallable, Category = "DepthMarker")
	void PopulateApproachPresenters();

	/** The toy look: stained-glass decal and beam on a surface, beam and fog puff in the air. Materials are set by the asset (setup script) */
	UFUNCTION(BlueprintCallable, Category = "DepthMarker")
	void PopulateToyPresenters();

	/** The toy approach look: beam and fog puff */
	UFUNCTION(BlueprintCallable, Category = "DepthMarker")
	void PopulateToyApproachPresenters();

	/** Problems of the style as text; runtime-safe (an Editor module can wrap it in the data validator). True when none */
	bool Validate(TArray<FText>& OutErrors) const;
};

// Depth marker component: the opt-in (depth-marker.md §5, §6.4).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "GSTN_DepthMarkerComponent.generated.h"

class UGSTN_DepthMarkerStyle;
class UGSTN_DepthTargetSelector;

/**
 * Gives its actor a depth marker. The opt-in is data: a pawn has a marker if it has this component (added by pawn data, an
 * ability set, a Game Feature AddComponents action or a minion definition), so no code asks "is this an enemy".
 *
 * Presentation only. The component is NOT replicated, holds no simulation state and registers with the local depth marker
 * subsystem, which does not exist on a dedicated server; there it is inert. The marker itself lives in the subsystem.
 */
UCLASS(ClassGroup = (GSTN), Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthMarkerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_DepthMarkerComponent();

	/** Look of this marker; empty = the settings default (or the built-in look) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker")
	TSoftObjectPtr<UGSTN_DepthMarkerStyle> Style;

	/** Look of the approach marker B of this pawn; empty = the style's own ApproachStyle slot, then the built-in look */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker")
	TSoftObjectPtr<UGSTN_DepthMarkerStyle> ApproachStyle;

	/** Target selection of this marker; empty = the default selector */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "DepthMarker")
	TObjectPtr<UGSTN_DepthTargetSelector> Selector;

	/** Who sees this marker. An enemy type that opts in sets All; the default shows it to the local player and allies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker")
	EGSTN_DepthAudience Audience = EGSTN_DepthAudience::LocalAndAllies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker")
	bool bOverrideTint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker", meta = (EditCondition = "bOverrideTint"))
	FLinearColor TintOverride = FLinearColor::White;

	/** Hidden while the owner tags (from IGSTN_DepthTagSource) match. Data, never a code branch; empty = never hidden */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Tags")
	FGameplayTagQuery HideWhen;

	/** Shown only while the owner tags match; empty = always */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Tags")
	FGameplayTagQuery ShowOnlyWhen;

	/** Local switch (a Blueprint can turn the marker off for a phase without removing the component) */
	UFUNCTION(BlueprintCallable, Category = "DepthMarker")
	void SetMarkerEnabled(bool bInEnabled) { bMarkerEnabled = bInEnabled; }

	UFUNCTION(BlueprintPure, Category = "DepthMarker")
	bool IsMarkerEnabled() const { return bMarkerEnabled; }

	/** The last computed targets, for Blueprint presenters and tests; empty until the subsystem ran */
	UFUNCTION(BlueprintPure, Category = "DepthMarker")
	const FGSTN_DepthResult& GetLastResult() const { return LastResult; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UGSTN_DepthMarkerSubsystem;

	bool bMarkerEnabled = true;
	FGSTN_DepthResult LastResult;
};

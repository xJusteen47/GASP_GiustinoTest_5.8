// Projection volumes: where the depth marker and its blob shadow are cast along something other than gravity (a wall, a ceiling, a set piece).

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_DepthMarkerTypes.h"
#include "GSTN_DepthProjectionVolume.generated.h"

/**
 * While a marker's feet are inside this box, the marker is cast along Projection instead of gravity (unless a state rule of the
 * marker component says otherwise). Among several boxes the higher Priority wins. Presentation only: no collision, not replicated,
 * a point-in-box test per marker per frame (toaster-cheap).
 */
UCLASS(ClassGroup = (GSTN), Blueprintable, meta = (BlueprintSpawnableComponent, DisplayName = "Depth Projection Box"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthProjectionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UGSTN_DepthProjectionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Projection")
	FGSTN_DepthProjection Projection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Projection")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Projection")
	bool bProjectionEnabled = true;

	/** A World direction given in this box's own frame instead (rotate the box to aim the shadows) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Projection")
	bool bDirectionInBoxSpace = false;

	UFUNCTION(BlueprintPure, Category = "DepthMarker")
	bool ContainsPoint(const FVector& WorldPoint) const;

	/** Projection with a box-space direction already turned into world space */
	FGSTN_DepthProjection GetWorldProjection() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};

/** A placeable actor with one projection box */
UCLASS(Blueprintable, meta = (DisplayName = "Depth Projection Volume"))
class GSTNDEPTHMARKERRUNTIME_API AGSTN_DepthProjectionVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_DepthProjectionVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DepthMarker")
	TObjectPtr<UGSTN_DepthProjectionComponent> Box;
};

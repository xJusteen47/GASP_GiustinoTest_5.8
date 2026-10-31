// Designer-declared projection planes for the approach marker (depth-marker.md §2.7; the approach layer is an addition to it).

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_DepthMarkerTypes.h"
#include "GSTN_DepthMarkerPlane.generated.h"

/**
 * Declares a projection plane explicitly: the top face of this box (along the component's own up, plus PlaneOffsetCm). While a
 * depth marker character overlaps the box, the approach marker B uses this plane instead of probing: the override for a
 * level where the probes guess wrong, and the fallback where they find nothing. Presentation only: no collision, not replicated.
 */
UCLASS(ClassGroup = (GSTN), Blueprintable, meta = (BlueprintSpawnableComponent, DisplayName = "Depth Marker Plane"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthMarkerPlaneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UGSTN_DepthMarkerPlaneComponent();

	/** Among several overlapping planes the higher priority wins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Plane")
	int32 Priority = 0;

	/** Moves the plane along the component's up from the box's top face (negative = into the box) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Plane", meta = (ForceUnits = "cm"))
	float PlaneOffsetCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Plane")
	bool bPlaneEnabled = true;

	/** The plane in world space: a point of the top face and the component's up */
	UFUNCTION(BlueprintPure, Category = "DepthMarker")
	void GetProjectionPlane(FVector& OutPoint, FVector& OutNormal) const;

	/** True when the point is inside the box (scale taken into account) */
	UFUNCTION(BlueprintPure, Category = "DepthMarker")
	bool ContainsPoint(const FVector& WorldPoint) const;

	/** The plane as an explicit approach candidate */
	FGSTN_DepthPlaneCandidate MakeCandidate() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};

/** A placeable actor with one plane component: drop it in the level around the gap and set the top face to the platform's height */
UCLASS(Blueprintable, meta = (DisplayName = "Depth Marker Plane Volume"))
class GSTNDEPTHMARKERRUNTIME_API AGSTN_DepthMarkerPlaneVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_DepthMarkerPlaneVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DepthMarker")
	TObjectPtr<UGSTN_DepthMarkerPlaneComponent> Plane;
};

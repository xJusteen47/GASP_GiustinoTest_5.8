// Depth marker presenters: what a marker looks like (depth-marker.md §4, §6.5). Presentation only.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "UObject/Object.h"
#include "GSTN_DepthMarkerPresenter.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

/**
 * One drawn element of a marker. A style (UGSTN_DepthMarkerStyle) lists presenter TEMPLATES; the subsystem duplicates them for
 * each marker, so a presenter instance may keep per marker state and components. Components are created on the subsystem's
 * holder actor, never on the pawn, so nothing about the pawn changes (and nothing replicates).
 *
 * Blueprint children (the diegetic option) override OnMarkerUpdated and drive any actor or Niagara system.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthMarkerPresenter : public UObject
{
	GENERATED_BODY()

public:
	/** States of the target in which this element draws. Empty draws in every state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Presenter")
	TArray<EGSTN_DepthMarkerState> ActiveStates;

	/** Which layer of the result it follows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Presenter")
	EGSTN_DepthLayer Layer = EGSTN_DepthLayer::Below;

	/** Multiplies the alpha the marker computed (the above ring is lighter by default so two markers do not compete) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Presenter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlphaScale = 1.f;

	/** Called every frame the marker is active. Holder is the actor that owns the presenter components */
	UFUNCTION(BlueprintNativeEvent, Category = "DepthMarker")
	void OnMarkerUpdated(const FGSTN_DepthMarkerView& View, AActor* Holder);

	/** Called when the target state of the layer changes (hidden, ground, edge hold, no ground, above) */
	UFUNCTION(BlueprintNativeEvent, Category = "DepthMarker")
	void OnStateChanged(EGSTN_DepthMarkerState OldState, EGSTN_DepthMarkerState NewState);

	/** Hide or destroy whatever the presenter made: the marker went away or the style changed */
	UFUNCTION(BlueprintNativeEvent, Category = "DepthMarker")
	void OnMarkerReleased();

	/** True when the layer's target exists, is visible and is in one of the active states */
	bool ShouldDraw(const FGSTN_DepthMarkerView& View) const;

	/** Alpha of this element for the frame (marker alpha x AlphaScale) */
	float GetAlpha(const FGSTN_DepthMarkerView& View) const { return View.GetAlpha(Layer) * AlphaScale; }

	/** Used by the subsystem to detect state changes without a Blueprint call per frame */
	EGSTN_DepthMarkerState LastState = EGSTN_DepthMarkerState::Hidden;

protected:
	virtual void OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder) {}
	virtual void OnStateChanged_Implementation(EGSTN_DepthMarkerState OldState, EGSTN_DepthMarkerState NewState) {}
	virtual void OnMarkerReleased_Implementation() {}
};

/** Shared plumbing of the built-in presenters: soft content, one dynamic material, a tint */
UCLASS(Abstract)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Primitive : public UGSTN_DepthMarkerPresenter
{
	GENERATED_BODY()

public:
	/** Replaces the settings' default (soft: never loaded by the plugin until first drawn). Empty = settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Content")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

protected:
	/** Applies the tint and alpha to the dynamic material */
	void ApplyLook(const FGSTN_DepthMarkerView& View);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
};

/**
 * Ground disc: a decal projected along Down, so it clips to geometry and follows slopes (§4.1). Draws in the Ground and
 * EdgeHold states by default.
 */
UCLASS(meta = (DisplayName = "Decal (ground disc)"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Decal : public UGSTN_DepthPresenter_Primitive
{
	GENERATED_BODY()

public:
	UGSTN_DepthPresenter_Decal();

	/** Radius multiplier on top of FootRadius x the height curve */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Decal", meta = (ClampMin = "0.1"))
	float RadiusMultiplier = 1.5f;

	/** Half depth of the projection box along Down: how far above and below the plane the decal still paints */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Decal", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float DepthHalfExtentCm = 60.f;

protected:
	virtual void OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder) override;
	virtual void OnMarkerReleased_Implementation() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Decal;
};

/** Shared plumbing of the presenters made of instanced engine meshes (no custom content needed) */
UCLASS(Abstract)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Instanced : public UGSTN_DepthPresenter_Primitive
{
	GENERATED_BODY()

public:
	/** Mesh of one segment or dash (a unit cube scaled); empty = settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Content")
	TSoftObjectPtr<UStaticMesh> MeshOverride;

protected:
	/** Creates the component and its fixed pool of Count instances on first use */
	UInstancedStaticMeshComponent* EnsureComponent(AActor* Holder, int32 Count);
	void HideAll();
	virtual void OnMarkerReleased_Implementation() override;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	int32 PoolSize = 0;
};

/** A hollow ring on a plane made of flat segments: the "no ground" ring below and the ring above (§4.1) */
UCLASS(meta = (DisplayName = "Ring (hollow, flat segments)"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Ring : public UGSTN_DepthPresenter_Instanced
{
	GENERATED_BODY()

public:
	UGSTN_DepthPresenter_Ring();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Ring", meta = (ClampMin = "6", ClampMax = "64"))
	int32 Segments = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Ring", meta = (ClampMin = "0.1"))
	float RadiusMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Ring", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	float ThicknessCm = 3.f;

protected:
	virtual void OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder) override;
};

/**
 * The dashed line from the feet (below layer) or from the head (above layer) to the marker point (§4.2), made of cube dashes that
 * march along the line. No shader needed: the dash maths is the spec's, evaluated on the CPU.
 */
UCLASS(meta = (DisplayName = "Dashed line"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Line : public UGSTN_DepthPresenter_Instanced
{
	GENERATED_BODY()

public:
	UGSTN_DepthPresenter_Line();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Line", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float DashPeriodCm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Line", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DutyCycle = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Line", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	float WidthCm = 2.5f;

	/** Dashes drift towards the target at this speed: a motion cue for "down" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Line", meta = (ForceUnits = "cm/s"))
	float MarchSpeed = 30.f;

	/** The pool size; a longer line widens the period instead of growing the pool */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Line", meta = (ClampMin = "4", ClampMax = "128"))
	int32 MaxDashes = 64;

protected:
	virtual void OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder) override;
};

/**
 * A flat quad centred on the marker point and oriented to the layer's present normal, for a designer's world-aligned grid material.
 * Off (nothing created, nothing loaded) while PlaneMaterial is empty. Dynamic material parameters, set every frame:
 * MarkerWorldPosition (vector), MarkerNormal (vector), Opacity (scalar, the layer alpha), EdgeDistance (scalar, cm; approach layer, else 0).
 */
UCLASS(meta = (DisplayName = "Plane (grid material quad)"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Plane : public UGSTN_DepthMarkerPresenter
{
	GENERATED_BODY()

public:
	UGSTN_DepthPresenter_Plane();

	/** Material of the quad; empty = the layer is off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Plane")
	TSoftObjectPtr<UMaterialInterface> PlaneMaterial;

	/** Replaces the engine plane (a 100 cm quad facing +Z); empty = the engine plane */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Plane")
	TSoftObjectPtr<UStaticMesh> MeshOverride;

	/** Side length of the quad */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Plane", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float PlaneExtentCm = 150.f;

	/** The component and the dynamic material, for tests and adapters; null while the layer is off or not yet drawn */
	UStaticMeshComponent* GetPlaneComponent() const { return PlaneComponent; }
	UMaterialInstanceDynamic* GetPlaneMaterialInstance() const { return Material; }

protected:
	virtual void OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder) override;
	virtual void OnMarkerReleased_Implementation() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PlaneComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
};

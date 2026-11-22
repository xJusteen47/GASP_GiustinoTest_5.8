// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "Preview/GSTN_GizmoMath.h"
#include "Preview/GSTN_GizmoTarget.h"
#include "Preview/GSTN_PreviewCamera.h"
#include "Preview/SGSTN_TimelineViewport.h"

class FGSTN_TimelineEditorModel;
class FGSTN_TimelinePreviewScene;
class SEditorViewport;

/**
 * The viewport client of the timeline editor's preview (combat-m5-editor 5.4): an orbit camera on the subject with side, front and 3/4 presets,
 * follow root, a ground grid, lit and unlit, and the fixed drawing of the root (capsule and facing arrow) plus whatever overlays are registered.
 *
 * It also owns every piece of the gizmo wiring (plan m5 E7, spike S4): the translate/rotate/scale widget (FWidget through FEditorViewportClient, enabled by
 * FGSTN_GizmoEdMode), the W/E/R, K, Shift+K and Ctrl+` keys, the mouse drag that turns widget deltas into one accumulated delta for the IGSTN_GizmoTarget,
 * Escape, and clicking a shape to select it. The data side is the target; the maths is GSTNGizmo (GSTN_GizmoMath.h) and is tested without a viewport.
 *
 * Nothing allocates in Draw: the drawing helpers write lines straight into the primitive interface.
 */
class FGSTN_TimelineViewportClient : public FEditorViewportClient
{
public:
	FGSTN_TimelineViewportClient(const TSharedRef<FGSTN_TimelinePreviewScene>& InScene, const TSharedRef<SEditorViewport>& InViewportWidget, const TSharedPtr<FGSTN_TimelineEditorModel>& InModel);

	/** Places the camera for the preset, framing everything the scene shows */
	void SetCameraPreset(EGSTN_PreviewCameraPreset Preset);
	EGSTN_PreviewCameraPreset GetCameraPreset() const { return Preset; }

	/** Frames the subject again, along the current preset (after a subject change, or the Frame button) */
	void FrameSubject();

	bool IsFollowRoot() const { return bFollowRoot; }
	void SetFollowRoot(bool bInFollow);

	bool IsGridShown() const { return bShowGrid; }
	void SetGridShown(bool bShown);

	bool IsRootShown() const { return bShowRoot; }
	void SetRootShown(bool bShown);

	bool IsLit() const { return bLit; }

	bool AreHitboxesShown() const { return bShowHitboxes; }
	void SetHitboxesShown(bool bShown);

	/** What the gizmo moves. Null: no gizmo (the viewport is a plain preview) */
	void SetGizmoTarget(const TSharedPtr<IGSTN_GizmoTarget>& InTarget);
	const TSharedPtr<IGSTN_GizmoTarget>& GetGizmoTarget() const { return GizmoTarget; }

	/** W, E and R */
	void SetGizmoMode(UE::Widget::EWidgetMode Mode);

	/** Ctrl+`: the gizmo's axes follow the key's space (local) or the world. The scale gizmo is always local, because it scales along the shape's axes */
	bool IsGizmoLocal() const;
	void ToggleGizmoLocal();

	/** Something more to draw after the fixed parts and the registered overlays (the hurtbox set editor draws its shapes here) */
	void SetExtraDraw(const FGSTN_ViewportDrawExtra& InExtra) { ExtraDraw = InExtra; }
	void SetLit(bool bInLit);

	//~ FEditorViewportClient
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual bool InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void TrackingStopped() override;
	virtual void AbortTracking() override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override;
	virtual bool CanCycleWidgetMode() const override { return false; }

private:
	TWeakPtr<FGSTN_TimelinePreviewScene> Scene;
	TWeakPtr<FGSTN_TimelineEditorModel> Model;

	FGSTN_ViewportDrawExtra ExtraDraw;

	TSharedPtr<IGSTN_GizmoTarget> GizmoTarget;

	/** The mouse is down on a handle and the target has an open drag */
	bool bGizmoDragging = false;

	/** Escape (or a target with nothing to edit) ended the drag: the rest of the mouse movement is swallowed until the button is released */
	bool bGizmoDragCancelled = false;

	/** The sum of the widget deltas of the drag so far; the target always applies the whole of it from the state at the start */
	GSTNGizmo::FDragDelta GizmoDragDelta;

	EGSTN_PreviewCameraPreset Preset = EGSTN_PreviewCameraPreset::ThreeQuarter;
	bool bFollowRoot = true;
	bool bShowGrid = true;
	bool bShowRoot = true;
	bool bLit = true;
	bool bShowHitboxes = true;

	/** The root's world position the camera last followed */
	FVector FollowedRoot = FVector::ZeroVector;
};

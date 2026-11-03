// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FGSTN_PreviewController;
class IGSTN_GizmoTarget;
class FGSTN_TimelineViewportClient;
class FPrimitiveDrawInterface;

/** Something else to draw over the scene each redraw (the hurtbox set editor's shapes). Root space is world space in the preview */
DECLARE_DELEGATE_OneParam(FGSTN_ViewportDrawExtra, FPrimitiveDrawInterface*);

/**
 * The Viewport tab of the timeline editor: the 3D preview of the controller's scene with a small toolbar (camera presets, frame, follow root,
 * grid, root capsule, lit/unlit) and a line naming the subject and anything wrong with it. The viewport only displays: the pose, the subject
 * and the playhead all come from the controller.
 */
class SGSTN_TimelineViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineViewport) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_PreviewController>, Controller)

		/** Called from the viewport's Draw, after the fixed drawing. Unbound: nothing extra */
		SLATE_EVENT(FGSTN_ViewportDrawExtra, OnDrawExtra)

		/** What the gizmos move. Unset: the hitbox keys of the controller's timeline model, or no gizmo for a controller that has none */
		SLATE_ARGUMENT(TSharedPtr<IGSTN_GizmoTarget>, GizmoTarget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_TimelineViewport() override;

	TSharedPtr<FGSTN_TimelineViewportClient> GetTimelineViewportClient() const { return TimelineClient; }

protected:
	//~ SEditorViewport
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual EVisibility GetTransformToolbarVisibility() const override { return EVisibility::Collapsed; }

private:
	void HandlePreviewChanged();

	TSharedPtr<FGSTN_PreviewController> Controller;
	TSharedPtr<FGSTN_TimelineViewportClient> TimelineClient;
	TSharedPtr<IGSTN_GizmoTarget> GizmoTarget;
	FGSTN_ViewportDrawExtra DrawExtra;
	FDelegateHandle PreviewChangedHandle;
	FDelegateHandle SelectionChangedHandle;

	/** The scene had a mesh the last time the camera framed it; a subject that just got a mesh frames again */
	bool bFramedSubject = false;
};

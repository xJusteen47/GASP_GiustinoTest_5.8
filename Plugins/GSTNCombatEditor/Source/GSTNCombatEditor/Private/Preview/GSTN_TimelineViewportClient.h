// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "Preview/GSTN_PreviewCamera.h"

class FGSTN_TimelineEditorModel;
class FGSTN_TimelinePreviewScene;
class SEditorViewport;

/**
 * The viewport client of the timeline editor's preview (combat-m5-editor 5.4): an orbit camera on the subject with side, front and 3/4 presets,
 * follow root, a ground grid, lit and unlit, and the fixed drawing of the root (capsule and facing arrow) plus whatever overlays are registered.
 * Gizmos come with E7 and live in this class.
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
	void SetLit(bool bInLit);

	//~ FEditorViewportClient
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TWeakPtr<FGSTN_TimelinePreviewScene> Scene;
	TWeakPtr<FGSTN_TimelineEditorModel> Model;

	EGSTN_PreviewCameraPreset Preset = EGSTN_PreviewCameraPreset::ThreeQuarter;
	bool bFollowRoot = true;
	bool bShowGrid = true;
	bool bShowRoot = true;
	bool bLit = true;

	/** The root's world position the camera last followed */
	FVector FollowedRoot = FVector::ZeroVector;
};

// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Preview/GSTN_TimelinePreviewScene.h"

class FPrimitiveDrawInterface;
class FGSTN_TimelineEditorModel;

/** What an overlay needs to draw the timeline at the playhead frame */
struct FGSTN_PreviewDrawContext
{
	const FGSTN_TimelineEditorModel* Model = nullptr;
	const FGSTN_TimelinePreviewScene* Scene = nullptr;
	int32 Frame = 0;
};

/**
 * Something drawn over the mesh at the playhead frame: the hurtboxes of the kit, the hitboxes active at the frame, baked-socket markers. The
 * shapes belong to M4, so they are not drawn yet: M4 registers its overlays here and the viewport calls every registered one (combat-m5-editor
 * 5.4, plan E6: "the overlay code compiles behind the registry, no #if"). An overlay is stateless between calls and must not allocate per draw.
 */
class IGSTN_PreviewOverlay
{
public:
	virtual ~IGSTN_PreviewOverlay() = default;

	/** Registry key; one overlay per name */
	virtual FName GetOverlayName() const = 0;

	virtual void Draw(const FGSTN_PreviewDrawContext& Context, FPrimitiveDrawInterface* PDI) const = 0;
};

class FGSTN_PreviewOverlayRegistry
{
public:
	/** Adds an overlay, replacing one of the same name */
	static void Register(const TSharedRef<IGSTN_PreviewOverlay>& Overlay);
	static void Unregister(FName OverlayName);
	static int32 Num();

	/** Draws every registered overlay in registration order */
	static void DrawAll(const FGSTN_PreviewDrawContext& Context, FPrimitiveDrawInterface* PDI);
};

/** The fixed parts of the preview drawing: ground grid, root capsule, facing arrow. Free functions over the PDI, no allocation */
namespace GSTNPreviewDrawing
{
	/** Grid on the floor: lines every Spacing out to Half cells each way, centred on the origin at the floor height */
	void DrawGrid(FPrimitiveDrawInterface* PDI, float FloorZ, float Spacing, int32 HalfCells);

	/** The wireframe capsule of the root (root space is world space in the preview) */
	void DrawRootCapsule(FPrimitiveDrawInterface* PDI, const FGSTN_PreviewCapsule& Capsule);

	/** An arrow from the middle of the capsule along +X, the direction the character faces, as long as the capsule is high */
	void DrawFacingArrow(FPrimitiveDrawInterface* PDI, const FGSTN_PreviewCapsule& Capsule);
}

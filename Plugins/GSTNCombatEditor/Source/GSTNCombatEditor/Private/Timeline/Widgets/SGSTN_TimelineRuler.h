// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"

/**
 * The ruler above the lanes (combat-m5-editor 5.2): frame ticks whose labels thin out with the zoom, a second line with the time
 * in milliseconds, the FAF and Total markers, and the playhead. Dragging on it scrubs the playhead; the wheel scrolls and, with
 * Ctrl, zooms the view that every lane shares. It decides nothing: the spacing comes from FGSTN_TimelineLayout.
 */
class SGSTN_TimelineRuler : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineRuler) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelinePaintStats>, PaintStats)
		/** Receives the keyboard focus when the ruler is clicked */
		SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Width of the lane area in pixels, from the last arrangement (0 before the first) */
	double GetLaneWidth() const { return GetCachedGeometry().GetLocalSize().X; }

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	void ScrubTo(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	TSharedPtr<FGSTN_TimelinePaintStats> PaintStats;
	TWeakPtr<SWidget> FocusTarget;
	TUniquePtr<FGSTN_TimelineRepaintHook> RepaintHook;
};

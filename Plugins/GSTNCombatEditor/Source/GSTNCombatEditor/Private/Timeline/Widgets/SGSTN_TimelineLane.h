// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"

/**
 * The lane of one track: the window as a bar, the keys as diamonds, the grid, the playhead and the shared box-select rectangle,
 * all painted in OnPaint (no child widget per key, so 12 tracks of 60 keys stay cheap). It turns the mouse into calls of
 * FGSTN_TimelineLayout (what is under the cursor) and FGSTN_TimelineGesture (what the drag does to the model); it holds no state
 * of its own beyond the track index it shows. The gesture is shared by all lanes and the phase bar: only one runs at a time.
 */
class SGSTN_TimelineLane : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineLane) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineGesture>, Gesture)
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelinePaintStats>, PaintStats)
		SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget)
		SLATE_ARGUMENT(int32, TrackIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	int32 GetTrackIndex() const { return TrackIndex; }

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	/** The mouse position in the lane's own space, with the y shifted into content space (rows stacked from the top of the list) */
	FVector2D ToContent(const FGeometry& Geometry, const FPointerEvent& MouseEvent) const;

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	TSharedPtr<FGSTN_TimelineGesture> Gesture;
	TSharedPtr<FGSTN_TimelinePaintStats> PaintStats;
	TWeakPtr<SWidget> FocusTarget;
	TUniquePtr<FGSTN_TimelineRepaintHook> RepaintHook;
	int32 TrackIndex = INDEX_NONE;
};

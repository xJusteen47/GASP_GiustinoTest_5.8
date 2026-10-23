// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Timeline/GSTN_PhaseBarData.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"

/**
 * The phase bar under the ruler (combat-m5-editor 5.2): startup, active, recovery and actionable stretches in the same frame
 * scale as the lanes, with the FAF marker you drag to write FirstActionableFrame, and below it the cancellable tail as a hatched
 * band from its start handle to the end, which you drag to move the tail start. Read-only except for those two handles; the
 * numbers come from FGSTN_PhaseBarData (E3's FGSTN_TimelinePhases replaces it). Hovering shows the frame data line.
 */
class SGSTN_PhaseBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_PhaseBar) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineGesture>, Gesture)
		SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	enum class EHandle : uint8 { None, FirstActionable, TailStart };

	/** Which draggable handle the local position is over */
	EHandle HitTestHandle(const FVector2D& Local) const;

	static FLinearColor PhaseColor(EGSTN_PhaseKind Kind);

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	TSharedPtr<FGSTN_TimelineGesture> Gesture;
	TWeakPtr<SWidget> FocusTarget;
	TUniquePtr<FGSTN_TimelineRepaintHook> RepaintHook;
};

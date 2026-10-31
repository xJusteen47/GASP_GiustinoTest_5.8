// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

/**
 * The launch angle of a hit as a dial (combat-m5-editor 5.1): a side view with the attacker at the pivot, the movement plane as the horizontal to the
 * right and the elevation as the arc above and below it. The needle shows AngleDeg; dragging it writes the angle, snapped to whole degrees (Shift: 5,
 * Ctrl: 15). The dial only reports numbers through its delegates; the details row decides how they reach the property (one transaction per drag).
 * The geometry is GSTNHitDataDial in GSTN_HitDataMath.h, which the tests cover.
 */
class SGSTN_AngleDial : public SLeafWidget
{
public:
	DECLARE_DELEGATE(FOnDragBegin)
	DECLARE_DELEGATE_OneParam(FOnAngleChanged, float /*AngleDeg*/)
	DECLARE_DELEGATE_OneParam(FOnDragEnd, float /*AngleDeg*/)

	SLATE_BEGIN_ARGS(SGSTN_AngleDial) {}
		/** The elevation shown, in degrees */
		SLATE_ATTRIBUTE(float, Angle)
		/** The special angle replaces the angle: the dial shows it dimmed and does not take a drag */
		SLATE_ATTRIBUTE(bool, bReadOnly)
		SLATE_EVENT(FOnDragBegin, OnDragBegin)
		/** Called for every step of a drag with the snapped angle */
		SLATE_EVENT(FOnAngleChanged, OnAngleChanged)
		SLATE_EVENT(FOnDragEnd, OnDragEnd)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The size the dial asks for: 150 x 100 */
	static FVector2D GetDialSize() { return FVector2D(150.0, 100.0); }

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return GetDialSize(); }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	/** Pivot and radius of the arc inside the widget */
	void GetArc(const FVector2D& Size, FVector2D& OutPivot, double& OutRadius) const;

	FReply Drag(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void EndDrag(float AngleDeg);

	TAttribute<float> Angle;
	TAttribute<bool> bReadOnly;
	FOnDragBegin OnDragBegin;
	FOnAngleChanged OnAngleChanged;
	FOnDragEnd OnDragEnd;

	bool bDragging = false;
	float DragStartAngle = 0.f;
	float LastDragAngle = 0.f;
};

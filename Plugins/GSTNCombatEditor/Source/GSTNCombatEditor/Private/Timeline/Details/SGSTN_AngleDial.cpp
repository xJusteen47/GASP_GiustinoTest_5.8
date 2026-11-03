// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Details/SGSTN_AngleDial.h"

#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Timeline/Details/GSTN_HitDataMath.h"

namespace
{
	const FLinearColor DialArc(0.45f, 0.45f, 0.5f, 1.f);
	const FLinearColor DialPlane(0.7f, 0.7f, 0.75f, 1.f);
	const FLinearColor DialNeedle(0.95f, 0.45f, 0.2f, 1.f);
	const FLinearColor DialTick(0.5f, 0.5f, 0.55f, 0.8f);
}

void SGSTN_AngleDial::Construct(const FArguments& InArgs)
{
	Angle = InArgs._Angle;
	bReadOnly = InArgs._bReadOnly;
	OnDragBegin = InArgs._OnDragBegin;
	OnAngleChanged = InArgs._OnAngleChanged;
	OnDragEnd = InArgs._OnDragEnd;
}

void SGSTN_AngleDial::GetArc(const FVector2D& Size, FVector2D& OutPivot, double& OutRadius) const
{
	// The pivot sits at the left, in the middle of the height; the arc takes what is left of the width and the height
	OutRadius = FMath::Max(FMath::Min(Size.Y * 0.5 - 6.0, Size.X - 28.0), 10.0);
	OutPivot = FVector2D(14.0, Size.Y * 0.5);
}

int32 SGSTN_AngleDial::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	FVector2D Pivot;
	double Radius = 0.0;
	GetArc(Size, Pivot, Radius);
	const bool bDimmed = bReadOnly.Get(false);
	const float Dim = bDimmed ? 0.4f : 1.f;
	const FLinearColor Tint(1.f, 1.f, 1.f, Dim);

	// The movement plane
	const TArray<FVector2f> Plane = { FVector2f(Pivot + FVector2D(-6.0, 0.0)), FVector2f(Pivot + FVector2D(Radius + 8.0, 0.0)) };
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Plane, ESlateDrawEffect::None, DialPlane * Tint, true, 1.f);

	// The arc from -90 to +90 and the ticks every 15 degrees, longer at 0, 45 and 90
	TArray<FVector2f> Arc;
	for (int32 Degrees = -90; Degrees <= 90; Degrees += 5)
	{
		Arc.Add(FVector2f(GSTNHitDataDial::PointOnArc(Pivot, Radius, static_cast<float>(Degrees))));
	}
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Arc, ESlateDrawEffect::None, DialArc * Tint, true, 1.5f);
	for (int32 Degrees = -90; Degrees <= 90; Degrees += 15)
	{
		const double Long = (Degrees % 45 == 0) ? 7.0 : 3.0;
		const TArray<FVector2f> Tick = { FVector2f(GSTNHitDataDial::PointOnArc(Pivot, Radius - Long, static_cast<float>(Degrees))), FVector2f(GSTNHitDataDial::PointOnArc(Pivot, Radius, static_cast<float>(Degrees))) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Tick, ESlateDrawEffect::None, DialTick * Tint, true, 1.f);
	}

	// The needle with a small head at its tip, and the pivot
	const float Shown = FMath::Clamp(Angle.Get(0.f), GSTNHitDataDial::MinAngle, GSTNHitDataDial::MaxAngle);
	const FVector2D Tip = GSTNHitDataDial::PointOnArc(Pivot, Radius, Shown);
	const TArray<FVector2f> Needle = { FVector2f(Pivot), FVector2f(Tip) };
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Needle, ESlateDrawEffect::None, DialNeedle * Tint, true, 2.5f);
	const FSlateBrush* Dot = FAppStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(FVector2f(9.f, 9.f), FSlateLayoutTransform(FVector2f(Tip - FVector2D(4.5, 4.5)))), Dot, ESlateDrawEffect::None, DialNeedle * Tint);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(FVector2f(7.f, 7.f), FSlateLayoutTransform(FVector2f(Pivot - FVector2D(3.5, 3.5)))), Dot, ESlateDrawEffect::None, DialPlane * Tint);
	return LayerId + 4;
}

FReply SGSTN_AngleDial::Drag(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D Pivot;
	double Radius = 0.0;
	GetArc(MyGeometry.GetLocalSize(), Pivot, Radius);
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float NewAngle = GSTNHitDataDial::AngleFromPoint(Pivot, Local, GSTNHitDataDial::GetSnapDegrees(MouseEvent.IsShiftDown(), MouseEvent.IsControlDown()));
	if (!FMath::IsNearlyEqual(NewAngle, LastDragAngle))
	{
		LastDragAngle = NewAngle;
		OnAngleChanged.ExecuteIfBound(NewAngle);
	}
	return FReply::Handled();
}

FReply SGSTN_AngleDial::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || bReadOnly.Get(false))
	{
		return FReply::Unhandled();
	}
	bDragging = true;
	DragStartAngle = Angle.Get(0.f);
	LastDragAngle = DragStartAngle;
	OnDragBegin.ExecuteIfBound();
	Drag(MyGeometry, MouseEvent);
	return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
}

FReply SGSTN_AngleDial::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bDragging ? Drag(MyGeometry, MouseEvent) : FReply::Unhandled();
}

FReply SGSTN_AngleDial::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bDragging)
	{
		return FReply::Unhandled();
	}
	EndDrag(LastDragAngle);
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SGSTN_AngleDial::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape puts the angle back and ends the drag (the row cancels its transaction)
	if (bDragging && InKeyEvent.GetKey() == EKeys::Escape)
	{
		LastDragAngle = DragStartAngle;
		OnAngleChanged.ExecuteIfBound(DragStartAngle);
		EndDrag(DragStartAngle);
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void SGSTN_AngleDial::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	if (bDragging)
	{
		EndDrag(LastDragAngle);
	}
}

void SGSTN_AngleDial::EndDrag(float AngleDeg)
{
	bDragging = false;
	OnDragEnd.ExecuteIfBound(AngleDeg);
}

FCursorReply SGSTN_AngleDial::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return bReadOnly.Get(false) ? FCursorReply::Unhandled() : FCursorReply::Cursor(EMouseCursor::Crosshairs);
}

// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Details/SGSTN_YawCompass.h"

#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Timeline/Details/GSTN_HitDataMath.h"

namespace
{
	// The same palette as the elevation dial, so the two read as one control
	const FLinearColor CompassRing(0.45f, 0.45f, 0.5f, 1.f);
	const FLinearColor CompassSource(0.7f, 0.7f, 0.75f, 1.f);
	const FLinearColor CompassArrow(0.95f, 0.45f, 0.2f, 1.f);
}

void SGSTN_YawCompass::Construct(const FArguments& InArgs)
{
	YawOffset = InArgs._YawOffset;
	bDimmed = InArgs._bDimmed;
}

int32 SGSTN_YawCompass::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Centre = Size * 0.5;
	const double Radius = FMath::Max(FMath::Min(Size.X, Size.Y) * 0.5 - 5.0, 8.0);
	const FLinearColor Tint(1.f, 1.f, 1.f, bDimmed.Get(false) ? 0.4f : 1.f);

	// The ring
	TArray<FVector2f> Ring;
	for (int32 Degrees = 0; Degrees <= 360; Degrees += 10)
	{
		Ring.Add(FVector2f(GSTNHitDataDial::YawArrowTip(Centre, Radius, static_cast<float>(Degrees))));
	}
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, CompassRing * Tint, true, 1.f);

	// The yaw source's way (offset 0), then the launch with a head at its tip
	const TArray<FVector2f> Source = { FVector2f(Centre), FVector2f(GSTNHitDataDial::YawArrowTip(Centre, Radius, 0.f)) };
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Source, ESlateDrawEffect::None, CompassSource * Tint, true, 1.f);

	const float Offset = YawOffset.Get(0.f);
	const FVector2D Tip = GSTNHitDataDial::YawArrowTip(Centre, Radius, Offset);
	const FVector2D HeadLeft = GSTNHitDataDial::YawArrowTip(Tip, 7.0, Offset + 150.f);
	const FVector2D HeadRight = GSTNHitDataDial::YawArrowTip(Tip, 7.0, Offset - 150.f);
	const TArray<FVector2f> Arrow = { FVector2f(Centre), FVector2f(Tip), FVector2f(HeadLeft), FVector2f(Tip), FVector2f(HeadRight) };
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, CompassArrow * Tint, true, 2.f);

	const FSlateBrush* Dot = FAppStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(FVector2f(7.f, 7.f), FSlateLayoutTransform(FVector2f(Centre - FVector2D(3.5, 3.5)))), Dot, ESlateDrawEffect::None, CompassSource * Tint);
	return LayerId + 4;
}

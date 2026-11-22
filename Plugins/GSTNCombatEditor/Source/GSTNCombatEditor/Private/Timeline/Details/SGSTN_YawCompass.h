// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

/**
 * The sideways turn of a launch (FGSTN_HitData::YawOffsetDeg) seen from above: the attacker at the centre, the yaw source's way pointing up the
 * widget (a dim line), the launch as an orange arrow turned by the offset, right of the line for a positive offset. Read only: the number row next
 * to it writes the value, so there is one way to edit it and one undo step per change. The geometry is GSTNHitDataDial::YawArrowTip, which the
 * tests cover.
 */
class SGSTN_YawCompass : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_YawCompass) {}
		/** The offset shown, in degrees */
		SLATE_ATTRIBUTE(float, YawOffset)
		/** AutoLink ignores the offset while the hitbox moves: the compass shows it dimmed */
		SLATE_ATTRIBUTE(bool, bDimmed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The size the compass asks for: 70 x 70 */
	static FVector2D GetCompassSize() { return FVector2D(70.0, 70.0); }

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return GetCompassSize(); }

private:
	TAttribute<float> YawOffset;
	TAttribute<bool> bDimmed;
};

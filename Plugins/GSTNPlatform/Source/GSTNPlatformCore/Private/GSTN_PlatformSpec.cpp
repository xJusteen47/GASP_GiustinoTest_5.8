// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PlatformSpec.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformSpec)

#define LOCTEXT_NAMESPACE "GSTNPlatformSpec"

int32 FGSTN_PlatformSpec::FindShapeIndex(int32 ShapeId) const
{
	return Shapes.IndexOfByPredicate([ShapeId](const FGSTN_PlatformShape& Shape) { return Shape.ShapeId == ShapeId; });
}

uint8 FGSTN_PlatformSpec::GetShapeIdMask() const
{
	uint8 Mask = 0;
	for (const FGSTN_PlatformShape& Shape : Shapes)
	{
		if (Shape.ShapeId < GSTNPlatform::MaxShapes)
		{
			Mask |= uint8(1u << Shape.ShapeId);
		}
	}
	return Mask;
}

bool FGSTN_PlatformSpec::CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	const int32 ErrorsBefore = OutErrors.Num();

	if (Shapes.Num() > GSTNPlatform::MaxShapes)
	{
		OutErrors.Add(FText::Format(LOCTEXT("TooManyShapes", "A platform has at most {0} shapes, this one has {1}"),
			GSTNPlatform::MaxShapes, Shapes.Num()));
	}

	uint8 Seen = 0;
	for (int32 Index = 0; Index < Shapes.Num(); ++Index)
	{
		const FGSTN_PlatformShape& Shape = Shapes[Index];
		if (Shape.ShapeId >= GSTNPlatform::MaxShapes)
		{
			OutErrors.Add(FText::Format(LOCTEXT("IdOutOfRange", "Shape {0}: ShapeId {1} is out of 0..{2}"), Index, int32(Shape.ShapeId),
				GSTNPlatform::MaxShapes - 1));
			continue;
		}
		const uint8 Bit = uint8(1u << Shape.ShapeId);
		if (Seen & Bit)
		{
			OutErrors.Add(FText::Format(LOCTEXT("IdRepeated", "Shape {0}: ShapeId {1} is used twice"), Index, int32(Shape.ShapeId)));
		}
		Seen |= Bit;

		const bool bDegenerate = Shape.Type == EGSTN_PlatformShapeType::Box
			? (Shape.Extent.X <= 0.0 || Shape.Extent.Y <= 0.0 || Shape.Extent.Z <= 0.0)
			: Shape.Type == EGSTN_PlatformShapeType::Sphere
				? Shape.Extent.X <= 0.0
				: (Shape.Extent.X <= 0.0 || Shape.Extent.Z < Shape.Extent.X);
		if (bDegenerate || Shape.Extent.ContainsNaN())
		{
			OutErrors.Add(FText::Format(LOCTEXT("Degenerate", "Shape {0} (id {1}): degenerate extent {2}"), Index, int32(Shape.ShapeId),
				FText::FromString(Shape.Extent.ToString())));
		}
	}

	if (DropGraceFrames < 0)
	{
		OutErrors.Add(LOCTEXT("NegativeGrace", "DropGraceFrames must not be negative"));
	}

	return OutErrors.Num() == ErrorsBefore;
}

#undef LOCTEXT_NAMESPACE

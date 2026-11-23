// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PlatformShape.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformShape)

namespace GSTNPlatform
{
	FName GetShapeComponentName(int32 ShapeId)
	{
		// Not FName("Surface", Number): the number suffix would print as "Surface_0"
		return FName(*FString::Printf(TEXT("Surface%d"), ShapeId));
	}
}

FTransform FGSTN_PlatformShape::ComposeWorld(const FTransform& OwnerPose) const
{
	const FTransform Unscaled(OwnerPose.GetRotation(), OwnerPose.GetLocation());
	return GetLocalTransform() * Unscaled;
}

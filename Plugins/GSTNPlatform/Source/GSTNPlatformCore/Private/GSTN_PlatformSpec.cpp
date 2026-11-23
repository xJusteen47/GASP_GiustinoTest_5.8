// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PlatformSpec.h"

#include "GSTN_SurfaceTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformSpec)

#define LOCTEXT_NAMESPACE "GSTNPlatformSpec"

namespace GSTNPlatformSpecPrivate
{
	/** The six face normals of a box shape in the enemy's frame */
	void GetBoxFaceNormals(const FGSTN_PlatformShape& Shape, FVector OutNormals[6])
	{
		const FQuat Rotation = Shape.LocalRotation.Quaternion();
		const FVector Axes[3] = { Rotation.GetAxisX(), Rotation.GetAxisY(), Rotation.GetAxisZ() };
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			OutNormals[Axis * 2] = Axes[Axis];
			OutNormals[Axis * 2 + 1] = -Axes[Axis];
		}
	}

	/** Angle between a normal and the enemy's up (deg): 0 is a floor, 90 a vertical wall, more an overhang */
	double AngleFromUp(const FVector& Normal)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Normal.GetSafeNormal().Z, -1.0, 1.0)));
	}
}

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

int32 FGSTN_PlatformSpec::GetDropGraceFrames(const FGSTN_PlatformShape& Shape) const
{
	return FMath::Max(0, Shape.DropGraceFrames >= 0 ? Shape.DropGraceFrames : DropGraceFrames);
}

uint8 FGSTN_PlatformSpec::GetActionStateMask(EGSTN_PlatformActionState State) const
{
	const int32 Bit = GSTNPlatform::ActionBit(State);
	uint8 Mask = 0;
	for (const FGSTN_PlatformShape& Shape : Shapes)
	{
		const int32 Allowed = Shape.bOverrideActiveActionStates ? Shape.ActiveActionStates : ActiveActionStates;
		if (Shape.ShapeId < GSTNPlatform::MaxShapes && (Allowed & Bit) != 0)
		{
			Mask |= uint8(1u << Shape.ShapeId);
		}
	}
	return Mask;
}

const FGSTN_OccupantResponse& FGSTN_PlatformSpec::GetOccupantResponse(const FGSTN_PlatformShape& Shape) const
{
	return Shape.bOverrideOccupantResponse ? Shape.OccupantResponse : DefaultOccupantResponse;
}

bool FGSTN_PlatformSpec::CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings, const FGSTN_PlatformValidationContext& Context) const
{
	using namespace GSTNPlatformSpecPrivate;
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
		const int32 Id = Shape.ShapeId;
		if (Shape.ShapeId >= GSTNPlatform::MaxShapes)
		{
			OutErrors.Add(FText::Format(LOCTEXT("IdOutOfRange", "Shape {0}: ShapeId {1} is out of 0..{2}"), Index, Id, GSTNPlatform::MaxShapes - 1));
			continue;
		}
		const uint8 Bit = uint8(1u << Shape.ShapeId);
		if (Seen & Bit)
		{
			OutErrors.Add(FText::Format(LOCTEXT("IdRepeated", "Shape {0}: ShapeId {1} is used twice"), Index, Id));
		}
		Seen |= Bit;

		const bool bDegenerate = Shape.Type == EGSTN_PlatformShapeType::Box
			? (Shape.Extent.X <= 0.0 || Shape.Extent.Y <= 0.0 || Shape.Extent.Z <= 0.0)
			: Shape.Type == EGSTN_PlatformShapeType::Sphere
				? Shape.Extent.X <= 0.0
				: (Shape.Extent.X <= 0.0 || Shape.Extent.Z < Shape.Extent.X);
		if (bDegenerate || Shape.Extent.ContainsNaN())
		{
			OutErrors.Add(FText::Format(LOCTEXT("Degenerate", "Shape {0} (id {1}): degenerate extent {2}"), Index, Id,
				FText::FromString(Shape.Extent.ToString())));
		}

		// Never a bone, never a primitive without a flat top for standing (traversal-surfaces.md 4.1)
		if (!Shape.Bone.IsNone())
		{
			OutErrors.Add(FText::Format(LOCTEXT("Bone", "Shape {0} (id {1}) names bone {2}: platform shapes never follow a bone (animation is not deterministic across machines); author it in the enemy's frame and clear Bone"),
				Index, Id, FText::FromName(Shape.Bone)));
		}

		const FGSTN_SurfaceBehaviour& Behaviour = Shape.Behaviour;
		const bool bStand = Behaviour.HasBehaviour(GSTNSurfaceTags::Behaviour_Stand);
		const bool bWall = Behaviour.HasBehaviour(GSTNSurfaceTags::Behaviour_WallSlide) || Behaviour.HasBehaviour(GSTNSurfaceTags::Behaviour_WallClimb);
		const bool bLaunch = Behaviour.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch);

		if (bStand && Shape.Type != EGSTN_PlatformShapeType::Box)
		{
			OutErrors.Add(FText::Format(LOCTEXT("StandNotBox", "Shape {0} (id {1}) offers Stand on a {2}: a sphere or capsule top fails the walkable test at its edges; use a Box"),
				Index, Id, Shape.Type == EGSTN_PlatformShapeType::Capsule ? LOCTEXT("Capsule", "capsule") : LOCTEXT("Sphere", "sphere")));
		}

		if (Shape.Type == EGSTN_PlatformShapeType::Box && (bStand || bWall))
		{
			FVector Normals[6];
			GetBoxFaceNormals(Shape, Normals);
			bool bHasWalkableTop = false;
			bool bHasWallFace = false;
			for (const FVector& Normal : Normals)
			{
				const double Angle = AngleFromUp(Normal);
				bHasWalkableTop |= Angle <= Context.MaxStandAngle;
				bHasWallFace |= Angle >= Context.MinWallSurfaceAngle && Angle <= Context.MaxWallSurfaceAngle;
			}
			if (bStand && !bHasWalkableTop)
			{
				OutWarnings.Add(FText::Format(LOCTEXT("NoWalkableTop", "Shape {0} (id {1}) offers Stand but no face is walkable in the enemy's upright pose"), Index, Id));
			}
			if (bWall && !bHasWallFace)
			{
				OutWarnings.Add(FText::Format(LOCTEXT("NoWallFace", "Shape {0} (id {1}) offers WallSlide/WallClimb but no face is within the wall modes' {2}..{3} degrees in the enemy's upright pose: players will never grab it"),
					Index, Id, Context.MinWallSurfaceAngle, Context.MaxWallSurfaceAngle));
			}
		}

		TArray<FText> BehaviourErrors;
		if (!Behaviour.Validate(BehaviourErrors))
		{
			for (const FText& Error : BehaviourErrors)
			{
				OutErrors.Add(FText::Format(LOCTEXT("BehaviourError", "Shape {0} (id {1}): {2}"), Index, Id, Error));
			}
		}

		if (!bStand && !bWall && !bLaunch && !Behaviour.SurfaceType.IsValid())
		{
			OutWarnings.Add(FText::Format(LOCTEXT("OffersNothing", "Shape {0} (id {1}) offers no traversal behaviour and no surface type: it ignores characters and nothing reads it"), Index, Id));
		}

		if (!Shape.PartName.IsNone() && !Context.bPartNamesResolved)
		{
			OutWarnings.Add(FText::Format(LOCTEXT("PartWithoutBridge", "Shape {0} (id {1}) names part {2}, but no part bridge (GSTNPlatformParts) resolves it: the shape behaves as body (always active while the platform is)"),
				Index, Id, FText::FromName(Shape.PartName)));
		}

		if (Shape.DropGraceFrames > 255)
		{
			OutErrors.Add(FText::Format(LOCTEXT("GraceTooLong", "Shape {0} (id {1}): DropGraceFrames is above 255"), Index, Id));
		}
	}

	if (DropGraceFrames < 0)
	{
		OutErrors.Add(LOCTEXT("NegativeGrace", "DropGraceFrames must not be negative"));
	}

	return OutErrors.Num() == ErrorsBefore;
}

#undef LOCTEXT_NAMESPACE

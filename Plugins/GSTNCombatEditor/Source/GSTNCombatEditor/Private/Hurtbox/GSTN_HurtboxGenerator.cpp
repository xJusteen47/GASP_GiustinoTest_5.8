// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Hurtbox/GSTN_HurtboxGenerator.h"

#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ReferenceSkeleton.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/** A primitive of a body, already reduced to a shape in the body's (bone's) local space */
	struct FPrimitive
	{
		FGSTN_HitShape Shape;
		bool bBoundingFallback = false;
		FText FallbackNote;
	};

	void SetExtentOf(FGSTN_HitShape& Shape, double Radius, double HalfHeight)
	{
		Shape.Extent = FVector(Radius, Radius, HalfHeight);
	}

	FPrimitive FromSphere(const FKSphereElem& Elem)
	{
		FPrimitive Primitive;
		Primitive.Shape.Type = EGSTN_ShapeType::Sphere;
		SetExtentOf(Primitive.Shape, Elem.Radius, Elem.Radius);
		Primitive.Shape.Local = FTransform(FQuat::Identity, Elem.Center);
		return Primitive;
	}

	FPrimitive FromBox(const FKBoxElem& Elem)
	{
		FPrimitive Primitive;
		Primitive.Shape.Type = EGSTN_ShapeType::Box;
		// The Physics Asset stores the full lengths; a hit shape stores half extents
		Primitive.Shape.Extent = FVector(Elem.X * 0.5, Elem.Y * 0.5, Elem.Z * 0.5);
		Primitive.Shape.Local = FTransform(Elem.Rotation, Elem.Center);
		return Primitive;
	}

	FPrimitive FromSphyl(const FKSphylElem& Elem)
	{
		FPrimitive Primitive;
		Primitive.Shape.Type = EGSTN_ShapeType::Capsule;
		// Length is the segment between the hemisphere centres; a hit shape's half height includes the hemispheres
		SetExtentOf(Primitive.Shape, Elem.Radius, Elem.Radius + Elem.Length * 0.5);
		Primitive.Shape.Local = FTransform(Elem.Rotation, Elem.Center);
		return Primitive;
	}

	/** The capsule that bounds the convex hull's box, its axis along the longest side of the box */
	FPrimitive FromConvex(const FKConvexElem& Elem)
	{
		FPrimitive Primitive;
		Primitive.bBoundingFallback = true;
		Primitive.FallbackNote = LOCTEXT("ConvexFallback", "is a convex body; the hurtbox is the capsule that bounds it, along its longest axis.");
		Primitive.Shape.Type = EGSTN_ShapeType::Capsule;

		const FVector BoxCentre = Elem.ElemBox.GetCenter();
		const FVector Half = Elem.ElemBox.GetExtent();
		int32 Axis = 2;
		if (Half.X >= Half.Y && Half.X >= Half.Z)
		{
			Axis = 0;
		}
		else if (Half.Y >= Half.Z)
		{
			Axis = 1;
		}
		const double HalfLength = Half[Axis];
		double Radius = 0.0;
		for (int32 Other = 0; Other < 3; ++Other)
		{
			if (Other != Axis)
			{
				Radius = FMath::Max(Radius, Half[Other]);
			}
		}
		Radius = FMath::Min(Radius, HalfLength);
		SetExtentOf(Primitive.Shape, Radius, HalfLength);

		// The capsule axis is local Z; turn Z onto the longest side of the box
		const FVector AxisDirection = Axis == 0 ? FVector::XAxisVector : (Axis == 1 ? FVector::YAxisVector : FVector::ZAxisVector);
		const FQuat TurnZ = FQuat::FindBetweenNormals(FVector::ZAxisVector, AxisDirection);
		Primitive.Shape.Local = FTransform(TurnZ, BoxCentre) * Elem.GetTransform();
		return Primitive;
	}

	/** The capsule that bounds a tapered capsule: the larger radius all along, the half height of the longer end */
	FPrimitive FromTapered(const FKTaperedCapsuleElem& Elem)
	{
		FPrimitive Primitive;
		Primitive.bBoundingFallback = true;
		Primitive.FallbackNote = LOCTEXT("TaperedFallback", "is a tapered capsule; the hurtbox is the capsule that bounds it.");
		Primitive.Shape.Type = EGSTN_ShapeType::Capsule;
		const double Radius = FMath::Max(Elem.Radius0, Elem.Radius1);
		SetExtentOf(Primitive.Shape, Radius, Radius + Elem.Length * 0.5);
		Primitive.Shape.Local = FTransform(Elem.Rotation, Elem.Center);
		return Primitive;
	}

	/** Scales the extent of a shape by a transform scale, the way the hit maths reads a scaled Local (sphere by the largest, capsule radius by the larger of X and Y) */
	void BakeScaleIntoExtent(FGSTN_HitShape& Shape, const FVector& Scale)
	{
		const FVector S = Scale.GetAbs();
		switch (Shape.Type)
		{
		case EGSTN_ShapeType::Sphere:
			{
				const double R = Shape.Extent.X * S.GetMax();
				Shape.Extent = FVector(R, R, R);
				break;
			}
		case EGSTN_ShapeType::Capsule:
			{
				const double R = Shape.Extent.X * FMath::Max(S.X, S.Y);
				Shape.Extent = FVector(R, R, FMath::Max(Shape.Extent.Z * S.Z, R));
				break;
			}
		case EGSTN_ShapeType::Box:
			Shape.Extent = FVector(Shape.Extent.X * S.X, Shape.Extent.Y * S.Y, Shape.Extent.Z * S.Z);
			break;
		}
	}
}

FGSTN_HurtboxGenOptions FGSTN_HurtboxGenOptions::FromSettings(const UGSTN_CombatEditorSettings& Settings)
{
	FGSTN_HurtboxGenOptions Options;
	Options.ZoneRules = Settings.HurtboxZoneRules;
	return Options;
}

FGameplayTag GSTNHurtboxGenerator::SuggestZone(TConstArrayView<FGSTN_HurtboxZoneRule> Rules, FName BoneName)
{
	const FString Bone = BoneName.ToString();
	for (const FGSTN_HurtboxZoneRule& Rule : Rules)
	{
		// An empty fragment would match every bone: a half-edited line must suggest nothing, not everything
		if (!Rule.BoneNameContains.IsEmpty() && Bone.Contains(Rule.BoneNameContains, ESearchCase::IgnoreCase))
		{
			return Rule.Zone;
		}
	}
	return FGameplayTag();
}

double GSTNHurtboxGenerator::GetBoundingRadius(const FGSTN_HitShape& Shape)
{
	switch (Shape.Type)
	{
	case EGSTN_ShapeType::Sphere: return Shape.Extent.X;
	case EGSTN_ShapeType::Capsule: return FMath::Max(Shape.Extent.Z, Shape.Extent.X);
	case EGSTN_ShapeType::Box: return Shape.Extent.Size();
	}
	return 0.0;
}

double GSTNHurtboxGenerator::GetLargestExtent(const FGSTN_HitShape& Shape)
{
	switch (Shape.Type)
	{
	case EGSTN_ShapeType::Sphere: return Shape.Extent.X;
	case EGSTN_ShapeType::Capsule: return FMath::Max(Shape.Extent.Z, Shape.Extent.X);
	case EGSTN_ShapeType::Box: return Shape.Extent.GetMax();
	}
	return 0.0;
}

void GSTNHurtboxGenerator::ExpandShapeToInclude(FGSTN_HitShape& Shape, const FVector& Centre, double Radius)
{
	switch (Shape.Type)
	{
	case EGSTN_ShapeType::Sphere:
		{
			const double R = FMath::Max(Shape.Extent.X, Centre.Size() + Radius);
			Shape.Extent = FVector(R, R, R);
			break;
		}
	case EGSTN_ShapeType::Capsule:
		{
			const double Lateral = FMath::Sqrt(Centre.X * Centre.X + Centre.Y * Centre.Y);
			const double R = FMath::Max(Shape.Extent.X, Lateral + Radius);
			const double H = FMath::Max3(Shape.Extent.Z, FMath::Abs(Centre.Z) + Radius, R);
			Shape.Extent = FVector(R, R, H);
			break;
		}
	case EGSTN_ShapeType::Box:
		Shape.Extent = FVector(
			FMath::Max(Shape.Extent.X, FMath::Abs(Centre.X) + Radius),
			FMath::Max(Shape.Extent.Y, FMath::Abs(Centre.Y) + Radius),
			FMath::Max(Shape.Extent.Z, FMath::Abs(Centre.Z) + Radius));
		break;
	}
}

FGSTN_HurtboxGenResult GSTNHurtboxGenerator::FromPhysicsAsset(const UPhysicsAsset& PhysicsAsset, const USkeletalMesh& Mesh, const FGSTN_HurtboxGenOptions& Options)
{
	return FromPhysicsAsset(PhysicsAsset, Mesh.GetRefSkeleton(), Options);
}

FGSTN_HurtboxGenResult GSTNHurtboxGenerator::FromPhysicsAsset(const UPhysicsAsset& PhysicsAsset, const FReferenceSkeleton& RefSkeleton, const FGSTN_HurtboxGenOptions& Options)
{
	FGSTN_HurtboxGenResult Result;

	// The reference pose of every bone in component space. Parents come before their children in a reference skeleton
	const TArray<FTransform>& LocalPose = RefSkeleton.GetRefBonePose();
	TArray<FTransform> ComponentPose;
	ComponentPose.SetNum(LocalPose.Num());
	for (int32 Bone = 0; Bone < LocalPose.Num(); ++Bone)
	{
		const int32 Parent = RefSkeleton.GetParentIndex(Bone);
		ComponentPose[Bone] = Parent == INDEX_NONE ? LocalPose[Bone] : LocalPose[Bone] * ComponentPose[Parent];
	}

	TSet<FName> UsedNames;
	TArray<int32> BoneOfHurtbox;
	const double Margin = Options.ScaleMargin > 0.f ? Options.ScaleMargin : 1.0;

	for (const TObjectPtr<USkeletalBodySetup>& BodyPtr : PhysicsAsset.SkeletalBodySetups)
	{
		const USkeletalBodySetup* Body = BodyPtr.Get();
		if (Body == nullptr)
		{
			continue;
		}
		++Result.BodyCount;

		const FKAggregateGeom& Geom = Body->AggGeom;
		TArray<FPrimitive> Primitives;
		for (const FKSphereElem& Elem : Geom.SphereElems) { Primitives.Add(FromSphere(Elem)); }
		for (const FKBoxElem& Elem : Geom.BoxElems) { Primitives.Add(FromBox(Elem)); }
		for (const FKSphylElem& Elem : Geom.SphylElems) { Primitives.Add(FromSphyl(Elem)); }
		for (const FKConvexElem& Elem : Geom.ConvexElems) { Primitives.Add(FromConvex(Elem)); }
		for (const FKTaperedCapsuleElem& Elem : Geom.TaperedCapsuleElems) { Primitives.Add(FromTapered(Elem)); }
		Result.PrimitiveCount += Primitives.Num();

		if (Options.bSkipKinematic && Body->PhysicsType == PhysType_Kinematic)
		{
			++Result.SkippedKinematic;
			continue;
		}
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Body->BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			++Result.SkippedNoBone;
			Result.Warnings.Add(FText::Format(LOCTEXT("BodyBoneMissing", "The body of bone {0} was skipped: the mesh has no such bone."), FText::FromName(Body->BoneName)));
			continue;
		}

		for (int32 Index = 0; Index < Primitives.Num(); ++Index)
		{
			FPrimitive& Primitive = Primitives[Index];
			FGSTN_Hurtbox Hurtbox;
			Hurtbox.Bone = Body->BoneName;

			// bone for a body with one primitive, bone_N for a body with several (N counts from 0)
			FName Name = Primitives.Num() == 1 ? Body->BoneName : FName(*FString::Printf(TEXT("%s_%d"), *Body->BoneName.ToString(), Index));
			for (int32 Bump = Primitives.Num(); UsedNames.Contains(Name); ++Bump)
			{
				Name = FName(*FString::Printf(TEXT("%s_%d"), *Body->BoneName.ToString(), Bump));
			}
			UsedNames.Add(Name);
			Hurtbox.Name = Name;

			// Primitive in the bone's space -> bone -> component -> root. Applied left to right: a point is moved by the first, then the next
			FTransform ToRoot = Primitive.Shape.Local * ComponentPose[BoneIndex] * Options.MeshToRoot;
			Hurtbox.Shape = Primitive.Shape;
			const FVector Scale = ToRoot.GetScale3D();
			if (!Scale.Equals(FVector::OneVector, 1e-4))
			{
				BakeScaleIntoExtent(Hurtbox.Shape, Scale);
				ToRoot.SetScale3D(FVector::OneVector);
				Result.Warnings.Add(FText::Format(LOCTEXT("ScaledBone", "{0}: the bone has a scale in the reference pose; the extent was scaled by it."), FText::FromName(Name)));
			}
			ToRoot.NormalizeRotation();
			Hurtbox.Shape.Local = ToRoot;
			Hurtbox.Shape.Extent *= Margin;

			const FGameplayTag Zone = SuggestZone(Options.ZoneRules, Body->BoneName);
			if (Zone.IsValid())
			{
				Hurtbox.Tags.AddTag(Zone);
			}
			if (Primitive.bBoundingFallback)
			{
				++Result.BoundingFallbacks;
				Result.Warnings.Add(FText::Format(LOCTEXT("PrimitiveFallback", "{0} {1}"), FText::FromName(Name), Primitive.FallbackNote));
			}
			Result.Hurtboxes.Add(Hurtbox);
			BoneOfHurtbox.Add(BoneIndex);
		}
	}

	// Small bodies into the nearest ancestor that has a big one
	if (Options.MergeBelowCm > 0.f)
	{
		const int32 Num = Result.Hurtboxes.Num();
		TArray<bool> bSmall;
		bSmall.Init(false, Num);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			bSmall[Index] = GetLargestExtent(Result.Hurtboxes[Index].Shape) < Options.MergeBelowCm;
		}

		TArray<bool> bMerged;
		bMerged.Init(false, Num);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (!bSmall[Index])
			{
				continue;
			}
			int32 Target = INDEX_NONE;
			for (int32 Bone = RefSkeleton.GetParentIndex(BoneOfHurtbox[Index]); Bone != INDEX_NONE && Target == INDEX_NONE; Bone = RefSkeleton.GetParentIndex(Bone))
			{
				for (int32 Other = 0; Other < Num; ++Other)
				{
					if (!bSmall[Other] && BoneOfHurtbox[Other] == Bone)
					{
						Target = Other;
						break;
					}
				}
			}
			if (Target == INDEX_NONE)
			{
				Result.Warnings.Add(FText::Format(LOCTEXT("SmallNoAncestor", "{0} is small but no bone above it has a bigger hurtbox, so it was kept."), FText::FromName(Result.Hurtboxes[Index].Name)));
				continue;
			}
			FGSTN_Hurtbox& Into = Result.Hurtboxes[Target];
			const FGSTN_Hurtbox& Small = Result.Hurtboxes[Index];
			const FVector CentreInTarget = Into.Shape.Local.InverseTransformPosition(Small.Shape.Local.GetLocation());
			ExpandShapeToInclude(Into.Shape, CentreInTarget, GetBoundingRadius(Small.Shape));
			bMerged[Index] = true;
			++Result.MergedSmall;
		}

		TArray<FGSTN_Hurtbox> Kept;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (!bMerged[Index])
			{
				Kept.Add(Result.Hurtboxes[Index]);
			}
		}
		Result.Hurtboxes = MoveTemp(Kept);
	}

	if (Result.Hurtboxes.Num() > 255)
	{
		Result.Warnings.Add(FText::Format(LOCTEXT("TooMany", "The generation made {0} hurtboxes and a set holds at most 255: merge small bodies or skip kinematic ones."), FText::AsNumber(Result.Hurtboxes.Num())));
	}
	return Result;
}

TArray<FGSTN_Hurtbox> GSTNHurtboxGenerator::Merge(const TArray<FGSTN_Hurtbox>& Existing, const TArray<FGSTN_Hurtbox>& Generated, EGSTN_RegenerateMode Mode, FGSTN_RegenerateSummary* OutSummary)
{
	FGSTN_RegenerateSummary Summary;
	TMap<FName, int32> GeneratedByName;
	for (int32 Index = 0; Index < Generated.Num(); ++Index)
	{
		GeneratedByName.Add(Generated[Index].Name, Index);
	}

	TArray<FGSTN_Hurtbox> Result;
	if (Mode == EGSTN_RegenerateMode::ReplaceAll)
	{
		Result = Generated;
		TSet<FName> ExistingNames;
		for (const FGSTN_Hurtbox& Old : Existing)
		{
			ExistingNames.Add(Old.Name);
			if (!GeneratedByName.Contains(Old.Name))
			{
				++Summary.Removed;
			}
		}
		for (const FGSTN_Hurtbox& New : Generated)
		{
			(ExistingNames.Contains(New.Name) ? Summary.Updated : Summary.Added)++;
		}
	}
	else
	{
		Result = Existing;
		TSet<FName> ExistingNames;
		for (FGSTN_Hurtbox& Old : Result)
		{
			ExistingNames.Add(Old.Name);
			if (const int32* Match = GeneratedByName.Find(Old.Name))
			{
				if (Mode == EGSTN_RegenerateMode::UpdateShapes)
				{
					Old.Shape = Generated[*Match].Shape;
					Old.Bone = Generated[*Match].Bone;
					++Summary.Updated;
				}
				else
				{
					++Summary.Kept;
				}
			}
			else
			{
				Summary.NotInGeneration.Add(Old.Name);
			}
		}
		for (const FGSTN_Hurtbox& New : Generated)
		{
			if (!ExistingNames.Contains(New.Name))
			{
				Result.Add(New);
				++Summary.Added;
			}
		}
	}

	if (OutSummary != nullptr)
	{
		*OutSummary = Summary;
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE

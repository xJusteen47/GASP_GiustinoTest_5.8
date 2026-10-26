// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_PhysicsAssetUtils.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

TArray<FGSTN_PhysicsPrimitive> UGSTN_PhysicsAssetUtils::ListPrimitives(const UPhysicsAsset* PhysicsAsset)
{
	TArray<FGSTN_PhysicsPrimitive> Out;
	if (!PhysicsAsset)
	{
		return Out;
	}
	for (const TObjectPtr<USkeletalBodySetup>& Body : PhysicsAsset->SkeletalBodySetups)
	{
		if (!Body)
		{
			continue;
		}
		const FKAggregateGeom& Geom = Body->AggGeom;
		for (const FKSphereElem& Sphere : Geom.SphereElems)
		{
			FGSTN_PhysicsPrimitive& Primitive = Out.AddDefaulted_GetRef();
			Primitive.Bone = Body->BoneName;
			Primitive.Shape.Type = EGSTN_ShapeType::Sphere;
			Primitive.Shape.Extent = FVector(Sphere.Radius);
			Primitive.Shape.Local = FTransform(Sphere.Center);
		}
		for (const FKSphylElem& Sphyl : Geom.SphylElems)
		{
			FGSTN_PhysicsPrimitive& Primitive = Out.AddDefaulted_GetRef();
			Primitive.Bone = Body->BoneName;
			Primitive.Shape.Type = EGSTN_ShapeType::Capsule;
			Primitive.Shape.Extent = FVector(Sphyl.Radius, Sphyl.Radius, Sphyl.Length * 0.5 + Sphyl.Radius);
			Primitive.Shape.Local = FTransform(Sphyl.Rotation, Sphyl.Center);
		}
		for (const FKBoxElem& Box : Geom.BoxElems)
		{
			FGSTN_PhysicsPrimitive& Primitive = Out.AddDefaulted_GetRef();
			Primitive.Bone = Body->BoneName;
			Primitive.Shape.Type = EGSTN_ShapeType::Box;
			Primitive.Shape.Extent = FVector(Box.X * 0.5, Box.Y * 0.5, Box.Z * 0.5);
			Primitive.Shape.Local = FTransform(Box.Rotation, Box.Center);
		}
	}
	return Out;
}

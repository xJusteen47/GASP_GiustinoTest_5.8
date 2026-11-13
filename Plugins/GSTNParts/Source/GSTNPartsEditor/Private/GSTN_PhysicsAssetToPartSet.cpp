#include "GSTN_PhysicsAssetToPartSet.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

TArray<FGSTN_PartDefinition> UGSTN_PhysicsAssetToPartSet::MakeDraftParts(const UPhysicsAsset* PhysicsAsset)
{
	TArray<FGSTN_PartDefinition> Parts;
	if (!PhysicsAsset)
	{
		return Parts;
	}

	// Part names key the bridges (platform shapes, events) and must be unique, whereas a bone can carry several bodies
	TSet<FName> Used;
	for (const USkeletalBodySetup* Body : PhysicsAsset->SkeletalBodySetups)
	{
		if (!Body)
		{
			continue;
		}
		FName Name = Body->BoneName;
		for (int32 Suffix = 1; Used.Contains(Name); ++Suffix)
		{
			Name = FName(*FString::Printf(TEXT("%s_%d"), *Body->BoneName.ToString(), Suffix));
		}
		Used.Add(Name);

		FGSTN_PartDefinition& Part = Parts.AddDefaulted_GetRef();
		Part.Name = Name;
		Part.Bones.Add(Body->BoneName);
	}
	return Parts;
}

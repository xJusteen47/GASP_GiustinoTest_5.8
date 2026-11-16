#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartDefinition.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PhysicsAssetToPartSet.generated.h"

class UPhysicsAsset;

/**
 * Starting point for authoring a part set from a Physics Asset (plan P13, the hook of the later part editor): one draft part per
 * body, ready to be merged, regrouped and tuned by hand. Thin on purpose: no UI, no guesses about hardness or caps.
 */
UCLASS()
class GSTNPARTSEDITOR_API UGSTN_PhysicsAssetToPartSet : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * One FGSTN_PartDefinition per body, in body order: Name is the bone name made unique (a second body on the same bone gets
	 * "_1", "_2"...), Bones holds that bone, every other field is the data default. The result may exceed the 16 part limit; the
	 * validator says so.
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Parts|Editor")
	static TArray<FGSTN_PartDefinition> MakeDraftParts(const UPhysicsAsset* PhysicsAsset);
};

// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitShape.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PhysicsAssetUtils.generated.h"

class UPhysicsAsset;

/** One collision primitive of a physics asset, as a hit shape in the space of the bone that owns the body */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_PhysicsPrimitive
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Hurtbox")
	FName Bone;

	/** Sphere / Capsule / Box with the extent convention of FGSTN_HitShape (capsule half height includes the hemispheres); Local is in bone space */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Hurtbox")
	FGSTN_HitShape Shape;
};

/**
 * Reads what Python cannot: the body setups of a physics asset are not exposed to reflection. Used by Tools/GAS/make_hurtbox_set.py, which turns the
 * primitives into a hurtbox set. Editor tooling, never called by the simulation.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_PhysicsAssetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Every sphere, capsule (sphyl) and box of every body, in the order of the asset's body setups */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hurtbox")
	static TArray<FGSTN_PhysicsPrimitive> ListPrimitives(const UPhysicsAsset* PhysicsAsset);
};

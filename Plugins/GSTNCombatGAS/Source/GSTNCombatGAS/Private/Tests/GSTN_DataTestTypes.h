// Test-only tags and an extension struct for the Automation tests of the kit data layer.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_DataTestTypes.generated.h"

namespace GSTNCombatGASTest
{
	/** Test leaves under the kit and weapon roots; production code never names a leaf (these exist only in the test module). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kit_TestA);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kit_TestB);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_TestA);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_TestB);
}

/** Data a test plugin would attach to a kit */
USTRUCT(BlueprintType)
struct FGSTN_TestKitExtensionA : public FGSTN_KitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;
};

/** A second extension type that no test entry holds */
USTRUCT(BlueprintType)
struct FGSTN_TestKitExtensionB : public FGSTN_KitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	float Strength = 0.f;
};

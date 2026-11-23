// Hit extension structs used only by the Core automation tests of the hit data.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
#include "GSTN_HitsTestTypes.generated.h"

USTRUCT(BlueprintType)
struct FGSTN_TestHitExtensionA : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct FGSTN_TestHitExtensionB : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct FGSTN_TestHitExtensionC : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;
};

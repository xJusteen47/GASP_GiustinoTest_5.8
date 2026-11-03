// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_KitExtension.generated.h"

/**
 * Base of the data an optional plugin attaches to a kit or a weapon (a shield's meter data, carry rules...), so the kit and
 * weapon assets never grow a property that names a feature (combat extension surface, modularity-and-tags 5b). Data only:
 * a derived struct holds tuning values, no behaviour.
 *
 * Nothing reads these yet; the kit and weapon assets arrive with M3 C5 and embed an FGSTN_KitExtensionList.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNCOMBATCORE_API FGSTN_KitExtension
{
	GENERATED_BODY()

	virtual ~FGSTN_KitExtension() = default;
};

/**
 * The extension array of a kit or weapon asset. An entry whose struct is not loaded (its plugin is disabled) is an empty
 * instanced struct: it is kept in the asset and ignored when read.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_KitExtensionList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extensions", meta = (BaseStruct = "/Script/GSTNCombatCore.GSTN_KitExtension", ExcludeBaseStruct))
	TArray<FInstancedStruct> ExtensionData;

	/** First extension of type ExtensionT, or null */
	template <typename ExtensionT>
	const ExtensionT* Find() const
	{
		for (const FInstancedStruct& Entry : ExtensionData)
		{
			if (const ExtensionT* Found = Entry.GetPtr<ExtensionT>())
			{
				return Found;
			}
		}
		return nullptr;
	}
};

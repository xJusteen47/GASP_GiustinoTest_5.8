// Loadout extension structs used only by the Core automation tests of the loadout data.

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "GSTN_LoadoutExtensionTestTypes.generated.h"

/** Travels with the reflected default format */
USTRUCT()
struct FGSTN_TestLoadoutExtA : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Level = 0;

	UPROPERTY()
	FString Label;
};

/** Travels with its own format (a version byte, then the value) */
USTRUCT()
struct FGSTN_TestLoadoutExtB : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	UPROPERTY()
	float Scale = 1.f;

	virtual bool SerializePayload(FArchive& Ar) override
	{
		uint8 Version = 1;
		Ar << Version;
		if (Version != 1)
		{
			Ar.SetError();
			return true;
		}
		Ar << Scale;
		return true;
	}
};

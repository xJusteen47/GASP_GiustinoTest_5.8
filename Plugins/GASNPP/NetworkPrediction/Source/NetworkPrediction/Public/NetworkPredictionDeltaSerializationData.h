// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionDeltaSerializationData.generated.h"

/**
 * 
 */
struct FAckedFrames
{
	TMap<uint32 , uint32> IDsToAckedFrames;
};


USTRUCT()
struct FSerializedAckedFrames
{
	GENERATED_USTRUCT_BODY()

	FSerializedAckedFrames() = default;

	FSerializedAckedFrames(const FAckedFrames& AckedFramesMap)
	{
		AckedFramesMap.IDsToAckedFrames.GenerateKeyArray(IDs);
		AckedFramesMap.IDsToAckedFrames.GenerateValueArray(AckedFrames);
	}
	

	UPROPERTY()
	TArray<uint32> IDs;
	
	UPROPERTY()
	TArray<uint32> AckedFrames;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& OutSuccess)
	{
		uint32 Num = Ar.IsSaving() ? IDs.Num() : 0;
		Ar.SerializeIntPacked(Num);
		
		if (Ar.IsLoading())
		{
			IDs.SetNum(Num);
			AckedFrames.SetNum(Num);
		}

		for (uint32 i = 0; i < Num; ++i)
		{
			Ar.SerializeIntPacked(IDs[i]);
			Ar.SerializeIntPacked(AckedFrames[i]);
		}
		
		OutSuccess = true;
		return true;
	}
};
template<>
struct TStructOpsTypeTraits<FSerializedAckedFrames> : public TStructOpsTypeTraitsBase2<FSerializedAckedFrames>
{
	enum
	{
		WithNetSerializer = true,
	};
};




struct FServerAckedFrames
{
	TMap<UNetConnection* , FAckedFrames> ConnectionsAckedFrames;
};
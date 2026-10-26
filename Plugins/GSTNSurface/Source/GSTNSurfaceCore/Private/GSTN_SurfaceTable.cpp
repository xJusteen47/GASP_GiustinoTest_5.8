// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceTable.h"

#include "GameplayTagsManager.h"
#include "Misc/Crc.h"
#include "Containers/StringConv.h"
#include "GSTN_SurfaceTags.h"
#include "GSTNSurfaceCoreModule.h"

void FGSTN_SurfaceTable::BakeMaterials(const TMap<const UPhysicalMaterial*, FGSTN_SurfaceBehaviour>& Entries)
{
	Materials = Entries;
	Materials.Remove(nullptr);
	bMaterialsBaked = true;
}

namespace
{
	/** The usable types in index order: valid, no duplicate, sorted by full name, at most FGSTN_SurfaceTable::MaxTypeIndex of them */
	TArray<FGameplayTag> OrderTypes(TArray<FGameplayTag> InTypes, bool bLogOverflow)
	{
		InTypes.RemoveAll([](const FGameplayTag& Tag) { return !Tag.IsValid(); });

		// Lexical order of the full names: independent of registration order, so equal tag sets give equal indices everywhere
		InTypes.Sort([](const FGameplayTag& A, const FGameplayTag& B)
		{
			return A.GetTagName().ToString() < B.GetTagName().ToString();
		});

		TArray<FGameplayTag> Ordered;
		for (const FGameplayTag& Type : InTypes)
		{
			if (Ordered.Contains(Type))
			{
				continue;
			}
			if (Ordered.Num() >= FGSTN_SurfaceTable::MaxTypeIndex)
			{
				if (bLogOverflow)
				{
					UE_LOG(LogGSTNSurface, Error, TEXT("More than %d Surface.Type tags: %s and the ones after it have no index"),
						FGSTN_SurfaceTable::MaxTypeIndex, *Type.ToString());
				}
				break;
			}
			Ordered.Add(Type);
		}
		return Ordered;
	}

	/** CRC of the count and of every name in order as UTF-8, so the value does not depend on the platform's TCHAR width */
	uint32 ChecksumOf(const TArray<FGameplayTag>& Ordered)
	{
		const int32 Count = Ordered.Num();
		uint32 Checksum = FCrc::MemCrc32(&Count, sizeof(int32), 0);
		for (const FGameplayTag& Type : Ordered)
		{
			const FTCHARToUTF8 Name(*Type.GetTagName().ToString());
			Checksum = FCrc::MemCrc32(Name.Get(), Name.Length() + 1, Checksum); // the terminator keeps "ab","c" apart from "a","bc"
		}
		return Checksum;
	}
}

bool FGSTN_SurfaceTable::BakeTypesFromTagManager()
{
	const FGameplayTagContainer Children = UGameplayTagsManager::Get().RequestGameplayTagChildren(GSTNSurfaceTags::Root_Type);
	TArray<FGameplayTag> Found;
	Children.GetGameplayTagArray(Found);
	return BakeTypes(MoveTemp(Found));
}

uint32 FGSTN_SurfaceTable::ComputeTypesChecksum(TArray<FGameplayTag> InTypes)
{
	return ChecksumOf(OrderTypes(MoveTemp(InTypes), false));
}

bool FGSTN_SurfaceTable::BakeTypes(TArray<FGameplayTag> InTypes)
{
	const TArray<FGameplayTag> Ordered = OrderTypes(MoveTemp(InTypes), !bTypesFrozen);
	const uint32 NewChecksum = ChecksumOf(Ordered);

	if (bTypesFrozen && bTypesBaked)
	{
		if (NewChecksum != TypesChecksum)
		{
			UE_LOG(LogGSTNSurface, Error, TEXT("The Surface.Type tags changed after the surface type table was frozen (checksum %08X, the tags now give %08X). The indices stay as they were, so the new types read as no type; machines whose tags differ will disagree on the types in the sync state. Add the tag before the engine starts, in a Config/Tags ini of the project or of a plugin that loads at startup"),
				TypesChecksum, NewChecksum);
			return false;
		}
		return true;
	}

	Types.Reset();
	TypeIndices.Reset();
	Types.Add(FGameplayTag());
	for (const FGameplayTag& Type : Ordered)
	{
		TypeIndices.Add(Type, static_cast<uint8>(Types.Num()));
		Types.Add(Type);
	}
	TypesChecksum = NewChecksum;
	bTypesBaked = true;
	return true;
}

const FGSTN_SurfaceBehaviour* FGSTN_SurfaceTable::FindMaterial(const UPhysicalMaterial* Material) const
{
	return Material ? Materials.Find(Material) : nullptr;
}

uint8 FGSTN_SurfaceTable::GetTypeIndex(const FGameplayTag& Type) const
{
	if (!Type.IsValid())
	{
		return 0;
	}
	const uint8* Index = TypeIndices.Find(Type);
	return Index ? *Index : 0;
}

FGameplayTag FGSTN_SurfaceTable::GetTypeByIndex(uint8 Index) const
{
	return Types.IsValidIndex(Index) ? Types[Index] : FGameplayTag();
}

void FGSTN_SurfaceTable::ResetMaterials()
{
	Materials.Reset();
	bMaterialsBaked = false;
}

void FGSTN_SurfaceTable::Reset()
{
	ResetMaterials();
	Types.Reset();
	TypeIndices.Reset();
	TypesChecksum = 0;
	bTypesBaked = false;
	bTypesFrozen = false;
}

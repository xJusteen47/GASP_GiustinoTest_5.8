// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceTable.h"

#include "GameplayTagsManager.h"
#include "GSTN_SurfaceTags.h"
#include "GSTNSurfaceCoreModule.h"

void FGSTN_SurfaceTable::BakeMaterials(const TMap<const UPhysicalMaterial*, FGSTN_SurfaceBehaviour>& Entries)
{
	Materials = Entries;
	Materials.Remove(nullptr);
	bMaterialsBaked = true;
}

void FGSTN_SurfaceTable::BakeTypesFromTagManager()
{
	const FGameplayTagContainer Children = UGameplayTagsManager::Get().RequestGameplayTagChildren(GSTNSurfaceTags::Root_Type);
	TArray<FGameplayTag> Found;
	Children.GetGameplayTagArray(Found);
	BakeTypes(MoveTemp(Found));
}

void FGSTN_SurfaceTable::BakeTypes(TArray<FGameplayTag> InTypes)
{
	InTypes.RemoveAll([](const FGameplayTag& Tag) { return !Tag.IsValid(); });

	// Lexical order of the full names: independent of registration order, so equal tag sets give equal indices everywhere
	InTypes.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.GetTagName().ToString() < B.GetTagName().ToString();
	});

	Types.Reset();
	TypeIndices.Reset();
	Types.Add(FGameplayTag());
	for (const FGameplayTag& Type : InTypes)
	{
		if (TypeIndices.Contains(Type))
		{
			continue;
		}
		if (Types.Num() > MaxTypeIndex)
		{
			UE_LOG(LogGSTNSurface, Error, TEXT("More than %d Surface.Type tags: %s and the ones after it have no index"),
				MaxTypeIndex, *Type.ToString());
			break;
		}
		TypeIndices.Add(Type, static_cast<uint8>(Types.Num()));
		Types.Add(Type);
	}
	bTypesBaked = true;
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

void FGSTN_SurfaceTable::Reset()
{
	Materials.Reset();
	Types.Reset();
	TypeIndices.Reset();
	bMaterialsBaked = false;
	bTypesBaked = false;
}

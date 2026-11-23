// Piggyback, editor part (editor only; the runtime never depends on it).

#include "GSTN_PairMoveFactories.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "GSTN_PairMoveDefinition.h"

#define LOCTEXT_NAMESPACE "GSTNPiggybackEditor"

namespace
{
	/** Gameplay > Combat, the section of the combat framework's own assets. Built on first use: no FText at static init */
	TConstArrayView<FAssetCategoryPath> CombatCategory()
	{
		static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(EAssetCategoryPaths::Gameplay, LOCTEXT("CombatCategory", "Combat"), ECategoryMenuType::Section) };
		return Categories;
	}

	/** The pair move ids of every definition the project has. Loads them: a project has a handful, and the id is not a registry tag */
	TSet<int32> CollectUsedPairMoveIds()
	{
		TSet<int32> Used;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UGSTN_PairMoveDefinition::StaticClass()->GetClassPathName(), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (const UGSTN_PairMoveDefinition* Definition = Cast<UGSTN_PairMoveDefinition>(Asset.GetAsset()))
			{
				Used.Add(Definition->PairMoveId);
			}
		}
		return Used;
	}
}

UGSTN_PairMoveDefinitionFactory::UGSTN_PairMoveDefinitionFactory()
{
	SupportedClass = UGSTN_PairMoveDefinition::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

int32 UGSTN_PairMoveDefinitionFactory::FindFreePairMoveId(const TSet<int32>& Used)
{
	for (int32 Id = 1; Id <= 32767; ++Id)
	{
		if (!Used.Contains(Id))
		{
			return Id;
		}
	}
	return 0;
}

UObject* UGSTN_PairMoveDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UGSTN_PairMoveDefinition* Definition = NewObject<UGSTN_PairMoveDefinition>(InParent, InClass, InName, Flags);
	const int32 Id = FindFreePairMoveId(CollectUsedPairMoveIds());
	if (Id > 0)
	{
		Definition->PairMoveId = Id;
	}
	return Definition;
}

UGSTN_PairMoveSetFactory::UGSTN_PairMoveSetFactory()
{
	SupportedClass = UGSTN_PairMoveSet::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UGSTN_PairMoveSetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UGSTN_PairMoveSet>(InParent, InClass, InName, Flags);
}

FText UGSTN_AssetDefinition_PairMoveDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("PairMoveDefinitionName", "Pair Move Definition");
}

FLinearColor UGSTN_AssetDefinition_PairMoveDefinition::GetAssetColor() const
{
	// Warm orange: link content, apart from the combat timeline and hurtbox colours
	return FLinearColor(0.9f, 0.45f, 0.1f);
}

TSoftClassPtr<UObject> UGSTN_AssetDefinition_PairMoveDefinition::GetAssetClass() const
{
	return UGSTN_PairMoveDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UGSTN_AssetDefinition_PairMoveDefinition::GetAssetCategories() const
{
	return CombatCategory();
}

FText UGSTN_AssetDefinition_PairMoveSet::GetAssetDisplayName() const
{
	return LOCTEXT("PairMoveSetName", "Pair Move Set");
}

FLinearColor UGSTN_AssetDefinition_PairMoveSet::GetAssetColor() const
{
	return FLinearColor(0.7f, 0.35f, 0.08f);
}

TSoftClassPtr<UObject> UGSTN_AssetDefinition_PairMoveSet::GetAssetClass() const
{
	return UGSTN_PairMoveSet::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UGSTN_AssetDefinition_PairMoveSet::GetAssetCategories() const
{
	return CombatCategory();
}

#undef LOCTEXT_NAMESPACE

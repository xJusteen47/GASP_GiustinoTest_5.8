// Piggyback, editor part (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "Factories/Factory.h"
#include "GSTN_PairMoveFactories.generated.h"

/**
 * Content Browser > Add > Gameplay > Combat > Pair Move Definition (combat-m7-piggyback.md 2b). A new definition gets the lowest PairMoveId no other
 * definition in the project uses, so two moves made one after the other do not collide on the wire id (the set validator still catches a
 * collision made by hand). Everything else keeps the class defaults: the designer names the abilities and the outcome.
 */
UCLASS()
class UGSTN_PairMoveDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UGSTN_PairMoveDefinitionFactory();

	/** The lowest id in 1..32767 that is not in Used; 0 when every id is taken */
	static int32 FindFreePairMoveId(const TSet<int32>& Used);

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

/** Content Browser > Add > Gameplay > Combat > Pair Move Set */
UCLASS()
class UGSTN_PairMoveSetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UGSTN_PairMoveSetFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

/** How the Content Browser shows a pair move definition: name, colour, the Gameplay > Combat section the factory's menu entry sits in */
UCLASS()
class UGSTN_AssetDefinition_PairMoveDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

/** How the Content Browser shows a pair move set */
UCLASS()
class UGSTN_AssetDefinition_PairMoveSet : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

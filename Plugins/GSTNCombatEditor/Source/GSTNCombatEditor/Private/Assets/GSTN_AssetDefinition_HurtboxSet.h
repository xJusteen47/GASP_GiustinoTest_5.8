// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "GSTN_AssetDefinition_HurtboxSet.generated.h"

/**
 * How the Content Browser treats a UGSTN_HurtboxSetDefinition: its name, colour and category, and that opening it opens the hurtbox set editor
 * (combat-m5-editor 8) rather than a plain data asset window. The engine finds it by reflection. No factory: sets are made as Data Assets of this class.
 */
UCLASS()
class UGSTN_AssetDefinition_HurtboxSet : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	//~ UAssetDefinition
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

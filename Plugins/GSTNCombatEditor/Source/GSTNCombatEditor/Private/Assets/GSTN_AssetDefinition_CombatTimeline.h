// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "GSTN_AssetDefinition_CombatTimeline.generated.h"

/**
 * How the Content Browser treats a UGSTN_CombatTimeline: its name, colour and category, and that opening it opens the timeline
 * editor (combat-m5-editor 5.1) rather than a plain data asset window. The engine finds it by reflection, nothing registers it.
 *
 * There is no factory: timelines are made as Data Assets of this class, as before. The definition only changes how they open.
 */
UCLASS()
class UGSTN_AssetDefinition_CombatTimeline : public UAssetDefinitionDefault
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

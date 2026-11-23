// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Assets/GSTN_AssetDefinition_HurtboxSet.h"

#include "Hits/GSTN_HurtboxTypes.h"
#include "Hurtbox/GSTN_HurtboxSetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FText UGSTN_AssetDefinition_HurtboxSet::GetAssetDisplayName() const
{
	return LOCTEXT("HurtboxSetAssetName", "Hurtbox Set");
}

FLinearColor UGSTN_AssetDefinition_HurtboxSet::GetAssetColor() const
{
	// The teal of the HurtboxModifier track: hurtboxes read as one family
	return FLinearColor(0.15f, 0.7f, 0.65f);
}

TSoftClassPtr<UObject> UGSTN_AssetDefinition_HurtboxSet::GetAssetClass() const
{
	return UGSTN_HurtboxSetDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UGSTN_AssetDefinition_HurtboxSet::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(EAssetCategoryPaths::Gameplay, LOCTEXT("CombatCategory", "Combat"), ECategoryMenuType::Section) };
	return Categories;
}

EAssetCommandResult UGSTN_AssetDefinition_HurtboxSet::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UGSTN_HurtboxSetDefinition* Set : OpenArgs.LoadObjects<UGSTN_HurtboxSetDefinition>())
	{
		const TSharedRef<FGSTN_HurtboxSetEditorToolkit> Toolkit = MakeShared<FGSTN_HurtboxSetEditorToolkit>();
		Toolkit->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Set);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE

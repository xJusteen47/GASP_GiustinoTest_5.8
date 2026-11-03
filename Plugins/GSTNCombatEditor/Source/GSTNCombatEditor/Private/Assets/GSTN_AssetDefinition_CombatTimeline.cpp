// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Assets/GSTN_AssetDefinition_CombatTimeline.h"

#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorToolkit.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FText UGSTN_AssetDefinition_CombatTimeline::GetAssetDisplayName() const
{
	return LOCTEXT("CombatTimelineAssetName", "Combat Timeline");
}

FLinearColor UGSTN_AssetDefinition_CombatTimeline::GetAssetColor() const
{
	// The orange of the Event track: frame data reads as one family
	return FLinearColor(0.9f, 0.4f, 0.15f);
}

TSoftClassPtr<UObject> UGSTN_AssetDefinition_CombatTimeline::GetAssetClass() const
{
	return UGSTN_CombatTimeline::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UGSTN_AssetDefinition_CombatTimeline::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(EAssetCategoryPaths::Gameplay, LOCTEXT("CombatCategory", "Combat"), ECategoryMenuType::Section) };
	return Categories;
}

EAssetCommandResult UGSTN_AssetDefinition_CombatTimeline::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UGSTN_CombatTimeline* Timeline : OpenArgs.LoadObjects<UGSTN_CombatTimeline>())
	{
		// A timeline that already has an editor is brought to the front by InitEditor and no second one opens
		const TSharedRef<FGSTN_TimelineEditorToolkit> Toolkit = MakeShared<FGSTN_TimelineEditorToolkit>();
		Toolkit->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, FGSTN_TimelineEditContext::ForAsset(Timeline));
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE

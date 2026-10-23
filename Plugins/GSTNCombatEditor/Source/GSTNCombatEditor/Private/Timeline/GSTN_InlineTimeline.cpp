// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_InlineTimeline.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineEditorToolkit.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

bool GSTNInlineTimeline::OpenEditor(const FGSTN_TimelineEditContext& Context)
{
	if (!Context.IsValid())
	{
		return false;
	}
	// A target that already has an editor is brought to the front by InitEditor and no second one opens
	const TSharedRef<FGSTN_TimelineEditorToolkit> Toolkit = MakeShared<FGSTN_TimelineEditorToolkit>();
	return Toolkit->InitEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Context);
}

GSTNInlineTimeline::EAfterCompile GSTNInlineTimeline::DecideAfterCompile(const FGSTN_TimelineEditContext& Context)
{
	return Context.IsValid() ? EAfterCompile::Refresh : EAfterCompile::Close;
}

bool GSTNInlineTimeline::ExtractToAsset(const FGSTN_TimelineEditContext& Context, UGSTN_CombatTimeline* NewAsset)
{
	UGSTN_GA_BaseCombat* Ability = Context.IsInline() ? Cast<UGSTN_GA_BaseCombat>(Context.GetOwner()) : nullptr;
	if (Ability == nullptr || NewAsset == nullptr || NewAsset == Context.GetOwner())
	{
		return false;
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("ExtractInlineTimeline", "Extract inline timeline to asset"));
		Context.Modify();
		NewAsset->Modify();

		NewAsset->Data = Ability->Timeline;
		Ability->TimelineAsset = NewAsset;
		Ability->Timeline = FGSTN_CombatTimelineData();

		// Both properties changed: the panel and the Blueprint see an event for each. NotifyChanged marks the Blueprint modified
		FProperty* AssetProperty = UGSTN_GA_BaseCombat::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UGSTN_GA_BaseCombat, TimelineAsset));
		FPropertyChangedEvent AssetEvent(AssetProperty, EPropertyChangeType::ValueSet);
		Ability->PostEditChangeProperty(AssetEvent);
		NewAsset->PostEditChange();
		Context.NotifyChanged();
	}
	return true;
}

void GSTNInlineTimeline::GetExtractDefaults(const FGSTN_TimelineEditContext& Context, FString& OutPackagePath, FString& OutAssetName)
{
	OutPackagePath.Reset();
	OutAssetName = TEXT("Timeline");
	if (const UBlueprint* Blueprint = Context.GetBlueprint())
	{
		OutPackagePath = FPackageName::GetLongPackagePath(Blueprint->GetOutermost()->GetName());
		OutAssetName = Blueprint->GetName() + TEXT("_Timeline");
	}
}

#undef LOCTEXT_NAMESPACE

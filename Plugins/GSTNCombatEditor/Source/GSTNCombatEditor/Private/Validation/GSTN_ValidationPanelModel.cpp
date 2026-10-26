// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_ValidationPanelModel.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_InlineTimeline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineEditorToolkit.h"
#include "Timeline/GSTN_TimelineGesture.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void FGSTN_ValidationPanelModel::SetResult(GSTNValidateAll::FResult InResult)
{
	Result = MoveTemp(InResult);
	bHasResult = true;
	Rebuild();
}

bool FGSTN_ValidationPanelModel::ReplaceAssetResult(GSTNValidateAll::FAssetResult InAssetResult)
{
	for (GSTNValidateAll::FAssetResult& Existing : Result.Assets)
	{
		if (Existing.AssetData.GetSoftObjectPath() == InAssetResult.AssetData.GetSoftObjectPath())
		{
			Existing = MoveTemp(InAssetResult);
			Rebuild();
			return true;
		}
	}
	return false;
}

void FGSTN_ValidationPanelModel::SetFilter(const FFilter& InFilter)
{
	Filter = InFilter;
	Rebuild();
}

bool FGSTN_ValidationPanelModel::PassesFilter(const FGSTN_ValidationFinding& Finding, const FString& AssetName, const FFilter& Filter)
{
	switch (Finding.Severity)
	{
	case EGSTN_Severity::Error:
		if (!Filter.bErrors) { return false; }
		break;
	case EGSTN_Severity::Warning:
		if (!Filter.bWarnings) { return false; }
		break;
	default:
		if (!Filter.bInfos) { return false; }
		break;
	}
	if (Filter.Text.IsEmpty())
	{
		return true;
	}
	return Finding.Code.ToString().Contains(Filter.Text) || Finding.Message.ToString().Contains(Filter.Text) || AssetName.Contains(Filter.Text);
}

void FGSTN_ValidationPanelModel::Rebuild()
{
	Roots.Reset();
	for (int32 AssetIndex = 0; AssetIndex < Result.Assets.Num(); ++AssetIndex)
	{
		const GSTNValidateAll::FAssetResult& Asset = Result.Assets[AssetIndex];
		const FString AssetName = Asset.AssetData.AssetName.ToString();
		TSharedPtr<FNode> AssetNode;
		for (int32 FindingIndex = 0; FindingIndex < Asset.Report.Findings.Num(); ++FindingIndex)
		{
			if (!PassesFilter(Asset.Report.Findings[FindingIndex], AssetName, Filter))
			{
				continue;
			}
			if (!AssetNode.IsValid())
			{
				AssetNode = MakeShared<FNode>();
				AssetNode->AssetIndex = AssetIndex;
			}
			const TSharedRef<FNode> FindingNode = MakeShared<FNode>();
			FindingNode->AssetIndex = AssetIndex;
			FindingNode->FindingIndex = FindingIndex;
			AssetNode->Children.Add(FindingNode);
		}
		if (AssetNode.IsValid())
		{
			Roots.Add(AssetNode);
		}
	}

	// The assets that need work first; stable inside a group so the order does not jump between two runs
	Roots.StableSort([this](const TSharedPtr<FNode>& A, const TSharedPtr<FNode>& B)
	{
		const EGSTN_Severity SeverityA = GetWorstSeverity(*A);
		const EGSTN_Severity SeverityB = GetWorstSeverity(*B);
		if (SeverityA != SeverityB)
		{
			return static_cast<uint8>(SeverityA) < static_cast<uint8>(SeverityB);
		}
		return Result.Assets[A->AssetIndex].AssetData.AssetName.LexicalLess(Result.Assets[B->AssetIndex].AssetData.AssetName);
	});
	OnChanged.Broadcast();
}

const FGSTN_ValidationFinding* FGSTN_ValidationPanelModel::GetFinding(const FNode& Node) const
{
	return Node.IsAsset() ? nullptr : &Result.Assets[Node.AssetIndex].Report.Findings[Node.FindingIndex];
}

EGSTN_Severity FGSTN_ValidationPanelModel::GetWorstSeverity(const FNode& AssetNode) const
{
	// Error < Warning < Info in the enum, so the smallest value is the worst
	EGSTN_Severity Worst = EGSTN_Severity::Info;
	for (const TSharedPtr<FNode>& Child : AssetNode.Children)
	{
		if (const FGSTN_ValidationFinding* Finding = GetFinding(*Child))
		{
			if (static_cast<uint8>(Finding->Severity) < static_cast<uint8>(Worst))
			{
				Worst = Finding->Severity;
			}
		}
	}
	return Worst;
}

FText FGSTN_ValidationPanelModel::GetSummary() const
{
	if (!bHasResult)
	{
		return LOCTEXT("PanelNotRun", "Press Validate all to check every combat asset of the project.");
	}
	return FText::Format(LOCTEXT("PanelSummary", "{0} assets checked: {1} errors, {2} warnings, {3} notes."),
		FText::AsNumber(Result.GetCheckedCount()), FText::AsNumber(Result.CountSeverity(EGSTN_Severity::Error)),
		FText::AsNumber(Result.CountSeverity(EGSTN_Severity::Warning)), FText::AsNumber(Result.CountSeverity(EGSTN_Severity::Info)));
}

bool GSTNValidationNavigation::OpenFinding(UObject* Asset, const FGSTN_FindingLocation& Where)
{
	if (Asset == nullptr)
	{
		return false;
	}

	// A timeline asset, or the inline timeline of an ability Blueprint: the timeline editor, then the place
	FGSTN_TimelineEditContext Context;
	if (UGSTN_CombatTimeline* Timeline = Cast<UGSTN_CombatTimeline>(Asset))
	{
		Context = FGSTN_TimelineEditContext::ForAsset(Timeline);
	}
	else if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		Context = FGSTN_TimelineEditContext::ForBlueprint(Blueprint);
	}

	if (Context.IsValid() && Where.IsSet())
	{
		GSTNInlineTimeline::OpenEditor(Context);
		if (const TSharedPtr<FGSTN_TimelineEditorToolkit> Toolkit = FGSTN_TimelineEditorToolkit::FindOpenFor(Context.GetTarget()))
		{
			if (Toolkit->GetModel().IsValid())
			{
				FGSTN_TimelineActions::GoToFinding(*Toolkit->GetModel(), Where);
				return true;
			}
		}
		return false;
	}

	if (GEditor != nullptr)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

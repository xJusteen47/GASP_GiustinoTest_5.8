// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Validation/GSTN_ValidateAll.h"

/**
 * What the validation panel shows, without a widget: the result of the last Validate all, the filter, and the tree built from them (assets
 * that have a finding to show, each with its findings). The widget is a view of this and the tests read it. Navigation to a finding is the
 * separate GSTNValidationNavigation, because it needs the editor.
 */
class FGSTN_ValidationPanelModel
{
public:
	/** A row of the tree: an asset (Children are its findings) or one finding of it */
	struct FNode
	{
		int32 AssetIndex = INDEX_NONE;

		/** INDEX_NONE for an asset row */
		int32 FindingIndex = INDEX_NONE;

		TArray<TSharedPtr<FNode>> Children;

		bool IsAsset() const { return FindingIndex == INDEX_NONE; }
	};

	struct FFilter
	{
		bool bErrors = true;
		bool bWarnings = true;
		bool bInfos = true;

		/** Case-insensitive; matches the code, the message or the asset name. Empty = everything */
		FString Text;
	};

	void SetResult(GSTNValidateAll::FResult InResult);
	void Clear() { SetResult(GSTNValidateAll::FResult()); }

	/** Replaces the result of one asset (re-checked after an edit) and rebuilds the tree. False when the asset is not in the result */
	bool ReplaceAssetResult(GSTNValidateAll::FAssetResult InAssetResult);

	const GSTNValidateAll::FResult& GetResult() const { return Result; }
	bool HasResult() const { return bHasResult; }

	void SetFilter(const FFilter& InFilter);
	const FFilter& GetFilter() const { return Filter; }

	static bool PassesFilter(const FGSTN_ValidationFinding& Finding, const FString& AssetName, const FFilter& Filter);

	/** Asset rows that have at least one visible finding: those with errors first, then by name */
	const TArray<TSharedPtr<FNode>>& GetRoots() const { return Roots; }

	const GSTNValidateAll::FAssetResult& GetAsset(const FNode& Node) const { return Result.Assets[Node.AssetIndex]; }

	/** The finding of a finding row, null for an asset row */
	const FGSTN_ValidationFinding* GetFinding(const FNode& Node) const;

	/** The most severe finding of an asset row's visible children */
	EGSTN_Severity GetWorstSeverity(const FNode& AssetNode) const;

	/** "14 assets checked: 2 errors, 3 warnings, 1 note", or what to do when nothing has run */
	FText GetSummary() const;

	FSimpleMulticastDelegate OnChanged;

private:
	void Rebuild();

	GSTNValidateAll::FResult Result;
	FFilter Filter;
	TArray<TSharedPtr<FNode>> Roots;
	bool bHasResult = false;
};

/** Going from a finding to the place that fixes it (combat-m5-editor 9.1) */
namespace GSTNValidationNavigation
{
	/**
	 * Opens the editor of the asset a finding belongs to. For a timeline finding that has a location (a timeline asset, or the inline timeline of an
	 * ability Blueprint) the timeline editor opens, selects the track (and key) and puts the playhead on the frame; other assets open in their
	 * own editor. Returns true when it selected a place in a timeline editor.
	 */
	bool OpenFinding(UObject* Asset, const struct FGSTN_FindingLocation& Where);
}

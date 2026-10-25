// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Validation/GSTN_ValidationReport.h"

/**
 * Validate all (combat-m5-editor 9.1): the validators over every combat asset the Asset Registry knows. The asset kinds are the ones the
 * Data Validation validators take (timelines, ability Blueprints, hurtbox sets, kits, weapons, anything holding a loadout), and each is checked
 * with the code those validators run (UGSTN_CombatValidatorBase::BuildReportForAsset), so this shows what Data Validation would, without
 * Data Validation's own context. No Slate in this header: the panel, the console command and the tests all read the same result.
 */
namespace GSTNValidateAll
{
	/** One asset and what was found on it. A clean asset is listed too, so the panel can say how many were checked */
	struct FAssetResult
	{
		FAssetData AssetData;

		/** Findings of the asset. Their Asset pointers are weak: the asset may be unloaded again by the time the panel reads them */
		FGSTN_ValidationReport Report;

		/** False when the asset could not be loaded or no combat validator applies */
		bool bValidated = false;
	};

	struct FResult
	{
		TArray<FAssetResult> Assets;

		int32 GetCheckedCount() const;
		int32 CountSeverity(EGSTN_Severity Severity) const;
		bool HasErrors() const { return CountSeverity(EGSTN_Severity::Error) > 0; }

		/** Every finding in one report, in asset order, for the Message Log */
		FGSTN_ValidationReport Flatten() const;
	};

	struct FOptions
	{
		/**
		 * Only assets whose package path starts with this (e.g. /Game/Combat). Empty = all project and plugin content: everything
		 * except /Engine and /Temp (unsaved in-memory assets)
		 */
		FString PathPrefix;

		/** A slow task with a cancel button while assets load (the panel and the console command set it; tests do not) */
		bool bShowProgress = false;
	};

	/** The assets the validators take, from the registry (the ability Blueprints are matched by the class they generate; nothing is loaded) */
	void CollectAssets(const FOptions& Options, TArray<FAssetData>& OutAssets);

	/** Loads each asset (an unloaded inline Blueprint is loaded here) and validates it. Returns what was found */
	FResult Run(const FOptions& Options);

	/** Runs on one asset the user has just changed, for the panel's re-check of a row */
	FAssetResult RunOne(const FAssetData& AssetData);
}

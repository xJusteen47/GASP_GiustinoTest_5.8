// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Validation/GSTN_ValidationReport.h"

class UClass;
class UObject;
struct FGSTN_CombatTimelineData;

/**
 * Rules another plugin adds to the combat validation (M11 L8: the kit mods editor is the first client). Without this seam an optional
 * plugin could only write its own Data Validation validator, and its findings would never reach the M5 validation panel or Validate all.
 *
 * An extension says which assets it applies to and fills the same FGSTN_ValidationReport the combat rules fill, with its own codes. It
 * runs in three places, always through RunRules: BuildReportForAsset (the panel, Validate all, the Validate button), after the combat
 * rules; one Data Validation validator of this module (UGSTN_ExtensionAssetValidator) for save and Validate Assets; and the tests.
 * AssetClasses brings extra asset kinds into Validate all (the mod definition); an extension may also apply to kinds the combat rules
 * already take (a kit with mod slots) without listing them.
 *
 * Game thread, editor only. Register in the client module's StartupModule, unregister in ShutdownModule.
 */
struct GSTNCOMBATEDITOR_API FGSTN_ValidationExtension
{
	/** Stable id, e.g. GSTN.KitMods. Registering the same name again replaces the entry (Live Coding, module reload) */
	FName Name;

	/** Asset classes Validate all collects for this extension (subclasses included). Leave empty when the extension only adds rules to combat assets */
	TArray<TWeakObjectPtr<UClass>> AssetClasses;

	/** Whether the rule has anything to say about this asset. Cheap: it is asked of every asset Data Validation sees */
	TFunction<bool(const UObject& Asset)> AppliesTo;

	/** Adds the findings. Only called when AppliesTo said yes */
	TFunction<void(UObject& Asset, FGSTN_ValidationReport& Report)> Rule;
};

struct GSTNCOMBATEDITOR_API FGSTN_ValidationExtensions
{
	static void Register(FGSTN_ValidationExtension Extension);
	static void Unregister(FName Name);
	static bool IsRegistered(FName Name);

	/** True when a registered extension applies to the asset */
	static bool AppliesToAny(const UObject* Asset);

	/** Runs every extension that applies; true when at least one did. Findings without an asset get this one */
	static bool RunRules(UObject& Asset, FGSTN_ValidationReport& Report);

	/** The classes of every extension's AssetClasses, without duplicates */
	static void GetAssetClasses(TArray<UClass*>& OutClasses);

	/**
	 * The M5 timeline rules (the timeline asset validator's, same codes) on frame data that is not an asset of its own, such as a timeline
	 * a kit mod derives. Asset is what the findings name. The montage is loaded for the length checks only when bLoadMontage
	 */
	static void ValidateTimelineData(const FGSTN_CombatTimelineData& Data, UObject* Asset, FGSTN_ValidationReport& Report, bool bLoadMontage = true);
};

// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_PartsPresentationBudget.h"
#include "GSTN_PartsSettings.generated.h"

class UGSTN_PenetrationTable;

/**
 * Settings of the parts plugin (Project Settings > Plugins > GSTN Parts): the defaults an entity falls back on when its part
 * set says nothing. Defaults ship in the plugin's Config/DefaultGSTNParts.ini; a project overrides them in its own
 * Config/DefaultGSTNParts.ini.
 *
 * The simulation of every machine reads the weight values, so they must be equal on all machines of a match and never change
 * during one, like any other setting the simulation reads. The presentation budget is read by the presentation module only and
 * may differ per machine.
 */
UCLASS(config = GSTNParts, defaultconfig, meta = (DisplayName = "GSTN Parts"))
class GSTNPARTSCORE_API UGSTN_PartsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_PartsSettings();

	/** The settings of the running project */
	static const UGSTN_PartsSettings& Get() { return *GetDefault<UGSTN_PartsSettings>(); }

	/**
	 * Penetration table for part sets and rules that name none. Empty = the built-in three bands (above full, equal 65 %,
	 * below ricochets). Soft, so the plugin's content (when it ships some) stays replaceable
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Penetration")
	TSoftObjectPtr<UGSTN_PenetrationTable> DefaultPenetrationTable;

	/**
	 * Shape of the default weight curve of a part set that has none of its own: the weight scale falls linearly from 1 at no
	 * damage to this value at the cap, so a battered enemy flies further (combat-parts.md §4.3)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Weight", meta = (ClampMin = 0, ClampMax = 1))
	float DefaultWeightScaleAtCap = 0.6f;

	/** Effective weight never goes below this fraction of the base weight, however much mass was lost (1 = weight never changes) */
	UPROPERTY(Config, EditAnywhere, Category = "Weight", meta = (ClampMin = 0, ClampMax = 1))
	float MinWeightScale = 0.35f;

	/** Ceilings of the cosmetic debris. Presentation only; scaled down by scalability level and distance */
	UPROPERTY(Config, EditAnywhere, Category = "Presentation")
	FGSTN_PartsPresentationBudget PresentationBudget;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_DefenceSettings.generated.h"

class UGSTN_DefenceRules;

/**
 * Settings of the defence plugin (Project Settings > Plugins > GSTN Defence). Defaults ship in the plugin's
 * Config/DefaultGSTNDefence.ini; a project overrides them in its own Config/DefaultGSTNDefence.ini.
 *
 * The simulation of every machine reads the rules asset named here, so it must be the same on all machines of a match and must
 * never change during one, like any other setting the simulation reads.
 */
UCLASS(config = GSTNDefence, defaultconfig, meta = (DisplayName = "GSTN Defence"))
class GSTNDEFENCECORE_API UGSTN_DefenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The settings of the running project */
	static const UGSTN_DefenceSettings& Get() { return *GetDefault<UGSTN_DefenceSettings>(); }

	/**
	 * Rules for a mode whose combat rules carry no defence extension. Empty = no defence rules: no shield exists. Soft, so the
	 * plugin ships no content and the asset stays replaceable
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	TSoftObjectPtr<UGSTN_DefenceRules> DefaultRules;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

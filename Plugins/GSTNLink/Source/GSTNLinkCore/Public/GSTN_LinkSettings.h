// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_LinkSettings.generated.h"

/**
 * Settings of the link plugin (Project Settings > Plugins > GSTN Link). Defaults ship in the plugin's Config/DefaultGSTNLink.ini;
 * a project overrides them in its own Config/DefaultGSTNLink.ini.
 *
 * The simulation of every machine reads these values, so they must be the same on all machines of a match and must never change
 * during one, like any other setting the simulation reads. Link types are not listed here: the asset manager finds them by their
 * primary asset type (GSTNLinkAssetTypes::LinkType).
 */
UCLASS(config = GSTNLink, defaultconfig, meta = (DisplayName = "GSTN Link"))
class GSTNLINKCORE_API UGSTN_LinkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The settings of the running project */
	static const UGSTN_LinkSettings& Get() { return *GetDefault<UGSTN_LinkSettings>(); }

	/** Frames a predicted link waits for its response before the link tick clears it */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Formation", meta = (ClampMin = "1", ClampMax = "120", ForceUnits = "Frames"))
	int32 RequestTimeoutFrames = 12;

	/** Frames after a formation during which a one-sided link is not yet an orphan (the other side's state is still in flight) */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Watchdog", meta = (ClampMin = "0", ClampMax = "60", ForceUnits = "Frames"))
	int32 OrphanGraceFrames = 3;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

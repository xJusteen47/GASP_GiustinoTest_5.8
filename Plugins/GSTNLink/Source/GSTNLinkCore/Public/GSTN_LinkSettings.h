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
 * primary asset type (GSTNLinkAssetTypes::LinkType), which the module registers itself (see LinkTypeDirectories).
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

	/**
	 * Content folders, besides the plugin's own content, where link type definitions live (a project's test or game content, another
	 * plugin's content). The module registers the GSTN_LinkType primary asset type with the asset manager over the plugin's content
	 * and these folders at startup, so the project's DefaultGame.ini does not list the type: a PrimaryAssetTypesToScan line naming a
	 * class of this plugin trips an engine ensure in every build where the plugin is disabled. Read once, when the asset manager is
	 * created; a change needs an editor restart
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Assets", meta = (ContentDir, LongPackageName))
	TArray<FDirectoryPath> LinkTypeDirectories;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

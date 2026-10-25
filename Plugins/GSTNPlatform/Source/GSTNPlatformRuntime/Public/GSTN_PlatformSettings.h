// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "GSTN_PlatformSettings.generated.h"

/**
 * Settings of the platform plugin (Project Settings > Plugins > GSTN Platform). Defaults ship in the plugin's
 * Config/DefaultGSTNPlatform.ini. The colliders of every machine are built from them, so they must be equal on all machines of
 * a match.
 */
UCLASS(config = GSTNPlatform, defaultconfig, meta = (DisplayName = "GSTN Platform"))
class GSTNPLATFORMRUNTIME_API UGSTN_PlatformSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGSTN_PlatformSettings& Get() { return *GetDefault<UGSTN_PlatformSettings>(); }

	/**
	 * Name of the object channel of the platform colliders (many-enemies-replication.md 4.3). The plugin's Config/DefaultEngine.ini
	 * declares it; it is found by name so a project can move it to another slot. Without it the colliders use WorldDynamic and a
	 * warning is logged once
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Collision")
	FName PlatformObjectChannelName = TEXT("GSTN_EnemyPlatform");

	/**
	 * When no simulation step has run the pose point for this long (real seconds: no pawn simulates in the world, the game is
	 * paused), a platform poses its colliders from the game tick so they still follow the enemy. Nothing simulates against that pose
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Pose", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PresentationFallbackSeconds = 0.25f;

	/** The object channel of the platform colliders: the channel named PlatformObjectChannelName, else WorldDynamic */
	ECollisionChannel GetPlatformObjectChannel() const;

	/** True if PlatformObjectChannelName names a channel of the project's collision settings */
	bool HasPlatformObjectChannel() const;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

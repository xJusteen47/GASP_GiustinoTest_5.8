// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "GSTN_PlatformCollision.h"
#include "GSTN_PlatformSpec.h"
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
	UGSTN_PlatformSettings();

	static const UGSTN_PlatformSettings& Get() { return *GetDefault<UGSTN_PlatformSettings>(); }

	/**
	 * Name of the object channel of the platform colliders (many-enemies-replication.md 4.3). The plugin's Config/Engine.ini
	 * declares it; it is found by name so a project can move it to another slot. Without it the colliders use WorldDynamic and a
	 * warning is logged once
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Collision")
	FName PlatformObjectChannelName = TEXT("GSTN_EnemyPlatform");

	/** Channels the characters' movement sweeps on (the Mover capsule's object type). Default: Pawn */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Collision")
	TArray<TEnumAsByte<ECollisionChannel>> MoverChannels;

	/** Channels the wall modes probe on. Default: Pawn (this project's wall modes probe with the capsule's object type) */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Collision")
	TArray<TEnumAsByte<ECollisionChannel>> WallProbeChannels;

	/** Extra channels Stand and Launch shapes block, such as the depth marker's trace channel (depth-marker.md 2.8). Default: none */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Collision")
	TArray<TEnumAsByte<ECollisionChannel>> StandAndLaunchExtraChannels;

	/**
	 * Replace the shipped behaviour-to-collision table (GSTNPlatformCollision::MakeDefaultRules over the three channel lists above)
	 * with CustomCollisionRules
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Collision")
	bool bUseCustomCollisionRules = false;

	/** The table used when bUseCustomCollisionRules is on */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Collision", meta = (EditCondition = "bUseCustomCollisionRules"))
	TArray<FGSTN_PlatformCollisionRule> CustomCollisionRules;

	/**
	 * When no simulation step has run the pose point for this long (real seconds: no pawn simulates in the world, the game is
	 * paused), a platform poses its colliders from the game tick so they still follow the enemy. Nothing simulates against that pose
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Pose", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PresentationFallbackSeconds = 0.25f;

	/** Validation only: the wall modes' accepted angle range between a face's normal and up (wall-slide-v2.md 6.8) */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Validation", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "Degrees"))
	float MinWallSurfaceAngle = 45.f;

	UPROPERTY(Config, EditAnywhere, Category = "Platform|Validation", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "Degrees"))
	float MaxWallSurfaceAngle = 120.f;

	/** Validation only: the steepest top a character can stand on */
	UPROPERTY(Config, EditAnywhere, Category = "Platform|Validation", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "Degrees"))
	float MaxStandAngle = 44.f;

	/** The object channel of the platform colliders: the channel named PlatformObjectChannelName, else WorldDynamic */
	ECollisionChannel GetPlatformObjectChannel() const;

	/** True if PlatformObjectChannelName names a channel of the project's collision settings */
	bool HasPlatformObjectChannel() const;

	/** The behaviour-to-collision table in use */
	TArray<FGSTN_PlatformCollisionRule> GetCollisionRules() const;

	/** Validation context from these settings (part names unresolved: the platform plugin alone has no part bridge) */
	FGSTN_PlatformValidationContext MakeValidationContext() const;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

// Depth marker project settings (depth-marker.md §5).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "GSTN_DepthMarkerSettings.generated.h"

class UGSTN_DepthMarkerStyle;
class UGSTN_DepthTargetSelector;
class UMaterialInterface;
class UStaticMesh;

/**
 * Project Settings > Plugins > GSTN Depth Marker. Defaults ship in the plugin Config/DefaultGSTNDepthMarker.ini; a project
 * overrides them in its own Config/DefaultGSTNDepthMarker.ini. Presentation only: every machine may use different values
 * (a low-end client lowers MaxMarkers) without desynchronising anything.
 */
UCLASS(config = GSTNDepthMarker, defaultconfig, meta = (DisplayName = "GSTN Depth Marker"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthMarkerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_DepthMarkerSettings();

	static const UGSTN_DepthMarkerSettings& Get() { return *GetDefault<UGSTN_DepthMarkerSettings>(); }

	/** Master switch of the subsystem (also console variable gstn.DepthMarker.Enable) */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	/** Collision channel of the probes. Platform shapes must Block it (§2.8). Proposal: a project channel DepthMarker */
	UPROPERTY(Config, EditAnywhere, Category = "Targets")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Object types that are never a target even if they block the channel (pawns, projectiles) */
	UPROPERTY(Config, EditAnywhere, Category = "Targets")
	TArray<TEnumAsByte<ECollisionChannel>> ExcludedObjectTypes;

	/** Surface behaviours that make a valid target (§2.8); empty = channel only */
	UPROPERTY(Config, EditAnywhere, Category = "Targets")
	FGameplayTagQuery TargetBehaviours;

	/** The look used when a component names none. Empty = the built-in look made of engine primitives */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	TSoftObjectPtr<UGSTN_DepthMarkerStyle> DefaultStyle;

	/** The selector used when a component brings none. Empty = the gravity selector */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	TSoftClassPtr<UGSTN_DepthTargetSelector> DefaultSelector;

	/** Mesh of one ring segment or line dash for the built-in presenters (a unit cube, scaled) */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Content")
	TSoftObjectPtr<UStaticMesh> DefaultSegmentMesh;

	/** Material of the built-in ring and line; it should expose a vector parameter named TintParameterName */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Content")
	TSoftObjectPtr<UMaterialInterface> DefaultSegmentMaterial;

	/** Material of the built-in ground decal */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Content")
	TSoftObjectPtr<UMaterialInterface> DefaultDecalMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Look|Content")
	FName TintParameterName = TEXT("Color");

	UPROPERTY(Config, EditAnywhere, Category = "Look|Content")
	FName OpacityParameterName = TEXT("Opacity");

	/** Markers drawn at once, ranked: local player, allies, then others by proximity (also gstn.DepthMarker.MaxMarkers) */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaxMarkers = 12;

	/** Traces issued per frame, oldest first (also gstn.DepthMarker.TracesPerFrame) */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaxTracesPerFrame = 8;

	/** Markers of others beyond this distance from every local pawn get no probe and no marker; the local pawn is never culled */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxViewDistanceCm = 4000.f;

	/** Ranks below this count probe every frame */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0"))
	int32 FullRateRanks = 4;

	/** Ranks below this count (and from FullRateRanks) probe at MidRateHz; the rest at LowRateHz */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0"))
	int32 MidRateRanks = 8;

	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "1.0", ForceUnits = "Hz"))
	float MidRateHz = 30.f;

	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "1.0", ForceUnits = "Hz"))
	float LowRateHz = 10.f;

	/** Fade and visibility thresholds; a style carries its own copy */
	UPROPERTY(Config, EditAnywhere, Category = "Visibility")
	FGSTN_DepthFadeParams Fade;

	/** Tolerances of the selection rules (§2); the default selector copies them */
	UPROPERTY(Config, EditAnywhere, Category = "Selection")
	FGSTN_DepthSelectorParams DefaultParams;
};

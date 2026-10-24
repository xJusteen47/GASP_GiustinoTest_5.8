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

/** Which look a machine uses by default (the console variable gstn.DepthMarker.Style toy|classic overrides it) */
UENUM(BlueprintType)
enum class EGSTN_DepthLook : uint8
{
	/** The current look: a component's style, DefaultStyle, then the built-in primitives */
	Classic,
	/** The toy look (ToyStyle): beam, fog puff, stained-glass decal, per player colours */
	Toy
};

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

	/** Default look when no console variable asks for one. Classic until the toy look has been checked */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Toy")
	EGSTN_DepthLook DefaultLook = EGSTN_DepthLook::Classic;

	/** The toy style asset (a project names its own /Game path in its Config; the plugin ships no content) */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Toy")
	TSoftObjectPtr<UGSTN_DepthMarkerStyle> ToyStyle;

	/** Mesh of the beam: a cone with its tip at local +Z (the bounds are read, any pivot works) */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Toy")
	TSoftObjectPtr<UStaticMesh> ToyBeamMesh;

	/** Mesh of the fog puff: a quad facing local +Z */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Toy")
	TSoftObjectPtr<UStaticMesh> ToyPuffMesh;

	/** First custom primitive data float the toy materials read (13 floats: colour, opacity, beam start, beam end) */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Toy", meta = (ClampMin = "0"))
	int32 ToyCustomPrimitiveDataBase = 0;

	/** Colour of the player by colour rank (alphabetical order of the players' names): red, blue, yellow, green. Extend freely; ranks wrap */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Toy")
	TArray<FLinearColor> PlayerPalette = { FLinearColor(0.9f, 0.1f, 0.08f), FLinearColor(0.12f, 0.38f, 1.f),
		FLinearColor(1.f, 0.82f, 0.05f), FLinearColor(0.15f, 0.8f, 0.22f) };

	/** Colour by team id for pawns that have no player (AI, allies without a player state) */
	UPROPERTY(Config, EditAnywhere, Category = "Look|Toy")
	TMap<int32, FLinearColor> TeamPalette;

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

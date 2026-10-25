// Pure part logic (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"

#include "GSTN_PartsSettings.generated.h"

/**
 * Ceilings of the part presentation (combat-parts 6.4), per machine that renders, the listen-server host included. They are
 * host-tier values: scalability and the measured frame time scale them down, never up. Presentation only: a skipped chunk or
 * shard never changes gameplay, so these values are not simulation input and may differ between machines.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartsPresentationBudget
{
	GENERATED_BODY()

	/** Physical chunks alive at once (limbs and large shards), all entities. The oldest is recycled when the pool is exhausted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Presentation", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxChunks = 48;

	/** Seconds a chunk lives before it shrinks and fades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Presentation", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float ChunkLifetimeSeconds = 4.f;

	/** Shatter shard particles alive at once (Niagara mesh particles, CPU simulation), all entities. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Presentation", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxShardParticles = 600;

	/** Geometry Collections alive at once (players and bosses only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Presentation", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxGeometryCollections = 2;

	/** Seconds after which a resting chunk stops simulating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SleepStopSeconds = 1.5f;
};

/**
 * Settings of the parts plugin (Project Settings > Plugins > GSTN Parts). Defaults live in the plugin's own
 * Config/DefaultGSTNParts.ini; a project overrides them in its own Config/DefaultGSTNParts.ini. Nothing about the parts plugin
 * lives in the project's DefaultGame.ini or DefaultEngine.ini.
 *
 * The two content references are soft: a part set that names no table of its own uses the default one, and the simulation reads
 * them before the pawn's first Network Prediction tick, so they must resolve to the same asset on the host and on every client
 * (the same rule as any other setting the simulation reads). Empty means the plugin ships no default yet.
 */
UCLASS(config = GSTNParts, defaultconfig, meta = (DisplayName = "GSTN Parts"))
class GSTNPARTSCORE_API UGSTN_PartsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_PartsSettings();

	/** The settings of the running project */
	static const UGSTN_PartsSettings& Get() { return *GetDefault<UGSTN_PartsSettings>(); }

	/** The penetration table (a UGSTN_PenetrationTable) a part set uses when it names none. Typed once the table exists. */
	UPROPERTY(Config, EditAnywhere, Category = "Parts|Simulation")
	FSoftObjectPath DefaultPenetrationTable;

	/** The curve of the weight that falls with remaining health, baked into the integer table the simulation reads. */
	UPROPERTY(Config, EditAnywhere, Category = "Parts|Simulation", meta = (AllowedClasses = "/Script/Engine.CurveFloat"))
	FSoftObjectPath DefaultWeightCurve;

	/** Presentation ceilings, see FGSTN_PartsPresentationBudget. */
	UPROPERTY(Config, EditAnywhere, Category = "Parts|Presentation")
	FGSTN_PartsPresentationBudget PresentationBudget;
};

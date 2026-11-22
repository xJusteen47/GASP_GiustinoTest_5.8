// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_StickThresholds.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "GSTN_LoadoutInputFeel.generated.h"

/**
 * The player's input feel that the simulation reads (owner 2026-11-16, ambit: input feel in PvP: "the player must be able to tune it"):
 * the attack stick zones and the flick window. The input simulation classifies the stick on every machine, so these values must be the
 * same everywhere: they travel in the loadout (sent to the server, replicated, part of the revision) instead of a local option. The local
 * side (the move-stick dead zone, which never enters the simulation) stays in UGSTN_InputOptionsSubsystem, which also keeps the player's
 * choice of these values and writes them here.
 *
 * Belongs to the player, not to a kit: it stays with them on a kit swap, and the server copies it from the applied loadout into a request
 * that lacks it (a loadout screen or kit picker does not need to know about it). No entry = the defaults of FGSTN_StickThresholds.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutInputFeelExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	static const FName WireId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Feel")
	FGSTN_StickThresholds StickThresholds;

	virtual bool StaysWithPlayerOnKitSwap() const override { return true; }

	/** The thresholds a loadout asks for: its entry's, sanitized, else the defaults */
	static FGSTN_StickThresholds Resolve(const TArray<FInstancedStruct>& Extensions);
};

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_ExperienceGateLibrary.generated.h"

/**
 * Stateless entry points to the Experience gate (spike G4), for code that has a world but no handle on the GameState component:
 * the game mode (may I spawn pawns?) and the Network Prediction adapters (may this pawn tick?).
 *
 * A world without an Experience manager, or with no Experience assigned, is not gated, so adopting the plugin changes nothing for a
 * classic GameMode until a mode assigns an Experience.
 */
UCLASS()
class GSTNEXPERIENCE_API UGSTN_ExperienceGateLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True when simulation may run: no Experience assigned, or the assigned one is Loaded. */
	UFUNCTION(BlueprintPure, Category = "GSTN|Experience", meta = (WorldContext = "WorldContextObject"))
	static bool IsSimulationAllowed(const UObject* WorldContextObject);

	/**
	 * For the game mode's PlayerCanRestart / pawn spawn path (Lyra's rule: the game mode does not spawn pawns until the Experience
	 * is ready). With bRequireExperience the answer is false until an Experience is assigned and Loaded, which is what a mode that
	 * always uses Experiences wants; without it the answer is IsSimulationAllowed.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Experience", meta = (WorldContext = "WorldContextObject"))
	static bool CanSpawnPawns(const UObject* WorldContextObject, bool bRequireExperience = true);

	/** A gated simulation asks before every tick; false = do not tick now. Counts the refusal. See UGSTN_ExperienceManagerComponent. */
	static bool TryBeginSimTick(const UObject* WorldContextObject);

	/** A gated simulation reports a tick it ran, so a tick before Loaded shows up as a leak. */
	static void NoteSimTickRan(const UObject* WorldContextObject);
};

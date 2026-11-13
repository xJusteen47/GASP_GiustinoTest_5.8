// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PartsKitLibrary.generated.h"

class AActor;
class UGSTN_PartSetDefinition;
class UGSTN_PrimaryKitDefinition;

/**
 * Editor-time helper that gives a kit a part set (setup_gas_assets.py, an editor tool): the FGSTN_PartsKitExtension entry of the kit's
 * extension array is awkward to edit from Python. Idempotent; not used by the simulation.
 */
UCLASS()
class GSTNPARTSNP_API UGSTN_PartsKitLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Sets (adds or replaces) the part set of the kit's FGSTN_PartsKitExtension; a null Set removes the entry. False for a null kit */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Content")
	static bool SetKitPartSet(UGSTN_PrimaryKitDefinition* Kit, UGSTN_PartSetDefinition* Set);

	/**
	 * Attaches a part set to an actor on this machine by hand, the way the attach component does from a kit (a harness or an editor tool that has no
	 * kit to carry it). Call it on every world that has the pawn. A null set detaches. Returns whether a set is attached afterwards
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Debug")
	static bool AttachPartSetToActor(AActor* Actor, UGSTN_PartSetDefinition* Set);

	/** The parts state of the actor's combat sync state as text ("mask=0x0008 serial=1 dmg=[0,0,0,255,0,0]"), "none" without a fragment, "no combat" without a combat component */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Debug")
	static FString GetPartsStateString(const AActor* Actor);

	/**
	 * Starts recording the part events (Combat.Event.Part.*) this world's event subsystem publishes: the harness counts "Broken exactly once" with it.
	 * Call it on every world; a second call on the same world clears the log. The listener lives until StopPartEventLog or the end of the world
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Debug")
	static void StartPartEventLog(AActor* WorldContext);

	/** The rows recorded since StartPartEventLog, "Type:Frame:PartIndex:BreakSerial:TargetName" separated by ';' (Type is the last tag segment: Broken, Restored...) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Debug")
	static FString GetPartEventLog(AActor* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Combat|Parts|Debug")
	static void StopPartEventLog(AActor* WorldContext);
};

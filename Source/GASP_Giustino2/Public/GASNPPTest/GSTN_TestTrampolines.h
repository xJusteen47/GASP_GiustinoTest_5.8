// GAS + NPP + Mover network test harness: trampolines for the trampoline scenario (M4 H11b).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_TestTrampolines.generated.h"

class AGSTN_Trampoline;

/**
 * Static helpers the Python scenarios call to place AGSTN_Trampoline actors at run time (no map edit). The trampolines are NOT replicated: static level
 * data is identical on every machine, so the scenario spawns the same one in every world of the process (the harness runs the server and the clients in
 * one) with the same arguments, exactly as a level would carry them.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_TrampolineTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Spawns a trampoline in WorldContext's world: Center, the box's local up along Up (the launch face; the box is turned the shortest way), Size in cm
	 * (depth, width, height), launching along its local direction LocalDirection at a fixed Speed. Returns its name, empty if it could not be spawned
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Trampolines", meta = (WorldContext = "WorldContext"))
	static FString SpawnTestTrampoline(UObject* WorldContext, FVector Center, FVector Up, FVector Size, float Speed, FVector LocalDirection, bool bKeepTangential, bool bResetAirJumps, bool bResetCoyote);

	/**
	 * Spawns a trampoline of Class (a Blueprint child: its defaults are the tuning, Size and Launch included) at Location with Yaw, tagged as a test
	 * trampoline. Not replicated: call it in every world of the process, like the level would carry it. Returns its name, empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Trampolines", meta = (WorldContext = "WorldContext"))
	static FString SpawnTestTrampolineOfClass(UObject* WorldContext, TSubclassOf<AGSTN_Trampoline> Class, FVector Location, float Yaw);

	/** Removes the test trampolines of WorldContext's world; returns how many */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Trampolines", meta = (WorldContext = "WorldContext"))
	static int32 ClearTestTrampolines(UObject* WorldContext);
};

// Spin and drill, ability-system half.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "GSTN_SpinDrillLibrary.generated.h"

class AActor;
class UGSTN_PrimaryKitDefinition;

/**
 * Content helpers of the spin and drill: the default kit bindings of the spin, and reads of an entity's drill state for tools and the network
 * harness (Tools/GASNPPTest/spin_drill_scenarios.py). Nothing here runs in the simulation; the reads look at the latest synced combat state
 * of the actor's world, like the other plugins' harness reads.
 */
UCLASS()
class GSTNSPINDRILLGAS_API UGSTN_SpinDrillLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The default spin bindings (plan D4), each with its slot inline (FGSTN_SpinSlots): per spin command an air spin binding that requires
	 * Mover.IsInAir and a ground spin binding that ignores it, so the context picks exactly one. bKeyboardShortcut adds the .Keys commands,
	 * bound to the same classes
	 */
	static TArray<FGSTN_AbilityBinding> MakeDefaultSpinBindings(bool bKeyboardShortcut = true);

	/**
	 * Appends the default spin bindings to a kit, skipping the ones it already has (same key). For setup scripts and the harness: the change is
	 * in memory until the asset is saved. Returns the number added
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|SpinDrill|Content")
	static int32 AddSpinBindingsToKit(UGSTN_PrimaryKitDefinition* Kit, bool bKeyboardShortcut = true);

	/** Removes every binding of a spin command from a kit (in memory). Returns the number removed */
	UFUNCTION(BlueprintCallable, Category = "GSTN|SpinDrill|Content")
	static int32 RemoveSpinBindingsFromKit(UGSTN_PrimaryKitDefinition* Kit);

	/** The drill fragment of the actor's latest combat state as text ("Drill Spinning@120 cw air 1 ..."), empty when it has none */
	UFUNCTION(BlueprintCallable, Category = "GSTN|SpinDrill|Debug")
	static FString GetShownDrillText(const AActor* Actor);

	/** Phase (EGSTN_DrillPhase as a byte), phase start frame, air spins used, low gravity until frame of the latest state; false without a fragment */
	UFUNCTION(BlueprintCallable, Category = "GSTN|SpinDrill|Debug")
	static bool GetShownDrillNumbers(const AActor* Actor, int32& OutPhase, int32& OutPhaseStartFrame, int32& OutAirSpinsUsed, int32& OutLowGravityUntilFrame);

	/** Whether the actor's ability system holds the air spin's low gravity effect (Combat.State.SpinLowGravity) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|SpinDrill|Debug")
	static bool HasSpinLowGravity(const AActor* Actor);
};

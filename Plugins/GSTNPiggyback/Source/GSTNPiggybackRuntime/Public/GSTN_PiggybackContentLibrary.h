// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PiggybackContentLibrary.generated.h"

class UGSTN_CombatRules;
class UGSTN_PrimaryKitDefinition;

/**
 * Content helpers of the plugin (setup scripts and the network harness, Tools/GASNPPTest/mount_scenarios.py): they write the mount data of a kit
 * and make the plugin's own test kits findable. Never called by the simulation. The test kits live in the plugin's Content/Test folder only
 * (KIT_Test_Mount, KIT_Test_MountHeavy); no project asset is edited.
 */
UCLASS()
class GSTNPIGGYBACKRUNTIME_API UGSTN_PiggybackContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds or replaces the kit's FGSTN_MountKitExtension (default profiles, bindings on) with these weights. Marks the kit dirty only on a change */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool WriteMountKitData(UGSTN_PrimaryKitDefinition* Kit, float RiderWeight, float CarryCapacity);

	/** The kit's mount data weights (RiderWeight, CarryCapacity); false without mount data */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool GetMountKitData(const UGSTN_PrimaryKitDefinition* Kit, float& RiderWeight, float& CarryCapacity);

	/**
	 * Adds the plugin's content to the asset manager's primary kit scan, so a kit saved there (the test kits) is found by its primary asset id.
	 * Called once the asset manager exists; again after a setup script created a kit. True when the plugin has a mounted content root
	 */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool ScanPluginKits();

	/** The registered mount link type's wire index (0 = not registered) */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static int32 GetMountTypeIndex();

	/**
	 * Inserts UGSTN_HitStep_MountHit right after ComputeKnockback (before ApplyDamage, MeterGain and LinkBreakOnHit: the rider's scaled knockback is
	 * what the later steps read, and the mount's break reason is marked first). Idempotent; never saves. False without ComputeKnockback
	 */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool AddMountStepToRules(UGSTN_CombatRules* Rules);

	/** Removes UGSTN_HitStep_MountHit from the pipeline. Never saves */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool RemoveMountStepFromRules(UGSTN_CombatRules* Rules);
};

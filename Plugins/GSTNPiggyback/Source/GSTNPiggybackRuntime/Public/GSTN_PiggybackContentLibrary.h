// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PiggybackContentLibrary.generated.h"

class UGSTN_CombatRules;
class UGSTN_PrimaryKitDefinition;
class UGSTN_PairMoveDefinition;
class UGSTN_PairMoveSet;

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

	/**
	 * TEST CONTENT (M7 G9 part B): fills Definition as one of the two reference pair moves of the spec (6.6) on the plugin's test abilities
	 * (UGSTN_GA_Test_*). PairTag names which: Combat.PairMove.ThrowRider (id 1, carrier throws on Command.Special.Neutral) or
	 * Combat.PairMove.BoostJump (id 2, rider boosts on Command.Movement.Jump). Marks it dirty on a change; false for another tag
	 */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool FillReferencePairMove(UGSTN_PairMoveDefinition* Definition, FGameplayTag PairTag);

	/** Sets the set's pair moves to Definitions (nulls skipped). Marks it dirty on a change */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool WritePairMoveSet(UGSTN_PairMoveSet* Set, const TArray<UGSTN_PairMoveDefinition*>& Definitions);

	/** Names Set as the pair moves of the kit's mount data (null clears it). False without mount data. Marks the kit dirty on a change */
	UFUNCTION(BlueprintCallable, Category = "Piggyback|Content")
	static bool WriteMountKitPairMoves(UGSTN_PrimaryKitDefinition* Kit, const UGSTN_PairMoveSet* Set);

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

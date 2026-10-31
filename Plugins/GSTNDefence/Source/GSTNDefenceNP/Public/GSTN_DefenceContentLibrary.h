// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_DefenceContentLibrary.generated.h"

class AActor;
class UGSTN_CombatRules;
class UGSTN_DefenceRules;
class UGSTN_PrimaryKitDefinition;

/**
 * Editor-time and harness helpers of the defence plugin (setup_defence_link_content.py, defence_scenarios.py): the kit extension entry
 * is an instanced struct that is awkward to edit from Python, and the harness attaches defence to a pawn by hand on every world the way
 * it attaches part sets. Idempotent; none of it is used by the simulation.
 */
UCLASS()
class GSTNDEFENCENP_API UGSTN_DefenceContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sets (adds or replaces) the kit's FGSTN_DefenceKitExtension: its rules and its shield ability class (null = the kit has no shield).
	 * Tech and get-up sets are left as they are (empty on a new entry). False for a null kit
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Content")
	static bool SetKitDefenceExtension(UGSTN_PrimaryKitDefinition* Kit, UGSTN_DefenceRules* Rules, TSubclassOf<UObject> ShieldAbility);

	/** Removes the kit's defence extension; true when the kit had one */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Content")
	static bool ClearKitDefenceExtension(UGSTN_PrimaryKitDefinition* Kit);

	/**
	 * F8: fills the kit's defence extension (added with no rules and no shield when the kit has none) with the plugin's native landing and tech
	 * abilities: with bLanding the get-up set (UGSTN_GA_GroundBounce, UGSTN_GA_GetupRoll, UGSTN_GA_GetupNeutral), with bTechSet the tech set
	 * (UGSTN_GA_Tech for InPlace, RollLeft, RollRight, WallTech, WallJumpTech); false clears that part. For the harness and tests (in memory)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Content")
	static bool SetKitDefenceMoves(UGSTN_PrimaryKitDefinition* Kit, bool bLanding, bool bTechSet);

	/** The held-direction drift used in the actor's current hitstop, cm (the combat state's Hit.SDIDistanceUsed); -1 without a combat state */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static float GetSDIDistanceUsed(const AActor* Actor);

	/**
	 * Attaches defence rules to an actor on this machine by hand, the way the loadout does from a kit (the bubble ignores the hit types of
	 * the actor's combat rules). Call it on every world that has the pawn. A null Rules detaches. Returns whether data is attached afterwards
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static bool AttachDefenceToActor(AActor* Actor, UGSTN_DefenceRules* Rules, bool bKitHasShield);

	/**
	 * The defence state of the actor's combat sync state as text ("value=25.50 flags=1 press=100 release=-1 stun=-1 recovery=-1 break=-1
	 * parry=[-1,-1)p0 last=-1 punish=-1 push=-1"), "none" without a fragment, "no combat" without a combat component. Frames and quantised
	 * integers only, so equal states give equal strings on every machine
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static FString GetDefenceStateString(const AActor* Actor);

	/**
	 * Puts the defence steps into a combat rules pipeline (m6 spec 9 steps 5, 6): Parry then Shield, after HurtboxReaction (else after
	 * HurtState, else first) and so before HealthWeight, PartResolve and ComputeKnockback. Idempotent; resolves the pipeline again. Returns
	 * whether the rules now hold both steps. The harness adds them in memory (never saved); content adds them to its rules asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Content")
	static bool AddDefenceStepsToRules(UGSTN_CombatRules* Rules);

	/** Removes the Parry and Shield steps again (the harness reset); true when one was removed */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Content")
	static bool RemoveDefenceStepsFromRules(UGSTN_CombatRules* Rules);

	/**
	 * Starts (or restarts) a log of the defence events (Parried, Blocked, ShieldBroken) the world of WorldContext publishes, rows
	 * "Type:frame:magnitude:target name" (debug and harness only, outside the simulation; the authority publishes, so a client world logs nothing)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static void StartDefenceEventLog(AActor* WorldContext);

	/** The rows since StartDefenceEventLog, joined with ";" */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static FString GetDefenceEventLog(AActor* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static void StopDefenceEventLog(AActor* WorldContext);

	/** The mirror attribute of the actor's ability system (ShieldHealth), -1 without the set */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static float GetShieldMirror(const AActor* Actor);
};

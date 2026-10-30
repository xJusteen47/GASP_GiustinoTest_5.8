// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_DefenceContentLibrary.generated.h"

class AActor;
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

	/** The mirror attribute of the actor's ability system (ShieldHealth), -1 without the set */
	UFUNCTION(BlueprintCallable, Category = "Combat|Defence|Debug")
	static float GetShieldMirror(const AActor* Actor);
};

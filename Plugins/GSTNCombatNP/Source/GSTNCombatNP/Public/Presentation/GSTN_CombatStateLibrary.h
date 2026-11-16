// Combat simulation (GAS-NPP fork): the combat state of any actor, for Blueprints (bp-exposure P1, owner 1A 2026-11-16).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Presentation/GSTN_CombatStateTypes.h"
#include "GSTN_CombatStateLibrary.generated.h"

class UGSTN_CombatSimComponent;

/**
 * Combat state queries that take the actor (Self by default), so an Animation Blueprint, a widget or a level script reads a fighter without
 * looking up its combat component first. All of them are PRESENTATION reads of the latest state this machine shows (see FGSTN_CombatStateView);
 * an actor without a combat component answers the defaults (not in hitstun, grounded, stage 0...). The events are on the combat component
 * (On Hit Received, On Landed...): bind them with Get Combat Component.
 */
UCLASS()
class GSTNCOMBATNP_API UGSTN_CombatStateLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The actor's combat component (UGSTN_CombatSimComponent), None if it has none */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static UGSTN_CombatSimComponent* GetCombatComponent(const AActor* Actor);

	/** Everything at once (cheapest when a graph reads several fields) */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static FGSTN_CombatStateView GetCombatState(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static EGSTN_CombatStatus GetCombatStatus(const AActor* Actor);

	/** Frames of hitstun left, 0 when none */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static int32 GetHitstunFramesLeft(const AActor* Actor);

	/** Frames of hitstop (hit freeze) left, 0 when none */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static int32 GetHitstopFramesLeft(const AActor* Actor);

	/** In a hit reaction that locks the controls (hitstun, tumble before it can act, splat) */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static bool IsInHitstun(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static bool IsInTumble(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static bool IsKnockedOut(const AActor* Actor);

	/** The last hit the actor received as its confirmed event reported it on this machine. False before the first */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static bool GetLastHitReceived(const AActor* Actor, FGSTN_CombatHitEventData& Hit);

	/** The last hit the actor landed as its confirmed event reported it on this machine. False before the first */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static bool GetLastHitLanded(const AActor* Actor, FGSTN_CombatHitEventData& Hit);

	/**
	 * The running move: its ability class, its timeline frame (0 = first frame, stands still in hitstop) and its length. False outside a move
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static bool GetCurrentMove(const AActor* Actor, TSubclassOf<UGSTN_GA_BaseCombat>& MoveAbility, int32& MoveFrame, int32& TotalFrames);

	/** The asset tags of the running move's ability, empty outside a move */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static FGameplayTagContainer GetCurrentMoveTags(const AActor* Actor);

	/** 0-based run stage (Mario run), 0 when the kit has no run stages */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static int32 GetRunStage(const AActor* Actor);

	/** Frames left of coyote time (a ground jump after walking off a ledge), 0 when none */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static int32 GetCoyoteFramesLeft(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static bool IsCombatAirborne(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static EGSTN_CombatAirState GetCombatAirState(const AActor* Actor);

	/** Simulation frames since the actor last landed, -1 while airborne or before its first landing */
	UFUNCTION(BlueprintPure, Category = "Combat|State", meta = (DefaultToSelf = "Actor"))
	static int32 GetFramesSinceLanded(const AActor* Actor);
};

// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/SubclassOf.h"
#include "GSTN_MoveActions.generated.h"

class UGSTN_GA_BaseCombat;
class UGameplayEffect;

/** What a move action runs with (CP2 of the Blueprint seams) */
struct FGSTN_MoveActionContext
{
	/** The running move. Null only in the rule tests */
	UGSTN_GA_BaseCombat* Ability = nullptr;

	/** The KeyTag of the Event key that runs the action; empty when a graph runs it (Run Move Action) */
	FGameplayTag KeyTag;

	/** The timeline frame the key was crossed on (stands still in hitstop), INDEX_NONE outside the move */
	int32 TimelineFrame = INDEX_NONE;
};

/**
 * A move action (CP2 of the Blueprint seams): a request a move makes from its timeline (the Actions of an Event key) or from its graph (Run Move
 * Action and the named nodes of UGSTN_GA_BaseCombat). It runs inside the ability's simulation step, on every machine and again in every
 * re-simulation, once per crossing (the key cursor is a sync variable), never while a frame is being restored. So an action only does what
 * simulation code may do, through the paths that are already rollback safe: an effect applied by the ability (a move goes through a queued
 * Mover move), a gameplay event to the own ability system, a loose tag the ability keeps in sync variables, a projectile through the host's
 * spawn-once path. It never writes character or combat state directly and never reads the world.
 *
 * A plugin adds its own actions by deriving from this struct in its own module (the projectile shot is in GSTNProjectileNP): the timeline's
 * Event key lists them as instanced structs of this base, so the Core data never names the plugin.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNCOMBATNP_API FGSTN_MoveAction
{
	GENERATED_BODY()

	virtual ~FGSTN_MoveAction() = default;

	/** Runs the action. Whether it did anything (false = nothing to do, or not allowed now) */
	virtual bool Execute(const FGSTN_MoveActionContext& Context) const { return false; }

	/** Editor check: empty when the action can run, else why not (the timeline validator reports it as a warning) */
	virtual FText Validate() const { return FText::GetEmpty(); }
};

/** Sends a gameplay event to the move's own ability system: abilities triggered by it start (and are adopted if they are timelines) */
USTRUCT(BlueprintType, meta = (DisplayName = "Send Event"))
struct GSTNCOMBATNP_API FGSTN_MoveAction_SendEvent : public FGSTN_MoveAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FGameplayTag EventTag;

	/** EventMagnitude of the payload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	float Magnitude = 0.f;

	virtual bool Execute(const FGSTN_MoveActionContext& Context) const override;
	virtual FText Validate() const override;
};

/**
 * Applies a gameplay effect to the move's owner. To move the character the effect carries a UApplyMoveGameplayEffectComponent (a queued Mover
 * move): an ability never sets the character's state itself
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Apply Effect"))
struct GSTNCOMBATNP_API FGSTN_MoveAction_ApplyEffect : public FGSTN_MoveAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0.0"))
	float Level = 1.f;

	virtual bool Execute(const FGSTN_MoveActionContext& Context) const override;
	virtual FText Validate() const override;
};

/**
 * Grants a tag for a number of combat frames (hitstop does not count) or until the move ends, whichever comes first: a chain window opened from
 * the graph, or from a key when the window's length is not known in advance. Opening the same tag again re-arms it.
 * UGSTN_GA_BaseCombat::MaxTagWindows windows at a time; a third replaces the one closing first
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Open Tag Window"))
struct GSTNCOMBATNP_API FGSTN_MoveAction_TagWindow : public FGSTN_MoveAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 Frames = 10;

	virtual bool Execute(const FGSTN_MoveActionContext& Context) const override;
	virtual FText Validate() const override;
};

/** The rules the move actions run with, split out so they run in a test without a world or an ability system */
struct GSTNCOMBATNP_API FGSTN_MoveActionRules
{
	/**
	 * Runs the move actions of the list in order and returns how many ran. An entry that is not a move action (empty, or a struct of a module that
	 * is not loaded) is skipped. Stops as soon as IsStillRunning says no: an action that ended the move (an event that started a move replacing it)
	 * leaves the rest of the list unrun, the same on every machine
	 */
	static int32 RunActions(TConstArrayView<FInstancedStruct> Actions, const FGSTN_MoveActionContext& Context, TFunctionRef<bool()> IsStillRunning);

	/**
	 * The slot a window of Tag opens in: the one that holds Tag already (re-armed), else a free one (empty tag), else the one closing first
	 * (Remaining = frames left, the earliest index on a tie). Tags and Remaining have one entry per slot
	 */
	static int32 PickTagWindowSlot(TConstArrayView<FGameplayTag> Tags, TConstArrayView<int32> Remaining, const FGameplayTag& Tag);

	/** True when a window opened at StartClock for Frames combat frames is over at Clock */
	static bool IsTagWindowOver(int32 Clock, int32 StartClock, int32 Frames);

	/**
	 * A deterministic random seed for a move: the same on every machine and in every re-simulation of the same frame (the simulation frame, the frame
	 * the move started on, a salt the graph picks to tell its draws apart). Never FMath::Rand in simulation code
	 */
	static uint32 MakeSeed(int32 SimulationFrame, int32 MoveStartFrame, int32 Salt);

	/** A float in [Min, Max] from the seed */
	static float RandomFloatInRange(uint32 Seed, float Min, float Max);

	/** An integer in [Min, Max] from the seed */
	static int32 RandomIntInRange(uint32 Seed, int32 Min, int32 Max);
};

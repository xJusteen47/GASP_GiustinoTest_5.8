// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Validation/GSTN_ValidationReport.h"

class UBlueprint;
class UFunction;
class UScriptStruct;
struct FEdGraphPinType;
struct FGSTN_ValidationReport;


/**
 * The rules of the simulation graphs of a combat ability Blueprint (CP2 of the Blueprint seams). The move events (On Move Started, On Move Key,
 * On Move Hit Confirmed, On Move Ended, On Move Frame), On Combat Frames Elapsed and the activation and end events run inside the simulation, on
 * every machine and again in a re-simulation; what they reach must be rollback safe. The rules walk what those events reach (exec links, the
 * nodes that feed their pins, the Blueprint's own functions and custom events they call) and report:
 * - Ability.SimLatent (error): Delay, timers and every latent function: they resume outside the simulation and are never rolled back.
 *   Async task nodes (ability tasks) are a warning: only the fork's rollback-safe tasks may run there.
 * - Ability.SimNonSyncVariable (warning): a Blueprint member variable written that is not a sync variable (FSyncBool, FSyncInteger...): a
 *   rollback does not restore it, so a re-simulation starts from the wrong value.
 * - Ability.SimWorldRead (warning): Mover component calls, actor transform and velocity reads, world time, traces and overlaps: none of them is
 *   the state of the frame being simulated (read the combat state, the snapshot and the input questions instead).
 * - Ability.SimRandom (warning): the engine's random nodes: different on every machine (use Get Move Random Float / Integer).
 * - Ability.SimTaskHint (warning): a task of the fork that is rollback safe but wrong for a combat move (raw input that skips the buffer, a
 *   montage that skips the hitboxes, a wait in seconds that counts the hitstop), with the combat node to use instead.
 * - Ability.SimSpawn (warning): Spawn Actor: a spawned actor is not rolled back and every re-simulation spawns another (Fire Projectile).
 * Every finding about a GAS node names the combat equivalent (move-timeline-decoupling 3A): Wait Delay and Delay -> Wait Combat Frames, Wait
 * Input Press -> Wait Command, Play Montage And Wait -> Play Combat Timeline And Wait, Apply Root Motion -> the Movement track or Apply Move
 * Effect, Wait Target Data -> Get Aim Rotation, hitboxes, Fire Projectile. Our own tasks (Play Combat Timeline And Wait, Wait Command, Wait
 * Combat Frames) and the fork's safe ones pass.
 * Nodes no simulation event reaches (presentation, a cosmetic hook) are not checked.
 */
struct FGSTN_SimGraphRules
{
	/** Is this event of the combat ability raised inside the simulation? */
	static bool IsSimulationEvent(FName EventFunctionName);

	/** Is this a struct of the fork's sync variables (derived from FBaseSyncVar)? */
	static bool IsSyncVarStruct(const UScriptStruct* Struct);

	/** Is this pin type a sync variable? */
	static bool IsSyncVarType(const FEdGraphPinType& Type);

	/** Delay-like: latent (a FLatentActionInfo) or a timer */
	static bool IsLatentFunction(const UFunction& Function);

	/** A read of something that is not the frame's simulated state (Mover, actor transform and velocity, world time, traces, overlaps) */
	static bool IsWorldRead(const UFunction& Function);

	/** An engine random node that is not seeded by the simulation */
	static bool IsUnseededRandom(const UFunction& Function);

	/** What the validator says about a task class used in a simulation graph */
	struct FTaskHint
	{
		/** False: the task is safe there (ours, or a fork task with no catch) */
		bool bFlagged = false;
		EGSTN_Severity Severity = EGSTN_Severity::Warning;
		/** The combat node or data to use instead, empty when there is none */
		FText Replacement;
		/** Why the task is wrong there */
		FText Reason;
	};

	/**
	 * The hint for a task class by name (the class's FName, no U prefix: AbilityTask_WaitDelay, WaitInputEventPredictionTask...). bForkTask: a
	 * task of the fork's prediction node (rollback safe, flagged only where a combat move needs another node); otherwise a GAS ability task or
	 * any other async node (never rollback safe: an error with the replacement when one is known, a warning when the task is unknown)
	 */
	static FTaskHint FindTaskHint(FName TaskClassName, bool bForkTask);

	/** Is this function a way to spawn an actor (Gameplay Statics' deferred spawn)? The Spawn Actor node is checked by its node class */
	static bool IsActorSpawn(const UFunction& Function);

	/** Walks the Blueprint's simulation graphs and adds the findings to Report (Asset is what they point at) */
	static void CheckBlueprint(const UBlueprint& Blueprint, UObject* Asset, FGSTN_ValidationReport& Report);
};

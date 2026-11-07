// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace GSTNMinion
{
	/** Bits of FGSTN_MinionIntent::Flags */
	namespace IntentFlags
	{
		constexpr uint8 Sprint = 1 << 0;
		/** Keep facing the target while moving (circling an enemy) instead of facing the move direction */
		constexpr uint8 StrafeTarget = 1 << 1;
	}

	/** Frames after which an unrefreshed intent is stale and the minion idles (combat-m8-minions.md 3) */
	constexpr int32 IntentStaleFrames = 30;
}

/**
 * What the AI (the brain, a StateTree, a script) asks a minion to do this frame. The brain never touches the store's position or
 * state: it writes an intent, and the batch stages read it and decide (movement, attack start, cooldown and token checks).
 * That keeps the simulation one code path whoever drives it, and makes a stale brain harmless.
 *
 * Latest value wins. The stamp is the server frame of the write; an intent older than IntentStaleFrames is ignored and the
 * minion idles, so a brain that stops (dead target, unloaded tree) cannot leave a minion walking forever. Plain data, not
 * reflected: the store holds one per slot and the batch copies it freely.
 */
struct GSTNMINIONCORE_API FGSTN_MinionIntent
{
	/** Wanted move direction on the ground plane, unit length or zero */
	FVector2f MoveDir = FVector2f::ZeroVector;

	/** Wanted ground speed in cm/s, clamped by the type's walk and run speed when applied */
	float DesiredSpeed = 0.f;

	/** Wanted facing in degrees (world yaw); read only when the face request is set */
	float DesiredYaw = 0.f;

	/** True when DesiredYaw is a request; false lets the movement face the move direction */
	bool bFaceRequested = false;

	/** The attack or move request: a Command.Minion.* tag selecting a move of the type's list. Invalid = none */
	FGameplayTag Command;

	/** Pipeline id of the target (GSTNEntityKinds ids), INDEX_NONE for none */
	int32 TargetEntityId = INDEX_NONE;

	/** GSTNMinion::IntentFlags */
	uint8 Flags = 0;

	/** Server frame of the write; INDEX_NONE for an intent that was never written */
	int32 StampFrame = INDEX_NONE;

	/** True when the intent was never written or is older than IntentStaleFrames at CurrentFrame */
	bool IsStale(int32 CurrentFrame) const
	{
		return StampFrame == INDEX_NONE || (CurrentFrame - StampFrame) > GSTNMinion::IntentStaleFrames;
	}

	/** True when the intent asks for a move or an attack that has a command */
	bool HasCommand() const { return Command.IsValid(); }

	bool HasFlag(uint8 Flag) const { return (Flags & Flag) != 0; }
};

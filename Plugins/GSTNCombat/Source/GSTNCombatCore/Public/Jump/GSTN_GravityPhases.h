// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"

/**
 * The authored gravity of a pawn, remembered next to the gravity the jump phases write over it. Mover's gravity override is one
 * value that the phases rewrite every simulated frame, so without this the scaled value would be read back as the pawn's gravity
 * and scaled again. Whatever else sets the gravity (a level object, a test, a physics volume) is noticed because the value
 * Mover reports is then not the one this wrote last, and becomes the new base.
 *
 * It is configuration mirrored from outside the simulation, never sync state: a rollback does not restore it and does not need to,
 * because it only changes when something outside the simulation changes the gravity. Hold it per pawn on the game thread.
 */
struct GSTNCOMBATCORE_API FGSTN_GravityBase
{
	/** The gravity (cm/s^2, any direction) the pawn has without the jump phases */
	FVector Base = FVector::ZeroVector;

	/** What the phases wrote last */
	FVector LastWritten = FVector::ZeroVector;

	bool bValid = false;
};

/**
 * The gravity phases of the jump (combat.md 11b) as pure functions of the local gravity: the pull keeps its direction, whatever it
 * is (tilted, sideways, inverted), and only its size follows the phase scale. Nothing here reads a world or assumes world Z.
 */
struct GSTNCOMBATCORE_API FGSTN_GravityPhases
{
	/**
	 * Updates Tracker from CurrentGravity (what the Mover component reports now) and returns the gravity to write for the given
	 * phase scale: base * Scale, same direction as the base. OutWrite is false when the pawn keeps the gravity it has (the scale is 1
	 * and nothing of ours is written over it), so a pawn the phases never touch is never given an override.
	 */
	static FVector Resolve(FGSTN_GravityBase& Tracker, const FVector& CurrentGravity, float Scale, bool& OutWrite);
};

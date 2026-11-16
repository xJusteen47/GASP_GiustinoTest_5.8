// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * The arithmetic behind the stats movement modifier (combat-m3 section 5), kept out of the Mover-facing code so it is testable
 * and so the rule that makes it rollback-safe lives in one place: a scaled value is always BASE * FACTOR, assigned, never
 * the current value times a factor and never a delta. The base is a value that no simulation ever writes (the archetype of the
 * object), so writing the same factor twice, or writing a factor after any other one, leaves the same value, whatever the
 * object held before (a rollback does not restore plain object members).
 */
struct GSTNCOMBATCORE_API FGSTN_StatsScaling
{
	/**
	 * Scale a stat gives relative to the value the content was authored for: Stat / Reference, never below 0. A reference of 0 or
	 * less means "no reference", the factor is 1 (the stat does not scale anything).
	 */
	static float ComputeFactor(float Stat, float Reference);

	/**
	 * Assigns Target's floating point (float or double) property of each name Base's value times Factor. A name the class does
	 * not have, or a property that is not a floating point number, is skipped. Base must be an object of the same class or of a parent
	 * class of Target's that has the properties (the archetype). Returns the number of properties assigned.
	 */
	static int32 ApplyScaledFloats(UObject& Target, const UObject& Base, TConstArrayView<FName> Names, float Factor);

	/** Reads the floating point property Name of Object. False when the class has no such property or it is not a floating point number */
	static bool ReadFloat(const UObject& Object, FName Name, double& OutValue);
};

// On-screen speed graph of the local pawn (greybox-capsula 1A, owner 2026-11-24): tune the run start feel on the speed curve, not in the clip.

#pragma once

#include "CoreMinimal.h"

/**
 * gstn.Debug.SpeedGraph 1 draws, bottom left of the game view, the local pawn's horizontal speed over the last seconds (measured from its
 * presented location, so it is exactly what the eye sees), the current and peak speed, and the times of the last start from rest to 50 %
 * and 90 % of the speed it settled at (t50 / t90, the numbers of docs/notes/runstart-accel/report.md). Debug only, not in Shipping.
 */
class FGSTN_SpeedGraph
{
public:
	static void Startup();
	static void Shutdown();

	/** Pure: t50 / t90 of a start from rest. Samples are (seconds since the start, speed); the reference is the highest speed reached.
	 *  Returns false when the samples never reach 90 %. The tests call it. */
	static bool MeasureStart(const TArray<FVector2D>& Samples, float& OutT50, float& OutT90);
};

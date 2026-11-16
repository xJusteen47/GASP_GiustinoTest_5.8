// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

/**
 * The phase scheduler of the batch (combat-m8-minions.md 7, "Scheduling"): which slots a tier steps in which frame, so the cost
 * of a tier with interval P is spread over P frames instead of landing in one.
 *
 * A slot with interval P runs in frame F when (F + Index) % P == 0, that is its phase is Index % P. Slots with consecutive
 * indices therefore fall in consecutive frames, and N such slots load every frame with floor(N / P) or ceil(N / P) of them.
 * Interval 1 runs every frame. The rule reads only (Frame, Index, Interval): no clock, no random, no hidden state, so the
 * server, a replay and a test agree. A slot that runs steps its timeline by Interval frames in one go (timelines are frame
 * stamps), which is why the interval is part of the step; see GSTNMinionSchedule::StepFrames.
 */
namespace GSTNMinionSchedule
{
	/** The frame offset of a slot inside its interval: Index % Interval (Interval below 1 counts as 1) */
	GSTNMINIONCORE_API int32 Phase(int32 SlotIndex, int32 Interval);

	/** True when the slot runs in Frame (F + Index) % Interval == 0. Frame and Index must be non-negative */
	GSTNMINIONCORE_API bool ShouldRun(int32 Frame, int32 SlotIndex, int32 Interval);

	/** The first frame at or after Frame in which the slot runs */
	GSTNMINIONCORE_API int32 NextRunFrame(int32 Frame, int32 SlotIndex, int32 Interval);

	/** Frames of simulated time a run of this interval advances: the interval itself (a T1 minion steps its timeline by 2) */
	GSTNMINIONCORE_API int32 StepFrames(int32 Interval);

	/**
	 * Appends, in ascending slot order, the slots of [0, SlotCount) that run in Frame. Convenience for tests and tools;
	 * the batch walks its own columns with ShouldRun.
	 */
	GSTNMINIONCORE_API void CollectRunning(int32 Frame, int32 SlotCount, int32 Interval, TArray<int32>& OutSlots);
}

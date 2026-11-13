// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Schedule/GSTN_MinionSchedule.h"

namespace GSTNMinionSchedule
{
	int32 Phase(int32 SlotIndex, int32 Interval)
	{
		return Interval <= 1 ? 0 : SlotIndex % Interval;
	}

	bool ShouldRun(int32 Frame, int32 SlotIndex, int32 Interval)
	{
		if (Interval <= 1)
		{
			return true;
		}
		// Reduce both terms first so a large frame counter cannot overflow the sum
		return ((Frame % Interval) + (SlotIndex % Interval)) % Interval == 0;
	}

	int32 NextRunFrame(int32 Frame, int32 SlotIndex, int32 Interval)
	{
		if (Interval <= 1)
		{
			return Frame;
		}
		const int32 Wait = (Interval - ((Frame % Interval) + (SlotIndex % Interval)) % Interval) % Interval;
		return Frame + Wait;
	}

	int32 StepFrames(int32 Interval)
	{
		return FMath::Max(Interval, 1);
	}

	void CollectRunning(int32 Frame, int32 SlotCount, int32 Interval, TArray<int32>& OutSlots)
	{
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			if (ShouldRun(Frame, Slot, Interval))
			{
				OutSlots.Add(Slot);
			}
		}
	}
}

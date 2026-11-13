// Minion runtime: lag compensation history of the authority batch (M8 G5, spike 5).

#include "LagComp/GSTN_MinionHistory.h"

void FGSTN_MinionHistory::Reset(int32 Capacity, int32 InDepth)
{
	Depth = FMath::Max(InDepth, 2);
	Records.Reset();
	Records.SetNum(FMath::Max(Capacity, 0) * Depth);
	LastFrame.Init(INDEX_NONE, FMath::Max(Capacity, 0));
	Cells.Reset();
}

void FGSTN_MinionHistory::ClearSlot(int32 Slot)
{
	if (!LastFrame.IsValidIndex(Slot))
	{
		return;
	}
	for (int32 Index = Slot * Depth; Index < (Slot + 1) * Depth; ++Index)
	{
		Records[Index] = FGSTN_MinionHistoryRecord();
	}
	LastFrame[Slot] = INDEX_NONE;
}

void FGSTN_MinionHistory::Write(int32 Slot, int32 Frame, const FVector3f& Position, float YawDegrees, float BoundRadius, uint8 ReactionFlags)
{
	if (!LastFrame.IsValidIndex(Slot) || Frame < 0)
	{
		return;
	}
	const float Radius = FMath::Max(BoundRadius, 0.f);
	FBox3f Bound(Position - FVector3f(Radius), Position + FVector3f(Radius));
	// The previous record still in the ring: the sweep of the frame (a slot the LOD skipped sweeps over the skipped frames too)
	if (const FGSTN_MinionHistoryRecord* Previous = Find(Slot, Frame - 1))
	{
		Bound += FBox3f(Previous->Position - FVector3f(Radius), Previous->Position + FVector3f(Radius));
	}

	FGSTN_MinionHistoryRecord& Record = Records[IndexOf(Slot, Frame)];
	Record.Frame = Frame;
	Record.Position = Position;
	Record.YawDegrees = YawDegrees;
	Record.SweptBound = Bound;
	Record.ReactionFlags = ReactionFlags;
	LastFrame[Slot] = Frame;
}

const FGSTN_MinionHistoryRecord* FGSTN_MinionHistory::Find(int32 Slot, int32 Frame) const
{
	if (!LastFrame.IsValidIndex(Slot) || LastFrame[Slot] == INDEX_NONE || Frame < 0)
	{
		return nullptr;
	}
	// Nothing newer than the last write exists: start there, so a frame in the future reads the present
	const int32 From = FMath::Min(Frame, LastFrame[Slot]);
	for (int32 Back = 0; Back < Depth; ++Back)
	{
		const int32 Wanted = From - Back;
		if (Wanted < 0)
		{
			break;
		}
		const FGSTN_MinionHistoryRecord& Record = Records[IndexOf(Slot, Wanted)];
		// The stamp tells a record of this lap from one of an older lap that shares the index
		if (Record.Frame == Wanted)
		{
			return &Record;
		}
	}
	return nullptr;
}

const FGSTN_MinionHistoryRecord* FGSTN_MinionHistory::FindOldest(int32 Slot) const
{
	if (!LastFrame.IsValidIndex(Slot) || LastFrame[Slot] == INDEX_NONE)
	{
		return nullptr;
	}
	// Ascending from the oldest frame the ring can still hold; a slot the LOD skipped has gaps, so the first stamped match is the oldest
	for (int32 Wanted = FMath::Max(LastFrame[Slot] - Depth + 1, 0); Wanted <= LastFrame[Slot]; ++Wanted)
	{
		const FGSTN_MinionHistoryRecord& Record = Records[IndexOf(Slot, Wanted)];
		if (Record.Frame == Wanted)
		{
			return &Record;
		}
	}
	return nullptr;
}

void FGSTN_MinionHistory::RebuildHash(TConstArrayView<int32> Slots, TConstArrayView<FVector3f> PositionBySlot, float CellSize)
{
	HashCellSize = FMath::Max(CellSize, 1.f);
	for (TPair<FIntPoint, TArray<int32>>& Pair : Cells)
	{
		Pair.Value.Reset();
	}
	for (const int32 Slot : Slots)
	{
		if (!PositionBySlot.IsValidIndex(Slot))
		{
			continue;
		}
		const FVector3f& Position = PositionBySlot[Slot];
		Cells.FindOrAdd(CellOf(Position.X, Position.Y)).Add(Slot);
	}
}

void FGSTN_MinionHistory::QueryHash(const FBox& Box, TArray<int32>& OutSlots) const
{
	OutSlots.Reset();
	if (!Box.IsValid)
	{
		return;
	}
	const FIntPoint Min = CellOf(Box.Min.X, Box.Min.Y);
	const FIntPoint Max = CellOf(Box.Max.X, Box.Max.Y);
	const int64 Span = (static_cast<int64>(Max.X) - Min.X + 1) * (static_cast<int64>(Max.Y) - Min.Y + 1);
	if (Span > Cells.Num())
	{
		// A box wider than the occupied cells: walk the occupied cells instead (the sort below makes the walk order irrelevant)
		for (const TPair<FIntPoint, TArray<int32>>& Pair : Cells)
		{
			if (Pair.Key.X >= Min.X && Pair.Key.X <= Max.X && Pair.Key.Y >= Min.Y && Pair.Key.Y <= Max.Y)
			{
				OutSlots.Append(Pair.Value);
			}
		}
		OutSlots.Sort();
		return;
	}
	for (int32 X = Min.X; X <= Max.X; ++X)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			if (const TArray<int32>* Cell = Cells.Find(FIntPoint(X, Y)))
			{
				OutSlots.Append(*Cell);
			}
		}
	}
	// A slot is in one cell only, but the order across cells is the walk's: sort so the caller sees ascending slots
	OutSlots.Sort();
}

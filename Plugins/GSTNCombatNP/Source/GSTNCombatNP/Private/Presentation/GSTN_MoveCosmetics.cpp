// Combat presentation (CP4 of the Blueprint seams): the cosmetic window tracker.

#include "Presentation/GSTN_MoveCosmetics.h"

void FGSTN_MoveCosmeticTracker::Update(int32 Entry, int32 RunStartFrame, int32 TimelineFrame, TConstArrayView<FGSTN_MoveCosmeticWindow> Windows,
	double NowSeconds, double MemorySeconds, FDelta& OutDelta)
{
	const bool bRunLive = Entry != INDEX_NONE && RunStartFrame != INDEX_NONE && TimelineFrame != INDEX_NONE;

	// The windows live on this frame
	TArray<FGSTN_MoveCosmeticKey, TInlineAllocator<8>> Live;
	if (bRunLive)
	{
		for (int32 Index = 0; Index < Windows.Num(); ++Index)
		{
			if (Windows[Index].IsLiveAt(TimelineFrame))
			{
				Live.Add({ Entry, RunStartFrame, Index });
			}
		}
	}

	// Windows no longer live end (each key is its own effect, so the caller may process Began before Ended)
	for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
	{
		const FActive& Item = Active[Index];
		if (Live.Contains(Item.Key))
		{
			continue;
		}
		const bool bSameRun = bRunLive && Item.Key.Entry == Entry && Item.Key.RunStartFrame == RunStartFrame;
		bool bNatural = false;
		if (bSameRun)
		{
			// Still the same run: past the window is its end, before it is a correction that took the frames back
			bNatural = TimelineFrame > Item.StartFrame;
		}
		else
		{
			// The run is over or replaced: natural when the window had nothing left but its tail (or it lasts until the move ends)
			bNatural = Item.EndFrame < 0 || (LastFrame != INDEX_NONE && LastFrame >= Item.EndFrame);
		}
		OutDelta.Ended.Add({ Item.Key, !bNatural });
		Active.RemoveAt(Index);
	}

	for (const FGSTN_MoveCosmeticKey& Key : Live)
	{
		int32 Remembered = INDEX_NONE;
		for (int32 Index = 0; Index < Begun.Num(); ++Index)
		{
			if (Begun[Index].Key == Key)
			{
				Remembered = Index;
				break;
			}
		}
		if (Remembered != INDEX_NONE)
		{
			Begun[Remembered].Value = NowSeconds;
		}
		if (Active.ContainsByPredicate([&Key](const FActive& Item) { return Item.Key == Key; }))
		{
			continue;
		}
		if (Remembered != INDEX_NONE)
		{
			// It began already and a correction dropped it for a while: no replay
			continue;
		}
		Active.Add({ Key, Windows[Key.Window].StartFrame, Windows[Key.Window].EndFrame });
		Begun.Add({ Key, NowSeconds });
		OutDelta.Began.Add(Key);
	}

	// Windows crossed between two presented frames (the game draws slower than the 62.5 Hz simulation): a one-shot on a frame the screen never
	// showed still plays, and ends at once
	const bool bSameRunAsLast = bRunLive && Entry == LastEntry && RunStartFrame == LastRunStartFrame && LastFrame != INDEX_NONE;
	if (bSameRunAsLast && TimelineFrame > LastFrame)
	{
		for (int32 Index = 0; Index < Windows.Num(); ++Index)
		{
			const FGSTN_MoveCosmeticKey Key{ Entry, RunStartFrame, Index };
			const int32 Start = Windows[Index].StartFrame;
			if (Start <= LastFrame || Start > TimelineFrame || Live.Contains(Key))
			{
				continue;
			}
			bool bRemembered = false;
			for (const TPair<FGSTN_MoveCosmeticKey, double>& Pair : Begun)
			{
				bRemembered |= Pair.Key == Key;
			}
			if (!bRemembered)
			{
				Begun.Add({ Key, NowSeconds });
				OutDelta.Began.Add(Key);
				OutDelta.Ended.Add({ Key, false });
			}
		}
	}
	LastEntry = bRunLive ? Entry : INDEX_NONE;
	LastRunStartFrame = bRunLive ? RunStartFrame : INDEX_NONE;
	LastFrame = bRunLive ? TimelineFrame : INDEX_NONE;

	// Forget keys not live for MemorySeconds (their run is long over)
	for (int32 Index = Begun.Num() - 1; Index >= 0; --Index)
	{
		const FGSTN_MoveCosmeticKey& Key = Begun[Index].Key;
		if (NowSeconds - Begun[Index].Value > MemorySeconds && !Active.ContainsByPredicate([&Key](const FActive& Item) { return Item.Key == Key; }))
		{
			Begun.RemoveAtSwap(Index);
		}
	}
}

void FGSTN_MoveCosmeticTracker::EndAll(FDelta& OutDelta)
{
	for (const FActive& Item : Active)
	{
		OutDelta.Ended.Add({ Item.Key, true });
	}
	Active.Reset();
	LastEntry = LastRunStartFrame = LastFrame = INDEX_NONE;
}

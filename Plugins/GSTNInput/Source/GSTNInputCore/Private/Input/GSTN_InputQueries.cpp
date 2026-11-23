// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputQueries.h"

#include "Input/GSTN_InputMatcher.h"

namespace
{
	FORCEINLINE int32 AgeOf(int32 Frame, const FGSTN_InputQueries::FAgeContext& Context)
	{
		// The buffer's own age rule, so "pressed within N frames" and the buffer of N frames agree on every press
		FGSTN_MatchParams Params;
		Params.Now = Context.Now;
		Params.PausedStart = Context.PausedStart;
		Params.PausedEnd = Context.PausedEnd;
		return FGSTN_InputMatcher::GetAge(Frame, Params);
	}

	FORCEINLINE bool IsPressOf(const FGSTN_ButtonEvent& Event, int32 Button, int32 Now, bool bIncludeUsed)
	{
		return Event.Button == Button && !Event.bRelease && (bIncludeUsed || !Event.bConsumed) && Event.Frame <= Now;
	}
}

int32 FGSTN_InputQueries::GetNewestPressAge(const FGSTN_InputHistory& History, int32 Button, const FAgeContext& Context, bool bIncludeUsed)
{
	if (!IsValidButton(Button))
	{
		return INDEX_NONE;
	}
	// Newest first: the ring is in append order, so the first press found is the newest one
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		if (IsPressOf(Event, Button, Context.Now, bIncludeUsed))
		{
			return AgeOf(Event.Frame, Context);
		}
	}
	return INDEX_NONE;
}

int32 FGSTN_InputQueries::CountPressesWithin(const FGSTN_InputHistory& History, int32 Button, const FAgeContext& Context, int32 Frames, bool bIncludeUsed)
{
	if (!IsValidButton(Button) || Frames < 0)
	{
		return 0;
	}
	int32 Count = 0;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		if (Event.Frame > Context.Now)
		{
			continue;
		}
		if (AgeOf(Event.Frame, Context) > Frames)
		{
			// Every older event is older still
			break;
		}
		Count += IsPressOf(Event, Button, Context.Now, bIncludeUsed) ? 1 : 0;
	}
	return Count;
}

int32 FGSTN_InputQueries::GetHeldFrames(const FGSTN_InputHistory& History, int32 Button, int32 Now)
{
	int32 HeldFrames = 0;
	return IsValidButton(Button) && History.IsButtonHeldAt(Button, Now, HeldFrames) ? HeldFrames : INDEX_NONE;
}

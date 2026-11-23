// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputTags.h"

bool FGSTN_BufferResetRule_AfterIdleFrames::ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& History, int32 Now) const
{
	int32 LastEventFrame = FGSTN_InputHistory::AncientFrame;
	if (History.GetNumButtonEvents() > 0)
	{
		LastEventFrame = FMath::Max(LastEventFrame, History.GetButtonEvent(History.GetButtonSlot(0)).Frame);
	}
	if (History.GetNumDirectionEvents() > 0)
	{
		LastEventFrame = FMath::Max(LastEventFrame, History.GetDirectionEvent(0).Frame);
	}
	return Now - LastEventFrame >= Frames;
}

bool FGSTN_BufferResetRules::Apply(FGSTN_InputHistory& History, const FGSTN_ButtonMap& Buttons,
	TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> Rules, const FGSTN_BufferFrameEvents& Events, int32 Now)
{
	bool bFired = false;
	for (const TInstancedStruct<FGSTN_BufferResetRule>& Instance : Rules)
	{
		const FGSTN_BufferResetRule* Rule = Instance.GetPtr();
		if (Rule && Rule->ShouldFlush(Events, History, Now))
		{
			// An empty filter flushes everything. A filter of buttons the config does not have flushes nothing.
			const uint32 Mask = Rule->Buttons.IsEmpty() ? MAX_uint32 : Buttons.GetMask(Rule->Buttons);
			History.FlushPresses(Mask, Now);
			bFired = true;
		}
	}
	return bFired;
}

bool FGSTN_BufferResetRules::ApplyInterrupted(FGSTN_InputHistory& History, const FGSTN_BufferFrameEvents& Events, int32 Now)
{
	if (!Events.EventTags.HasTagExact(GSTNInputTags::BufferEvent_Interrupted))
	{
		return false;
	}
	History.FlushPresses(MAX_uint32, Now);
	return true;
}

void FGSTN_BufferResetRules::GetWatchedTags(TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> Rules, FGameplayTagContainer& OutTags)
{
	for (const TInstancedStruct<FGSTN_BufferResetRule>& Instance : Rules)
	{
		if (const FGSTN_BufferResetRule* Rule = Instance.GetPtr())
		{
			Rule->GetWatchedTags(OutTags);
		}
	}
}

// Links, Network Prediction half.

#include "Sim/GSTN_LinkMash.h"

#include "Data/GSTN_GrabRules.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputHistory.h"
#include "Sim/GSTN_LinkFragment.h"
#include "State/GSTN_LinkMath.h"

int32 FGSTN_LinkMash::InitialEndFrame(const FGSTN_MashRule& Rule, int32 LinkFrame, float DamagePercent, float Resistance)
{
	return LinkFrame + UGSTN_LinkMath::MashInitialFrames(Rule, DamagePercent, Resistance);
}

uint32 FGSTN_LinkMash::ButtonMaskOf(const FGSTN_MashRule& Rule, const FGSTN_ButtonMap* Buttons)
{
	if (Rule.Buttons.IsEmpty() || !Buttons)
	{
		return MAX_uint32;
	}
	uint32 Mask = 0;
	for (const FGameplayTag& Button : Rule.Buttons)
	{
		const int32 Index = Buttons->GetIndex(Button);
		if (Index >= 0 && Index < 32)
		{
			Mask |= 1u << Index;
		}
	}
	return Mask;
}

void FGSTN_LinkMash::CountSince(const FGSTN_InputHistory& History, uint32 Mask, int32 After, int32& OutPresses, int32& OutFlicks)
{
	OutPresses = 0;
	OutFlicks = 0;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		if (!Event.bRelease && Event.Frame > After && (Mask & (1u << Event.Button)) != 0)
		{
			++OutPresses;
		}
	}
	for (int32 Age = 0; Age < History.GetNumDirectionEvents(); ++Age)
	{
		const FGSTN_DirectionEvent& Event = History.GetDirectionEvent(Age);
		if (Event.Stick.bFlick && Event.Frame > After)
		{
			++OutFlicks;
		}
	}
}

bool FGSTN_LinkMash::Step(FGSTN_LinkFragment& Fragment, const FGSTN_InputHistory& History, const FGSTN_MashRule& Rule, uint32 Mask, int32 Frame)
{
	FGSTN_LinkState& Body = Fragment.Links.Body;
	if (!Body.IsActive() || Body.EndFrame <= 0 || UGSTN_LinkMath::MashEscaped(Body.EndFrame, Frame))
	{
		return false;
	}
	// Only presses made while linked count: a press of the grab's own frame or earlier is the past
	const int32 After = FMath::Max(Fragment.MashProcessedFrame, Body.LinkFrame - 1);
	int32 Presses = 0;
	int32 Flicks = 0;
	CountSince(History, Mask, After, Presses, Flicks);
	const int32 Latest = History.GetLatestFrame();
	const int32 NewEnd = UGSTN_LinkMath::MashProgress(Rule, Body.EndFrame, Frame, Presses, Flicks);
	const bool bChanged = NewEnd != Body.EndFrame || (Latest > Fragment.MashProcessedFrame);
	Body.EndFrame = NewEnd;
	Fragment.MashProcessedFrame = FMath::Max(Fragment.MashProcessedFrame, Latest);
	return bChanged;
}

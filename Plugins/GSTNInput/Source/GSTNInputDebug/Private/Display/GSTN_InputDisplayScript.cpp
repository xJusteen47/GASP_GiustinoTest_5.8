// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Display/GSTN_InputDisplayScript.h"

#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_SlotResolver.h"

FGSTN_InputDisplayScriptResult FGSTN_InputDisplayScript::Resolve(const FString& Notation, TConstArrayView<FGameplayTag> ButtonTags,
	TConstArrayView<const FGSTN_InputSlotSpec*> Slots, const UGSTN_InputSettings& Settings)
{
	FGSTN_InputDisplayScriptResult Result;

	// Parse then Advance rather than FGSTN_InputScript::Build: a typo while typing is a message in the widget, not a log error
	TArray<FGameplayTag> Tags;
	Tags.Append(ButtonTags);
	FGSTN_InputScript Script(MoveTemp(Tags));
	if (!Script.Parse(Notation, Result.Error))
	{
		return Result;
	}
	if (Script.GetLastFrame() == INDEX_NONE)
	{
		Result.Error = TEXT("empty notation");
		return Result;
	}
	if (!Script.Advance(Result.Data.History, Settings, Script.GetLastFrame(), &Result.Error))
	{
		Result.Data = FGSTN_InputDisplayData();
		return Result;
	}
	Result.bOk = true;
	Result.LastFrame = Result.Data.History.GetLatestFrame();

	const FGSTN_ButtonMap Buttons{ ButtonTags };
	FGSTN_ResolveParams Params;
	Params.Buttons = &Buttons;
	Params.Now = Result.LastFrame;
	const FGSTN_SlotResolution Resolution = FGSTN_SlotResolver::Resolve(Slots, Result.Data.History, Params, Settings, [](int32) { return true; });
	if (Resolution.SlotIndex != INDEX_NONE && Slots.IsValidIndex(Resolution.SlotIndex))
	{
		Result.SlotIndex = Resolution.SlotIndex;
		Result.Command = Slots[Resolution.SlotIndex]->Command;
		// What the simulation does once the command is requested, so the strip greys the press the command used
		Resolution.ConsumeFrom(Result.Data.History);
		Result.Data.Markers.Add({ Result.LastFrame, Result.Command });
	}
	return Result;
}

// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_BlueprintRules.h"

#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputPattern.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlot.h"

bool FGSTN_InputCondition_Blueprint::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const UGSTN_InputConditionBlueprint* Object = Condition ? Condition->GetDefaultObject<UGSTN_InputConditionBlueprint>() : nullptr;
	if (!Object)
	{
		return false;
	}

	FGSTN_BlueprintConditionView View;
	View.AnchorFrame = AnchorFrame;
	View.Now = Ctx.Now;
	View.SimultaneityFrames = Ctx.SimultaneityFrames;
	for (int32 Index = 0; Index < Ctx.Buttons.GetNum(); ++Index)
	{
		int32 HeldFrames = 0;
		if (Ctx.History.IsButtonHeldAt(Index, AnchorFrame, HeldFrames))
		{
			View.HeldButtons.AddTag(Ctx.Buttons.GetTag(Index));
		}
	}
	const FGSTN_StickSample Stick = Ctx.History.GetStickAt(AnchorFrame);
	View.StickZone = Stick.Zone;
	View.FacingDirection = Stick.FacingDir;
	View.StickDirection = Stick.StickDir;
	View.WorldDirection = Stick.WorldDir;

	if (!Object->Matches(View))
	{
		return false;
	}
	Out = FGSTN_ConditionResult();
	Out.Frame = AnchorFrame;
	return true;
}

int32 FGSTN_PriorityCriterion_Blueprint::Score(const FGSTN_PriorityContext& Context) const
{
	const UGSTN_PriorityCriterionBlueprint* Object = Criterion ? Criterion->GetDefaultObject<UGSTN_PriorityCriterionBlueprint>() : nullptr;
	if (!Object)
	{
		return 0;
	}

	FGSTN_BlueprintPriorityView View;
	View.Command = Context.Slot.Command;
	View.Tier = Context.Settings.GetCommandTier(Context.Slot.Command);
	View.Specificity = Context.Pattern.GetSpecificity();
	View.ManualOffset = Context.Slot.PriorityOffset;
	return Object->Score(View);
}

bool FGSTN_BufferResetRule_Blueprint::ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& History, int32 Now) const
{
	const UGSTN_BufferResetRuleBlueprint* Object = Rule ? Rule->GetDefaultObject<UGSTN_BufferResetRuleBlueprint>() : nullptr;
	if (!Object)
	{
		return false;
	}

	FGSTN_BlueprintResetView View;
	View.EventTags = Events.EventTags;
	View.TagsAdded = Events.TagsAdded;
	View.Now = Now;
	View.HeldButtonsMask = static_cast<int32>(History.GetHeldButtons());

	int32 LastEventFrame = FGSTN_InputHistory::AncientFrame;
	if (History.GetNumButtonEvents() > 0)
	{
		LastEventFrame = FMath::Max(LastEventFrame, History.GetButtonEvent(History.GetButtonSlot(0)).Frame);
	}
	if (History.GetNumDirectionEvents() > 0)
	{
		LastEventFrame = FMath::Max(LastEventFrame, History.GetDirectionEvent(0).Frame);
	}
	View.FramesSinceLastInput = Now - LastEventFrame;
	return Object->ShouldFlush(View);
}

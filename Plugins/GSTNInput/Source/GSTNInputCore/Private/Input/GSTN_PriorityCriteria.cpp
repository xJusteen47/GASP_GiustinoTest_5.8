// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_PriorityCriteria.h"
#include "Input/GSTN_InputPattern.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlot.h"

int32 FGSTN_PriorityCriterion_Tier::Score(const FGSTN_PriorityContext& Context) const
{
	return Context.Settings.GetCommandTier(Context.Slot.Command);
}

int32 FGSTN_PriorityCriterion_Specificity::Score(const FGSTN_PriorityContext& Context) const
{
	return Context.Pattern.GetSpecificity();
}

int32 FGSTN_PriorityCriterion_ManualOffset::Score(const FGSTN_PriorityContext& Context) const
{
	return Context.Slot.PriorityOffset;
}

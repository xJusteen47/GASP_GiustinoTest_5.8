// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputConfig.h"
#include "InputAction.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GSTNInputConfig"

int32 UGSTN_InputConfig::FindButtonIndex(const FGameplayTag& ButtonTag) const
{
	if (!ButtonTag.IsValid())
	{
		return INDEX_NONE;
	}
	return Buttons.IndexOfByPredicate([&ButtonTag](const FGSTN_InputButtonDef& Button)
	{
		return Button.Tag == ButtonTag;
	});
}

uint32 UGSTN_InputConfig::GetButtonMask(const FGameplayTag& ButtonTag) const
{
	const int32 Index = FindButtonIndex(ButtonTag);
	if (Index < 0 || Index >= 32)
	{
		return 0u;
	}
	return 1u << Index;
}

FGSTN_ButtonMap UGSTN_InputConfig::MakeButtonMap() const
{
	TArray<FGameplayTag, TInlineAllocator<MaxButtons>> Tags;
	for (const FGSTN_InputButtonDef& Button : Buttons)
	{
		Tags.Add(Button.Tag);
	}
	// A config over the limit is reported by IsDataValid; bind only the bits that exist.
	Tags.SetNum(FMath::Min(Tags.Num(), MaxButtons));
	return FGSTN_ButtonMap(Tags);
}

void UGSTN_InputConfig::ResolveActions()
{
	ResolvedButtonActions.Reset(Buttons.Num());
	for (const FGSTN_InputButtonDef& Button : Buttons)
	{
		ResolvedButtonActions.Add(Button.Action.IsNull() ? nullptr : Button.Action.LoadSynchronous());
	}
	ResolvedMoveAction = MoveAction.IsNull() ? nullptr : MoveAction.LoadSynchronous();
}

UInputAction* UGSTN_InputConfig::GetButtonAction(int32 ButtonIndex) const
{
	return ResolvedButtonActions.IsValidIndex(ButtonIndex) ? ResolvedButtonActions[ButtonIndex].Get() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UGSTN_InputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Buttons.Num() > MaxButtons)
	{
		Context.AddError(FText::Format(LOCTEXT("TooManyButtons", "{0} buttons, the input masks hold {1}."), Buttons.Num(), MaxButtons));
		Result = EDataValidationResult::Invalid;
	}

	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		const FGSTN_InputButtonDef& Button = Buttons[Index];
		if (!Button.Tag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("ButtonNoTag", "Button {0} has no tag."), Index));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (Button.Action.IsNull())
		{
			Context.AddWarning(FText::Format(LOCTEXT("ButtonNoAction", "Button {0} ({1}) has no input action: only AI and scripts can press it."),
				Index, FText::FromName(Button.Tag.GetTagName())));
		}

		for (int32 Other = 0; Other < Index; ++Other)
		{
			if (Buttons[Other].Tag == Button.Tag)
			{
				Context.AddError(FText::Format(LOCTEXT("ButtonDuplicateTag", "Buttons {0} and {1} have the same tag {2}."),
					Other, Index, FText::FromName(Button.Tag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
			if (!Button.Action.IsNull() && Buttons[Other].Action == Button.Action)
			{
				Context.AddError(FText::Format(LOCTEXT("ButtonDuplicateAction", "Buttons {0} and {1} use the same input action."), Other, Index));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE

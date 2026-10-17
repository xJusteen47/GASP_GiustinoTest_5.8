// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatInputConfig.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GSTNCombatInputConfig"

int32 UGSTN_CombatInputConfig::FindButtonIndex(const FGameplayTag& ButtonTag) const
{
	if (!ButtonTag.IsValid())
	{
		return INDEX_NONE;
	}
	return Buttons.IndexOfByPredicate([&ButtonTag](const FGSTN_CombatButtonDef& Button)
	{
		return Button.Tag == ButtonTag;
	});
}

uint32 UGSTN_CombatInputConfig::GetButtonMask(const FGameplayTag& ButtonTag) const
{
	const int32 Index = FindButtonIndex(ButtonTag);
	return Index == INDEX_NONE ? 0u : (1u << Index);
}

FGSTN_ButtonMap UGSTN_CombatInputConfig::MakeButtonMap() const
{
	TArray<FGameplayTag, TInlineAllocator<MaxButtons>> Tags;
	for (const FGSTN_CombatButtonDef& Button : Buttons)
	{
		Tags.Add(Button.Tag);
	}
	// A config over the limit is reported by IsDataValid; bind only the bits that exist.
	Tags.SetNum(FMath::Min(Tags.Num(), MaxButtons));
	return FGSTN_ButtonMap(Tags);
}

#if WITH_EDITOR
EDataValidationResult UGSTN_CombatInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Buttons.Num() > MaxButtons)
	{
		Context.AddError(FText::Format(LOCTEXT("TooManyButtons", "{0} buttons, the input masks hold {1}."), Buttons.Num(), MaxButtons));
		Result = EDataValidationResult::Invalid;
	}

	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		const FGSTN_CombatButtonDef& Button = Buttons[Index];
		if (!Button.Tag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("ButtonNoTag", "Button {0} has no tag."), Index));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (!Button.Action)
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
			if (Button.Action && Buttons[Other].Action == Button.Action)
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

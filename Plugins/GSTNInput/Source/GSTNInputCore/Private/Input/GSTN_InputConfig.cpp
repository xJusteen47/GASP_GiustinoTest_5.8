// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputConfig.h"
#include "InputAction.h"
#include "EnhancedInputSubsystemInterface.h"
#include "InputMappingContext.h"
#include "UObject/Package.h"
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
	ResolvedContexts.Reset(MappingContexts.Num());
	ResolvedContextPriorities.Reset(MappingContexts.Num());
	for (const FGSTN_InputContextEntry& Entry : MappingContexts)
	{
		if (UInputMappingContext* Context = Entry.Context.IsNull() ? nullptr : Entry.Context.LoadSynchronous())
		{
			ResolvedContexts.Add(Context);
			ResolvedContextPriorities.Add(Entry.Priority);
		}
	}
	ResolvedReplacedContexts.Reset(ReplacedMappingContexts.Num());
	for (const TSoftObjectPtr<UInputMappingContext>& Replaced : ReplacedMappingContexts)
	{
		if (UInputMappingContext* Context = Replaced.IsNull() ? nullptr : Replaced.LoadSynchronous())
		{
			ResolvedReplacedContexts.Add(Context);
		}
	}
	// The automatic actions of the previous resolve, kept to be reused below: every pawn that runs this shared config resolves it
	// again (each spawn, each kit swap), and a new context each time piled up on the local players (dozens seen in a playtest)
	UInputMappingContext* const PreviousAutoContext = AutoMappingContext;
	UInputAction* const PreviousAutoStick = AimStickAction.IsNull() ? ResolvedAimStickAction.Get() : nullptr;
	UInputAction* const PreviousAutoToggle = ReticleToggleAction.IsNull() ? ResolvedReticleToggleAction.Get() : nullptr;
	ResolvedAimStickAction = AimStickAction.IsNull() ? nullptr : AimStickAction.LoadSynchronous();
	ResolvedReticleToggleAction = ReticleToggleAction.IsNull() ? nullptr : ReticleToggleAction.LoadSynchronous();

	// The automatic right stick actions (plan precisione-weakpoint 4-5): transient, never saved, rebuilt on every resolve so an asset edited
	// in the editor gets its own action back. Kept by this config's transient references
	AutoMappingContext = nullptr;
	if (!bAutoAimActions || (ResolvedAimStickAction && ResolvedReticleToggleAction))
	{
		return;
	}
	// Same keys as last time: the same context and actions (a player that already has the context keeps it, nothing is added again)
	if (PreviousAutoContext)
	{
		const bool bNeedStick = !ResolvedAimStickAction && AutoAimStickKey.IsValid();
		const bool bNeedToggle = !ResolvedReticleToggleAction && AutoReticleToggleKey.IsValid();
		int32 Matched = 0;
		for (const FEnhancedActionKeyMapping& Mapping : PreviousAutoContext->GetMappings())
		{
			const bool bStick = bNeedStick && Mapping.Action == PreviousAutoStick && Mapping.Key == AutoAimStickKey;
			const bool bToggle = bNeedToggle && Mapping.Action == PreviousAutoToggle && Mapping.Key == AutoReticleToggleKey;
			Matched += (bStick || bToggle) ? 1 : 0;
		}
		if (Matched > 0 && Matched == PreviousAutoContext->GetMappings().Num() && Matched == static_cast<int32>(bNeedStick) + static_cast<int32>(bNeedToggle))
		{
			ResolvedAimStickAction = bNeedStick ? PreviousAutoStick : ResolvedAimStickAction.Get();
			ResolvedReticleToggleAction = bNeedToggle ? PreviousAutoToggle : ResolvedReticleToggleAction.Get();
			AutoMappingContext = PreviousAutoContext;
			return;
		}
	}
	auto MakeAction = [](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UInputAction::StaticClass(), Name), RF_Transient);
		Action->ValueType = Type;
		// The camera's look action shares the right stick: this one must let it through
		Action->bConsumeInput = false;
		return Action;
	};
	UInputMappingContext* Context = NewObject<UInputMappingContext>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UInputMappingContext::StaticClass(), TEXT("IMC_GSTN_AutoAim")), RF_Transient);
	if (!ResolvedAimStickAction && AutoAimStickKey.IsValid())
	{
		ResolvedAimStickAction = MakeAction(TEXT("IA_GSTN_AutoAimStick"), EInputActionValueType::Axis2D);
		Context->MapKey(ResolvedAimStickAction, AutoAimStickKey);
	}
	if (!ResolvedReticleToggleAction && AutoReticleToggleKey.IsValid())
	{
		ResolvedReticleToggleAction = MakeAction(TEXT("IA_GSTN_AutoReticleToggle"), EInputActionValueType::Boolean);
		Context->MapKey(ResolvedReticleToggleAction, AutoReticleToggleKey);
	}
	AutoMappingContext = Context->GetMappings().Num() > 0 ? Context : nullptr;
}

void UGSTN_InputConfig::ApplyMappingContexts(IEnhancedInputSubsystemInterface& Input) const
{
	for (const UInputMappingContext* Replaced : ResolvedReplacedContexts)
	{
		// A config that lists the same context on both sides keeps it: its own list wins
		if (Replaced && Input.HasMappingContext(Replaced) && !ResolvedContexts.Contains(Replaced))
		{
			Input.RemoveMappingContext(Replaced);
		}
	}
	for (int32 Index = 0; Index < ResolvedContexts.Num(); ++Index)
	{
		int32 Priority = 0;
		const UInputMappingContext* Context = ResolvedContexts[Index];
		if (Context && (!Input.HasMappingContext(Context, Priority) || Priority != ResolvedContextPriorities[Index]))
		{
			// Adding an applied context again only moves it to the new priority
			Input.AddMappingContext(Context, ResolvedContextPriorities[Index]);
		}
	}
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

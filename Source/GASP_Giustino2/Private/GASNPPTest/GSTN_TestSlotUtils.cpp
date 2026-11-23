// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestSlotUtils.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_InputSlot.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"

bool UGSTN_TestSlotUtils::AddButtonPressSlot(UGSTN_InputSlotSet* Set, FGameplayTag Command, FGameplayTag Button)
{
	if (!Set || !Command.IsValid() || !Button.IsValid())
	{
		return false;
	}

	for (const FGSTN_InputSlotEntry& Entry : Set->Slots)
	{
		if (Entry.GetSpec().Command == Command)
		{
			return false;
		}
	}

	FGSTN_InputCondition_Press Press;
	Press.Buttons.AddTag(Button);

	FGSTN_InputStep Step;
	Step.AllOf.Add(TInstancedStruct<FGSTN_InputCondition>::Make<FGSTN_InputCondition_Press>(Press));

	FGSTN_InputSlotEntry Entry;
	Entry.Inline.Command = Command;
	Entry.Inline.Pattern.Steps.Add(MoveTemp(Step));

	Set->Modify();
	Set->Slots.Add(MoveTemp(Entry));
	Set->MarkPackageDirty();
	return true;
}

namespace
{
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(const APlayerController* Controller)
	{
		const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	}
}

bool UGSTN_TestSlotUtils::InjectInput(APlayerController* Controller, UInputAction* Action, FVector Value, bool bPulse)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(Controller);
	if (!Subsystem || !Action)
	{
		return false;
	}
	if (bPulse)
	{
		Subsystem->InjectInputForAction(Action, FInputActionValue(true), {}, {});
	}
	else
	{
		Subsystem->StartContinuousInputInjectionForAction(Action, Value, {}, {});
	}
	return true;
}

bool UGSTN_TestSlotUtils::StopInput(APlayerController* Controller, UInputAction* Action)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(Controller);
	if (!Subsystem || !Action)
	{
		return false;
	}
	Subsystem->StopContinuousInputInjectionForAction(Action);
	return true;
}

namespace
{
	FGSTN_InputStep MakePressStep(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Press;
		Press.Buttons.AddTag(Button);
		FGSTN_InputStep Step;
		Step.AllOf.Add(TInstancedStruct<FGSTN_InputCondition>::Make<FGSTN_InputCondition_Press>(Press));
		return Step;
	}
}

FGSTN_InputSlotEntry UGSTN_TestSlotUtils::MakeSlotEntry(FGameplayTag Command, const TArray<FGameplayTag>& Buttons, bool bChord, bool bFallThrough, int32 PriorityOffset)
{
	FGSTN_InputSlotEntry Entry;
	Entry.Inline.Command = Command;
	Entry.Inline.bFallThroughWhenUnavailable = bFallThrough;
	Entry.Inline.PriorityOffset = PriorityOffset;
	if (bChord)
	{
		FGSTN_InputCondition_Chord Chord;
		for (const FGameplayTag& Button : Buttons)
		{
			Chord.Buttons.AddTag(Button);
		}
		FGSTN_InputStep Step;
		Step.AllOf.Add(TInstancedStruct<FGSTN_InputCondition>::Make<FGSTN_InputCondition_Chord>(Chord));
		Entry.Inline.Pattern.Steps.Add(MoveTemp(Step));
	}
	else
	{
		Entry.Inline.Pattern = MakePressSequence(Buttons);
	}
	return Entry;
}

FGSTN_InputPattern UGSTN_TestSlotUtils::MakePressSequence(const TArray<FGameplayTag>& Buttons)
{
	FGSTN_InputPattern Pattern;
	for (const FGameplayTag& Button : Buttons)
	{
		Pattern.Steps.Add(MakePressStep(Button));
	}
	return Pattern;
}

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

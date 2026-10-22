// What an ability may ask the component that started it (GAS-NPP fork).

#include "Simulation/GSTN_CommandSourceInterface.h"

#include "Bindings/GSTN_CommandBindingSet.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_InputSimTypes.h"

FGSTN_ActivationContext FGSTN_ActivationContext::FromRequest(const FGSTN_InputSimState& State, const FGSTN_CommandTable& Table, int32 Frame)
{
	FGSTN_ActivationContext Context;

	const int32 BindingIndex = static_cast<int32>(State.RequestedBinding) - 1;
	if (State.RequestFrame != Frame || !Table.Bindings.IsValidIndex(BindingIndex))
	{
		return Context;
	}
	const FGSTN_CompiledBinding& Binding = Table.Bindings[BindingIndex];
	if (!Table.Commands.IsValidIndex(Binding.CommandIndex))
	{
		return Context;
	}

	Context.Command = Table.Commands[Binding.CommandIndex].Tag;
	Context.PressFrame = Frame;

	if (State.RequestButton == 0)
	{
		Context.Source = EGSTN_ActivationSource::Forced;
		return Context;
	}

	Context.Source = EGSTN_ActivationSource::Command;
	if (State.RequestButton == FGSTN_InputSimState::CommandWithoutButton)
	{
		return Context;
	}

	Context.Button = static_cast<int32>(State.RequestButton) - 1;
	const FGSTN_InputHistory& History = State.History;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		if (!Event.bRelease && Event.Button == Context.Button && Event.Frame <= Frame)
		{
			Context.PressFrame = Event.Frame;
			break;
		}
	}
	return Context;
}

IGSTN_CommandSourceInterface* IGSTN_CommandSourceInterface::Find(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (IGSTN_CommandSourceInterface* Source = Cast<IGSTN_CommandSourceInterface>(Component))
		{
			return Source;
		}
	}
	return nullptr;
}

// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "Sources/GSTN_CombatInputSource.h"

#include "GameFramework/Pawn.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_StandardSlots.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CommandSourceInterface.h"

FGSTN_CombatInputSource::FGSTN_CombatInputSource(UGSTN_CombatSimComponent& InComponent)
	: Component(&InComponent)
{
}

TSharedPtr<IGSTN_InputHistorySource> FGSTN_CombatInputSource::Find(APawn& Pawn)
{
	if (UGSTN_CombatSimComponent* Found = Pawn.FindComponentByClass<UGSTN_CombatSimComponent>())
	{
		return MakeShared<FGSTN_CombatInputSource>(*Found);
	}
	return nullptr;
}

TArray<FGameplayTag> FGSTN_CombatInputSource::GetCombatButtonTags()
{
	return { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag(),
		GSTNCombatButtonTags::Button_Crouch.GetTag(), GSTNCombatButtonTags::Button_Shield.GetTag(), GSTNCombatButtonTags::Button_Jump.GetTag() };
}

bool FGSTN_CombatInputSource::IsSourceValid() const
{
	const UGSTN_CombatSimComponent* Current = Component.Get();
	return Current && IsValid(Current) && Current->GetOwner() && Current->GetOwner()->HasActorBegunPlay();
}

const FGSTN_InputHistory* FGSTN_CombatInputSource::GetInputHistoryView() const
{
	const UGSTN_CombatSimComponent* Current = Component.Get();
	return Current ? Current->GetInputHistoryView() : nullptr;
}

bool FGSTN_CombatInputSource::GetLatestCommandRequest(int32& OutFrame, FGameplayTag& OutCommand) const
{
	const UGSTN_CombatSimComponent* Current = Component.Get();
	const FGSTN_CombatSyncState* Sync = Current ? Current->GetSyncState() : nullptr;
	if (!Sync)
	{
		return false;
	}
	const FGSTN_InputSimState& Input = Sync->Core.Input;
	if (Input.RequestedBinding == 0 || Input.RequestFrame == INDEX_NONE)
	{
		return false;
	}
	const FGSTN_ActivationContext Context = FGSTN_ActivationContext::FromRequest(Input, Current->GetCommandTable(), Input.RequestFrame);
	if (!Context.IsValid())
	{
		return false;
	}
	OutFrame = Input.RequestFrame;
	OutCommand = Context.Command;
	return true;
}

void FGSTN_CombatInputSource::GetButtonTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags = GetCombatButtonTags();
}

bool FGSTN_CombatInputSource::GetScriptSlots(TArray<FGSTN_InputSlotSpec>& OutSlots) const
{
	// The plugin's standard set: the kit of the character is data the debug module does not read yet
	OutSlots = UGSTN_StandardSlots::Build();
	return true;
}

FText FGSTN_CombatInputSource::GetSourceLabel() const
{
	return NSLOCTEXT("GSTNCombatDebug", "CombatSourceLabel", "Combat component, standard command set");
}

const UObject* FGSTN_CombatInputSource::GetSourceObject() const
{
	return Component.Get();
}

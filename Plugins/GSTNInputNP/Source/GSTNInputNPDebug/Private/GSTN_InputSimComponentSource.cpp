// Development tooling of GSTN Input NP (PIE and development builds, never Shipping).

#include "GSTN_InputSimComponentSource.h"

#include "GameFramework/Pawn.h"
#include "Input/GSTN_InputConfig.h"
#include "Simulation/GSTN_CommandSourceInterface.h"
#include "Simulation/GSTN_InputSimComponent.h"

FGSTN_InputSimComponentSource::FGSTN_InputSimComponentSource(UGSTN_InputSimComponent& InComponent)
	: Component(&InComponent)
{
}

TSharedPtr<IGSTN_InputHistorySource> FGSTN_InputSimComponentSource::Find(APawn& Pawn)
{
	if (UGSTN_InputSimComponent* Found = Pawn.FindComponentByClass<UGSTN_InputSimComponent>())
	{
		return MakeShared<FGSTN_InputSimComponentSource>(*Found);
	}
	return nullptr;
}

bool FGSTN_InputSimComponentSource::IsSourceValid() const
{
	const UGSTN_InputSimComponent* Current = Component.Get();
	return Current && IsValid(Current) && Current->GetOwner() && Current->GetOwner()->HasActorBegunPlay() && !Current->IsInert();
}

const FGSTN_InputHistory* FGSTN_InputSimComponentSource::GetInputHistoryView() const
{
	const UGSTN_InputSimComponent* Current = Component.Get();
	const FGSTN_InputModelSyncState* Sync = Current ? Current->GetSyncState() : nullptr;
	return Sync ? &Sync->Input.History : nullptr;
}

bool FGSTN_InputSimComponentSource::GetLatestCommandRequest(int32& OutFrame, FGameplayTag& OutCommand) const
{
	const UGSTN_InputSimComponent* Current = Component.Get();
	const FGSTN_InputModelSyncState* Sync = Current ? Current->GetSyncState() : nullptr;
	if (!Sync)
	{
		return false;
	}
	const FGSTN_InputSimState& Input = Sync->Input;
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

void FGSTN_InputSimComponentSource::GetButtonTags(TArray<FGameplayTag>& OutTags) const
{
	// The config the component loaded at BeginPlay; never resolved here, so the display cannot cause a load
	const UGSTN_InputSimComponent* Current = Component.Get();
	const UGSTN_InputConfig* Config = Current ? Current->InputConfig.Get() : nullptr;
	if (Config)
	{
		for (const FGSTN_InputButtonDef& Button : Config->Buttons)
		{
			OutTags.Add(Button.Tag);
		}
	}
}

FText FGSTN_InputSimComponentSource::GetSourceLabel() const
{
	return NSLOCTEXT("GSTNInputNPDebug", "InputSimSourceLabel", "Input component");
}

const UObject* FGSTN_InputSimComponentSource::GetSourceObject() const
{
	return Component.Get();
}

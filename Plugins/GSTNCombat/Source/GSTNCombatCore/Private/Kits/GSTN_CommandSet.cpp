// Pure combat logic (no GAS, no Network Prediction).

#include "Kits/GSTN_CommandSet.h"
#include "Input/GSTN_CombatSettings.h"

const UGSTN_InputSlotSet* FGSTN_CommandSet::ResolveBaseSet() const
{
	if (!BaseSet.IsNull())
	{
		return BaseSet.LoadSynchronous();
	}
	return UGSTN_CombatSettings::Get().DefaultCommandSet.LoadSynchronous();
}

void FGSTN_CommandSet::Compose(const UGSTN_InputSlotSet* Base, TArray<const FGSTN_InputSlotSpec*>& OutSpecs) const
{
	OutSpecs.Reset();
	if (Base)
	{
		for (const FGSTN_InputSlotEntry& Entry : Base->Slots)
		{
			const FGSTN_InputSlotSpec& Spec = Entry.GetSpec();
			if (!RemovedCommands.HasTagExact(Spec.Command))
			{
				OutSpecs.Add(&Spec);
			}
		}
	}

	for (const FGSTN_InputSlotEntry& Entry : AddedSlots)
	{
		const FGSTN_InputSlotSpec& Spec = Entry.GetSpec();
		const int32 Existing = OutSpecs.IndexOfByPredicate([&Spec](const FGSTN_InputSlotSpec* Other) { return Other->Command == Spec.Command; });
		if (Existing != INDEX_NONE && Spec.Command.IsValid())
		{
			OutSpecs[Existing] = &Spec;
		}
		else
		{
			OutSpecs.Add(&Spec);
		}
	}
}

void FGSTN_CommandSet::GetCommands(const UGSTN_InputSlotSet* Base, TArray<FGameplayTag>& OutCommands) const
{
	TArray<const FGSTN_InputSlotSpec*> Specs;
	Compose(Base, Specs);
	OutCommands.Reset();
	for (const FGSTN_InputSlotSpec* Spec : Specs)
	{
		OutCommands.Add(Spec->Command);
	}
}

TArray<FGameplayTag> UGSTN_CommandSetUtils::GetCommandSetCommands(const FGSTN_CommandSet& CommandSet)
{
	TArray<FGameplayTag> Commands;
	CommandSet.GetCommands(CommandSet.ResolveBaseSet(), Commands);
	return Commands;
}

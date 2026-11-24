// Mode verbs, Network Prediction half.

#include "GSTN_InteractSettings.h"
#include "GSTN_ModeVerbTags.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the interact meaning of a requestable prop (Interact.Target.Objective) in the interact settings, unless the project lists it already,
 * before any registry or world exists: the interact subsystem compiles its meaning table from these settings, and a record whose meaning is not in
 * the table projects no tag. Removed at shutdown only if this module added it.
 */
class FGSTNModeVerbsNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UGSTN_InteractSettings* Interact = GetMutableDefault<UGSTN_InteractSettings>();
		const FGameplayTag Objective = GSTNModeVerbTags::Interact_Target_Objective;
		if (!Interact->Meanings.ContainsByPredicate([&Objective](const FGSTN_InteractMeaning& Meaning) { return Meaning.Tag == Objective; }))
		{
			FGSTN_InteractMeaning& Meaning = Interact->Meanings.AddDefaulted_GetRef();
			Meaning.Tag = Objective;
			// Proposal order (interaction.md 2.3, INT.5): Grab 100 > Pickup 90 > Objective 80 > Equipment > Mount. A hold avoids accidental activation
			Meaning.DefaultPriority = 80;
			Meaning.HoldVerb = NSLOCTEXT("GSTNModeVerbs", "ObjectiveVerb", "Activate");
			// The prompt's hold bar divides by this; keep it equal to the Hold of Command.Mode.Interact in the slot set (a validator will compare them)
			Meaning.HoldFrames = 30;
			bAddedMeaning = true;
		}
	}

	virtual void ShutdownModule() override
	{
		if (bAddedMeaning && UObjectInitialized())
		{
			const FGameplayTag Objective = GSTNModeVerbTags::Interact_Target_Objective;
			GetMutableDefault<UGSTN_InteractSettings>()->Meanings.RemoveAll([&Objective](const FGSTN_InteractMeaning& Meaning) { return Meaning.Tag == Objective; });
		}
	}

private:
	bool bAddedMeaning = false;
};

IMPLEMENT_MODULE(FGSTNModeVerbsNPModule, GSTNModeVerbsNP);

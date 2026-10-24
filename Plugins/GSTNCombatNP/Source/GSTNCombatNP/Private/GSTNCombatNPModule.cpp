#include "Bindings/GSTN_CommandBindingSet.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Modules/ModuleManager.h"
#include "Simulation/GSTN_CombatTableSet.h"

#define LOCTEXT_NAMESPACE "GSTNCombatNP"

namespace
{
	const FName TableLimitsValidatorId(TEXT("GSTN.NP.TableLimits"));
}

/**
 * Registers the limits of the combat simulation's table as a loadout validator, so a loadout whose table the simulation could not hold
 * is refused before it reaches it (a binding index is a byte in the sync state's request, a registry index an int16).
 */
class FGSTNCombatNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UGSTN_LoadoutRules::RegisterValidator(TableLimitsValidatorId, &FGSTNCombatNPModule::ValidateTableLimits);
	}

	virtual void ShutdownModule() override
	{
		UGSTN_LoadoutRules::UnregisterValidator(TableLimitsValidatorId);
	}

	static FGSTN_LoadoutRuleResult ValidateTableLimits(const FGSTN_LoadoutRulesView& View)
	{
		FGSTN_LoadoutRuleResult Result;
		if (View.Table.EntryCount > FGSTN_CommandTable::MaxBindings)
		{
			Result.Errors.Add(FText::Format(LOCTEXT("TooManyBindings", "The loadout compiles to {0} bindings, the combat simulation holds at most {1}."),
				View.Table.EntryCount, FGSTN_CommandTable::MaxBindings));
		}
		if (View.Table.RegistryAbilities.Num() > FGSTN_CompiledBindingTable::MaxRegistryEntries)
		{
			Result.Errors.Add(FText::Format(LOCTEXT("TooManyTimelines", "The loadout needs {0} timeline abilities, the combat simulation holds at most {1}."),
				View.Table.RegistryAbilities.Num(), FGSTN_CompiledBindingTable::MaxRegistryEntries));
		}
		return Result;
	}
};

IMPLEMENT_MODULE(FGSTNCombatNPModule, GSTNCombatNP)

#undef LOCTEXT_NAMESPACE

// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatInputSettings.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatInputTags.h"

UGSTN_CombatInputSettings::UGSTN_CombatInputSettings()
{
	CategoryName = TEXT("Plugins");

	// One tier per family, in increasing precedence. Airborne roots mirror the grounded ones: a tag is looked up through
	// its ancestors only, and "Command.Air.Attack" is not under "Command.Attack".
	CommandTiers.Add(GSTNCommandTags::Attack, 100);
	CommandTiers.Add(GSTNCommandTags::Air_Attack, 100);
	CommandTiers.Add(GSTNCommandTags::Strong, 200);
	CommandTiers.Add(GSTNCommandTags::Air_Strong, 200);
	CommandTiers.Add(GSTNCommandTags::Special, 300);
	CommandTiers.Add(GSTNCommandTags::Air_Special, 300);
	CommandTiers.Add(GSTNCommandTags::Combo, 350);
	CommandTiers.Add(GSTNCommandTags::Super, 400);

	ExclusiveButtons.Add(GSTNInputTags::Button_Attack);
	ExclusiveButtons.Add(GSTNInputTags::Button_Strong);
	ExclusiveButtons.Add(GSTNInputTags::Button_Special);
}

int32 UGSTN_CombatInputSettings::GetCommandTier(const FGameplayTag& Command) const
{
	for (FGameplayTag Tag = Command; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const int32* Tier = CommandTiers.Find(Tag))
		{
			return *Tier;
		}
	}
	return 0;
}

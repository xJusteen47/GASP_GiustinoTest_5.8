// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatTickExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatTickExtension, Log, All);

FGSTN_CombatTickExtensionRegistry& FGSTN_CombatTickExtensionRegistry::Get()
{
	static FGSTN_CombatTickExtensionRegistry Registry;
	return Registry;
}

bool FGSTN_CombatTickExtensionRegistry::Register(IGSTN_CombatTickExtension* Extension)
{
	if (!Extension)
	{
		return false;
	}

	FEntry NewEntry;
	NewEntry.Priority = Extension->GetPriority();
	NewEntry.TypeId = Extension->GetTypeId();
	NewEntry.Extension = Extension;

	int32 InsertAt = 0;
	for (const FEntry& Entry : Entries)
	{
		if (Entry.TypeId == NewEntry.TypeId)
		{
			UE_LOG(LogGSTNCombatTickExtension, Warning, TEXT("A tick extension with type id %d is already registered, the new one is ignored"), NewEntry.TypeId);
			return false;
		}
		if (Entry.Priority < NewEntry.Priority || (Entry.Priority == NewEntry.Priority && Entry.TypeId < NewEntry.TypeId))
		{
			++InsertAt;
		}
	}

	Entries.Insert(NewEntry, InsertAt);
	return true;
}

void FGSTN_CombatTickExtensionRegistry::Unregister(IGSTN_CombatTickExtension* Extension)
{
	Entries.RemoveAll([Extension](const FEntry& Entry) { return Entry.Extension == Extension; });
}

void FGSTN_CombatTickExtensionRegistry::Run(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) const
{
	for (const FEntry& Entry : Entries)
	{
		Entry.Extension->OnCombatHook(Point, Context);
	}
}

bool FGSTN_CombatTickExtensionRegistry::BlocksActions(const FGSTN_CombatTickContext& Context) const
{
	bool bBlocked = false;
	for (const FEntry& Entry : Entries)
	{
		bBlocked |= Entry.Extension->BlocksActions(Context);
	}
	return bBlocked;
}

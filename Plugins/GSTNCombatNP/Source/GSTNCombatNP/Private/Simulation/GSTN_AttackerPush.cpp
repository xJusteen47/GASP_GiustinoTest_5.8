// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_AttackerPush.h"

FGSTN_AttackerPushRegistry& FGSTN_AttackerPushRegistry::Get()
{
	static FGSTN_AttackerPushRegistry Registry;
	return Registry;
}

bool FGSTN_AttackerPushRegistry::Register(IGSTN_AttackerPushExtension* Extension)
{
	if (!Extension)
	{
		return false;
	}
	const uint8 TypeId = Extension->GetTypeId();
	for (const IGSTN_AttackerPushExtension* Entry : Entries)
	{
		if (Entry == Extension || Entry->GetTypeId() == TypeId)
		{
			return false;
		}
	}
	Entries.Add(Extension);
	// Type id order, whatever the module load order: every machine asks in the same order
	Entries.Sort([](const IGSTN_AttackerPushExtension& A, const IGSTN_AttackerPushExtension& B)
	{
		return A.GetTypeId() < B.GetTypeId();
	});
	return true;
}

void FGSTN_AttackerPushRegistry::Unregister(IGSTN_AttackerPushExtension* Extension)
{
	Entries.Remove(Extension);
}

bool FGSTN_AttackerPushRegistry::GetPush(const FGSTN_AttackerPushQuery& Query, FVector& OutVelocity, int32& OutFrames) const
{
	for (const IGSTN_AttackerPushExtension* Entry : Entries)
	{
		if (Entry->GetPush(Query, OutVelocity, OutFrames))
		{
			return true;
		}
	}
	return false;
}

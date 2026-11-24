#include "GSTN_ExperienceGateLibrary.h"
#include "GSTN_ExperienceManagerComponent.h"

bool UGSTN_ExperienceGateLibrary::IsSimulationAllowed(const UObject* WorldContextObject)
{
	const UGSTN_ExperienceManagerComponent* Manager = UGSTN_ExperienceManagerComponent::Find(WorldContextObject);
	return !Manager || Manager->IsSimulationOpen();
}

bool UGSTN_ExperienceGateLibrary::CanSpawnPawns(const UObject* WorldContextObject, bool bRequireExperience)
{
	const UGSTN_ExperienceManagerComponent* Manager = UGSTN_ExperienceManagerComponent::Find(WorldContextObject);
	if (bRequireExperience)
	{
		return Manager && Manager->IsExperienceLoaded();
	}
	return !Manager || Manager->IsSimulationOpen();
}

bool UGSTN_ExperienceGateLibrary::TryBeginSimTick(const UObject* WorldContextObject)
{
	UGSTN_ExperienceManagerComponent* Manager = UGSTN_ExperienceManagerComponent::Find(WorldContextObject);
	return !Manager || Manager->TryBeginSimTick();
}

void UGSTN_ExperienceGateLibrary::NoteSimTickRan(const UObject* WorldContextObject)
{
	if (UGSTN_ExperienceManagerComponent* Manager = UGSTN_ExperienceManagerComponent::Find(WorldContextObject))
	{
		Manager->NoteSimTickRan();
	}
}

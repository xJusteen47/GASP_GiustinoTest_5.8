#include "GSTN_ExperienceGateComponent.h"
#include "GSTN_ExperienceManagerComponent.h"

void UGSTN_ExperienceGateComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToManager(UGSTN_ExperienceManagerComponent::Find(this));
}

void UGSTN_ExperienceGateComponent::BindToManager(UGSTN_ExperienceManagerComponent* Manager)
{
	BoundManager = Manager;
	if (!Manager || Manager->IsSimulationOpen())
	{
		bHeld = false;
		return;
	}

	bHeld = true;
	Manager->RegisterGatedSimulation(this);
}

void UGSTN_ExperienceGateComponent::GSTN_OnExperienceGateOpened(const UGSTN_ExperienceDefinition* Experience)
{
	bHeld = false;
	OnGateOpenedNative.Broadcast(Experience);
	OnGateOpened.Broadcast(Experience);
}

bool UGSTN_ExperienceGateComponent::ShouldRunSimulationTick()
{
	UGSTN_ExperienceManagerComponent* Manager = BoundManager.Get();
	if (!Manager)
	{
		return !bHeld;
	}
	// The manager counts the refusal; the local flag covers the moment between "gate open" and our callback (never longer than one call stack).
	return Manager->TryBeginSimTick() && !bHeld;
}

void UGSTN_ExperienceGateComponent::ReportSimulationTickRan()
{
	if (UGSTN_ExperienceManagerComponent* Manager = BoundManager.Get())
	{
		Manager->NoteSimTickRan();
	}
}

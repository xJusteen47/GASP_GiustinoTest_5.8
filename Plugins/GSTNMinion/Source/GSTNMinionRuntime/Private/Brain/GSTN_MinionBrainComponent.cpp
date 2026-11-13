// Minion runtime (M8 G8): the designer handle of a minion's brain.

#include "Brain/GSTN_MinionBrainComponent.h"

#include "Brain/GSTN_MinionBrainSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Runtime/GSTN_MinionCombatComponent.h"

UGSTN_MinionBrainComponent::UGSTN_MinionBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UGSTN_MinionBrainComponent::StartBrain()
{
	const AActor* Owner = GetOwner();
	const UGSTN_MinionCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_MinionCombatComponent>() : nullptr;
	UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(GetWorld());
	if (!Combat || !Brain)
	{
		return false;
	}
	const FGSTN_MinionHandle Handle = Combat->GetHandle();
	if (!BrainOverride.IsNull())
	{
		return Brain->StartBrain(Handle, BrainOverride.LoadSynchronous());
	}
	// No override: the type's brain, or the native one for a type that names none
	return Brain->StartBrainForDefinition(Handle) || Brain->StartBrain(Handle, nullptr);
}

bool UGSTN_MinionBrainComponent::StopBrain()
{
	const AActor* Owner = GetOwner();
	const UGSTN_MinionCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_MinionCombatComponent>() : nullptr;
	UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(GetWorld());
	return Combat && Brain && Brain->StopBrain(Combat->GetHandle());
}

bool UGSTN_MinionBrainComponent::IsBrainRunning() const
{
	const AActor* Owner = GetOwner();
	const UGSTN_MinionCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_MinionCombatComponent>() : nullptr;
	const UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(GetWorld());
	return Combat && Brain && Brain->HasBrain(Combat->GetHandle());
}

// Minion runtime (authority batch, M8 G3).

#include "Runtime/GSTN_MinionCombatComponent.h"

#include "Runtime/GSTN_MinionCombatSubsystem.h"

UGSTN_MinionCombatComponent::UGSTN_MinionCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGSTN_MinionCombatComponent::SetIntent(const FGSTN_MinionIntent& Intent)
{
	if (UGSTN_MinionCombatSubsystem* Owner = Subsystem.Get())
	{
		Owner->SetIntent(Handle, Intent);
	}
}

void UGSTN_MinionCombatComponent::RequestCommand(FGameplayTag Command)
{
	if (UGSTN_MinionCombatSubsystem* Owner = Subsystem.Get())
	{
		Owner->RequestCommand(Handle, Command);
	}
}

float UGSTN_MinionCombatComponent::GetHealth() const
{
	const UGSTN_MinionCombatSubsystem* Owner = Subsystem.Get();
	return Owner ? Owner->GetHealth(Handle) : 0.f;
}

void UGSTN_MinionCombatComponent::BindToSlot(UGSTN_MinionCombatSubsystem* InSubsystem, const FGSTN_MinionHandle& InHandle)
{
	Subsystem = InSubsystem;
	Handle = InHandle;
}

void UGSTN_MinionCombatComponent::ResetForPool()
{
	Subsystem.Reset();
	Handle = FGSTN_MinionHandle();
	OnMinionHit.Clear();
	OnMinionDied.Clear();
	OnLODChanged.Clear();
}

// Minion runtime (authority batch, M8 G3).

#include "Spawn/GSTN_MinionSpawner.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"

namespace
{
	UGSTN_MinionCombatSubsystem* ActiveSubsystemOf(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		return UGSTN_MinionCombatSubsystem::GetActive(World);
	}
}

FGSTN_MinionHandle UGSTN_MinionSpawner::Spawn(const UObject* WorldContextObject, UGSTN_MinionDefinition* Definition, const FTransform& Transform, const FGSTN_MinionSpawnParams& Params)
{
	UGSTN_MinionCombatSubsystem* Subsystem = ActiveSubsystemOf(WorldContextObject);
	return Subsystem ? Subsystem->EnqueueSpawn(Definition, Transform, Params) : FGSTN_MinionHandle();
}

bool UGSTN_MinionSpawner::Despawn(const UObject* WorldContextObject, FGSTN_MinionHandle Handle)
{
	UGSTN_MinionCombatSubsystem* Subsystem = ActiveSubsystemOf(WorldContextObject);
	return Subsystem && Subsystem->Despawn(Handle);
}

bool UGSTN_MinionSpawner::IsMinionValid(const UObject* WorldContextObject, FGSTN_MinionHandle Handle)
{
	const UGSTN_MinionCombatSubsystem* Subsystem = ActiveSubsystemOf(WorldContextObject);
	return Subsystem && Subsystem->IsValidHandle(Handle);
}

AActor* UGSTN_MinionSpawner::Promote(const UObject* WorldContextObject, FGSTN_MinionHandle Handle, const FGSTN_MinionPromotionParams& Params)
{
	UGSTN_MinionCombatSubsystem* Subsystem = ActiveSubsystemOf(WorldContextObject);
	return Subsystem ? Subsystem->PromoteToFighter(Handle, Params) : nullptr;
}

bool UGSTN_MinionSpawner::IsMinionAlive(const UObject* WorldContextObject, FGSTN_MinionHandle Handle)
{
	const UGSTN_MinionCombatSubsystem* Subsystem = ActiveSubsystemOf(WorldContextObject);
	return Subsystem && Subsystem->IsAlive(Handle);
}

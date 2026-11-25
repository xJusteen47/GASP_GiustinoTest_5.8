// Rollback plan F0 spike: spawns the world sim entity.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_WorldSimSubsystem.generated.h"

class AGSTN_WorldSimActor;

/**
 * Spawns one AGSTN_WorldSimActor on the authority of a game world when gstn.WorldSim.Spawn is 1 at begin play (clients get it by
 * replication). Off by default: the spike never runs in a normal session.
 */
UCLASS()
class GSTNWORLDSIM_API UGSTN_WorldSimSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	AGSTN_WorldSimActor* GetWorldSimActor() const { return WorldSimActor.Get(); }

private:
	UPROPERTY(Transient)
	TObjectPtr<AGSTN_WorldSimActor> WorldSimActor;
};

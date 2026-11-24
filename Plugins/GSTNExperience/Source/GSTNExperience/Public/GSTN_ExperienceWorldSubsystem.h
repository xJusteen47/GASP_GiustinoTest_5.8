#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_ExperienceWorldSubsystem.generated.h"

class AGameStateBase;
struct FComponentRequestHandle;

/**
 * Makes the GameState a target for Game Feature actions and gives it the Experience manager, on the host and on every client
 * (spike G4, the "GameState component action" half; modularity-and-tags.md S-K).
 *
 * `UGameFeatureAction_AddComponents` only reaches actors that registered with the Game Framework component manager as receivers.
 * Modular base classes do that themselves; this project's GameState is a plain AGameStateBase, so this subsystem registers it the
 * moment it spawns (host: created by the game mode; client: created by replication) and then asks the manager to add
 * UGSTN_ExperienceManagerComponent to it. Both machines add the component locally under the same name, which is how replicated
 * components added by Game Feature actions work.
 *
 * Game worlds only (game, PIE). Both steps can be turned off in UGSTN_ExperienceSettings.
 */
UCLASS()
class GSTNEXPERIENCE_API UGSTN_ExperienceWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void PostInitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void HandleActorSpawned(AActor* Actor);
	void RegisterGameState(AGameStateBase* GameState);

	TSharedPtr<FComponentRequestHandle> ManagerRequest;
	TSet<TWeakObjectPtr<AActor>> RegisteredReceivers;
	FDelegateHandle ActorSpawnedHandle;
};

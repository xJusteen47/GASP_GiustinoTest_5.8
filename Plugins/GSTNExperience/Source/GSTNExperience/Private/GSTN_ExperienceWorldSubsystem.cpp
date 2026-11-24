#include "GSTN_ExperienceWorldSubsystem.h"
#include "GSTN_ExperienceManagerComponent.h"
#include "GSTN_ExperienceSettings.h"
#include "GSTN_ExperienceTypes.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

bool UGSTN_ExperienceWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UGSTN_ExperienceWorldSubsystem::PostInitialize()
{
	Super::PostInitialize();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UGSTN_ExperienceSettings* Settings = GetDefault<UGSTN_ExperienceSettings>();
	if (Settings->bAddManagerToGameState)
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UGameFrameworkComponentManager* Manager = GameInstance->GetSubsystem<UGameFrameworkComponentManager>())
			{
				ManagerRequest = Manager->AddComponentRequest(
					TSoftClassPtr<AActor>(AGameStateBase::StaticClass()), UGSTN_ExperienceManagerComponent::StaticClass());
			}
		}
		if (!ManagerRequest.IsValid())
		{
			UE_LOG(LogGSTNExperience, Warning, TEXT("Experience: no Game Framework component manager, the GameState gets no Experience manager."));
		}
	}

	// The game mode spawns the GameState after world init, a client receives it later by replication: catch both.
	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));
}

void UGSTN_ExperienceWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Covers a GameState that already existed before the handler was bound (late subsystem creation, travel).
	if (AGameStateBase* GameState = InWorld.GetGameState())
	{
		RegisterGameState(GameState);
	}
}

void UGSTN_ExperienceWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	ActorSpawnedHandle.Reset();

	for (const TWeakObjectPtr<AActor>& Weak : RegisteredReceivers)
	{
		if (AActor* Actor = Weak.Get())
		{
			UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(Actor);
		}
	}
	RegisteredReceivers.Reset();
	ManagerRequest.Reset(); // releasing the handle removes the request

	Super::Deinitialize();
}

void UGSTN_ExperienceWorldSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (AGameStateBase* GameState = Cast<AGameStateBase>(Actor))
	{
		RegisterGameState(GameState);
	}
}

void UGSTN_ExperienceWorldSubsystem::RegisterGameState(AGameStateBase* GameState)
{
	if (!GetDefault<UGSTN_ExperienceSettings>()->bRegisterGameStateAsReceiver)
	{
		return;
	}
	if (RegisteredReceivers.Contains(GameState))
	{
		return;
	}
	RegisteredReceivers.Add(GameState);
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(GameState);
}

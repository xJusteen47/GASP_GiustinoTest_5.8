// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "GSTN_CombatRulesProvider.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GSTN_CombatRulesComponent.h"
#include "Input/GSTN_CombatSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatRulesProvider, Log, All);

void UGSTN_CombatRulesProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGameFrameworkComponentManager>();

	if (!UGSTN_CombatSettings::Get().bAutoAddRulesComponent)
	{
		return;
	}

	if (UGameFrameworkComponentManager* Manager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GetGameInstance()))
	{
		// AddUnique: a GameState Blueprint that carries its own rules component keeps it
		Request = Manager->AddComponentRequest(AGameStateBase::StaticClass(), UGSTN_CombatRulesComponent::StaticClass(), EGameFrameworkAddComponentFlags::AddUnique);
	}
	WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UGSTN_CombatRulesProvider::OnWorldInitialised);
}

void UGSTN_CombatRulesProvider::Deinitialize()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
	for (const TPair<TWeakObjectPtr<UWorld>, FDelegateHandle>& Pair : WorldHandles)
	{
		if (UWorld* World = Pair.Key.Get())
		{
			World->GameStateSetEvent.Remove(Pair.Value);
		}
	}
	WorldHandles.Reset();
	Request.Reset();
	Super::Deinitialize();
}

void UGSTN_CombatRulesProvider::OnWorldInitialised(UWorld* World, const UWorld::InitializationValues InitValues)
{
	if (World == nullptr || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance() || WorldHandles.Contains(World))
	{
		return;
	}
	WorldHandles.Add(World, World->GameStateSetEvent.AddUObject(this, &UGSTN_CombatRulesProvider::OnGameStateSet));
}

void UGSTN_CombatRulesProvider::OnGameStateSet(AGameStateBase* GameState)
{
	// The manager creates a replicated component on the authority only; a client's copy comes by replication
	if (GameState != nullptr && GameState->HasAuthority())
	{
		UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(GameState);
		UE_LOG(LogGSTNCombatRulesProvider, Verbose, TEXT("Registered %s for the combat rules component"), *GameState->GetName());
	}
}

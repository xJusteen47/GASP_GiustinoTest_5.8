#include "GSTN_ExperienceManagerComponent.h"
#include "GSTN_ExperienceDefinition.h"
#include "GSTN_ExperienceGatedSimulation.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFeatureAction.h"
#include "Net/UnrealNetwork.h"

UGSTN_ExperienceManagerComponent::UGSTN_ExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UGSTN_ExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTN_ExperienceManagerComponent, CurrentExperience);
}

void UGSTN_ExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownExperience();
	Super::EndPlay(EndPlayReason);
}

FGSTN_ExperienceLoadBackend& UGSTN_ExperienceManagerComponent::GetBackend()
{
	if (!Backend.IsValid())
	{
		Backend = MakeShared<FGSTN_DefaultExperienceLoadBackend>();
	}
	return *Backend;
}

void UGSTN_ExperienceManagerComponent::SetLoadBackend(TSharedPtr<FGSTN_ExperienceLoadBackend> InBackend)
{
	ensureMsgf(LoadState == EGSTN_ExperienceLoadState::Unloaded, TEXT("SetLoadBackend after the load started."));
	Backend = MoveTemp(InBackend);
}

void UGSTN_ExperienceManagerComponent::SetCurrentExperience(FPrimaryAssetId ExperienceId)
{
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(ExperienceId);
	UObject* Loaded = Path.IsValid() ? Path.TryLoad() : nullptr;

	// A data asset, or a Blueprint class derived from the definition (its defaults are the data), both work.
	const UGSTN_ExperienceDefinition* Definition = Cast<UGSTN_ExperienceDefinition>(Loaded);
	if (!Definition)
	{
		if (const UClass* AsClass = Cast<UClass>(Loaded))
		{
			if (AsClass->IsChildOf(UGSTN_ExperienceDefinition::StaticClass()))
			{
				Definition = GetDefault<UGSTN_ExperienceDefinition>(AsClass);
			}
		}
	}

	if (!Definition)
	{
		UE_LOG(LogGSTNExperience, Error, TEXT("Experience: '%s' did not resolve to a UGSTN_ExperienceDefinition."), *ExperienceId.ToString());
		return;
	}
	SetCurrentExperienceDefinition(Definition);
}

void UGSTN_ExperienceManagerComponent::SetCurrentExperienceDefinition(const UGSTN_ExperienceDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		UE_LOG(LogGSTNExperience, Warning, TEXT("Experience: only the host sets the Experience; a client receives it by replication."));
		return;
	}
	if (CurrentExperience)
	{
		UE_LOG(LogGSTNExperience, Warning, TEXT("Experience: '%s' ignored, '%s' is already set (one Experience per world)."),
			*GetNameSafe(Definition), *GetNameSafe(CurrentExperience.Get()));
		return;
	}

	CurrentExperience = Definition;
	StartExperienceLoad();
}

void UGSTN_ExperienceManagerComponent::OnRep_CurrentExperience()
{
	if (CurrentExperience && LoadState == EGSTN_ExperienceLoadState::Unloaded)
	{
		StartExperienceLoad();
	}
}

void UGSTN_ExperienceManagerComponent::SetLoadState(EGSTN_ExperienceLoadState NewState)
{
	LoadState = NewState;
	UE_LOG(LogGSTNExperience, Verbose, TEXT("Experience '%s': %d"), *GetNameSafe(CurrentExperience.Get()), static_cast<int32>(NewState));
	LoadStateChanged.Broadcast(NewState);
}

void UGSTN_ExperienceManagerComponent::StartExperienceLoad()
{
	check(CurrentExperience);
	check(LoadState == EGSTN_ExperienceLoadState::Unloaded);

	SetLoadState(EGSTN_ExperienceLoadState::Loading);
	const int32 Generation = ++LoadGeneration;
	FailedGameFeaturePlugins.Reset();

	TArray<FPrimaryAssetId> Assets;
	CurrentExperience->CollectPrimaryAssetIds(Assets);

	// A client never loads server-only data and a dedicated server never loads client-only data.
	const AActor* Owner = GetOwner();
	const ENetMode NetMode = Owner ? Owner->GetNetMode() : NM_Standalone;
	TArray<FName> Bundles;
	if (NetMode != NM_DedicatedServer)
	{
		Bundles.Add(TEXT("Client"));
	}
	if (NetMode != NM_Client)
	{
		Bundles.Add(TEXT("Server"));
	}

	TWeakObjectPtr<UGSTN_ExperienceManagerComponent> WeakThis(this);
	GetBackend().LoadBundles(Assets, Bundles, [WeakThis, Generation]()
	{
		if (UGSTN_ExperienceManagerComponent* Self = WeakThis.Get())
		{
			Self->OnBundlesLoaded(Generation);
		}
	});
}

void UGSTN_ExperienceManagerComponent::OnBundlesLoaded(int32 Generation)
{
	if (Generation != LoadGeneration || LoadState != EGSTN_ExperienceLoadState::Loading)
	{
		return;
	}

	SetLoadState(EGSTN_ExperienceLoadState::LoadingGameFeatures);

	TArray<FString> Plugins;
	CurrentExperience->CollectGameFeatureNames(Plugins);
	if (Plugins.IsEmpty())
	{
		ExecuteActionsAndFinish(Generation);
		return;
	}

	// Count first, then start: a plugin that completes synchronously must not reach zero before the others began.
	PluginsPending = Plugins.Num();
	TWeakObjectPtr<UGSTN_ExperienceManagerComponent> WeakThis(this);
	for (const FString& PluginName : Plugins)
	{
		GetBackend().ActivateGameFeaturePlugin(PluginName, [WeakThis, Generation, PluginName](bool bSuccess)
		{
			if (UGSTN_ExperienceManagerComponent* Self = WeakThis.Get())
			{
				Self->OnGameFeaturePluginDone(Generation, PluginName, bSuccess);
			}
		});
	}
}

void UGSTN_ExperienceManagerComponent::OnGameFeaturePluginDone(int32 Generation, const FString& PluginName, bool bSuccess)
{
	if (Generation != LoadGeneration || LoadState != EGSTN_ExperienceLoadState::LoadingGameFeatures)
	{
		return;
	}

	if (!bSuccess)
	{
		// Lyra's choice, kept: the Experience goes on without the feature and the failure is in the log and in this list, because
		// holding every pawn back forever on one bad plugin would be harder to diagnose than a mode missing a feature.
		FailedGameFeaturePlugins.Add(PluginName);
	}

	if (--PluginsPending == 0)
	{
		ExecuteActionsAndFinish(Generation);
	}
}

void UGSTN_ExperienceManagerComponent::ExecuteActionsAndFinish(int32 Generation)
{
	SetLoadState(EGSTN_ExperienceLoadState::ExecutingActions);

	TArray<UGameFeatureAction*> Actions;
	CurrentExperience->CollectActions(Actions);
	// Recorded before they run, so a teardown triggered from inside an action still deactivates every one of them.
	ExecutedActions.Reset();
	ExecutedActions.Append(Actions);
	GetBackend().ExecuteActions(Actions, GetWorld());

	// An action may have asked for the Experience to end (world teardown); then there is nothing to finish.
	if (Generation != LoadGeneration)
	{
		return;
	}

	SetLoadState(EGSTN_ExperienceLoadState::Loaded);

	// Order matters: data listeners first (pawn data, ability sets applied), simulations released last.
	OnExperienceLoadedHighPriority.Broadcast(CurrentExperience);
	OnExperienceLoadedHighPriority.Clear();
	OnExperienceLoadedNormal.Broadcast(CurrentExperience);
	OnExperienceLoadedNormal.Clear();

	TArray<TWeakObjectPtr<UObject>> ToRelease = MoveTemp(GatedSimulations);
	GatedSimulations.Reset();
	for (const TWeakObjectPtr<UObject>& Weak : ToRelease)
	{
		if (IGSTN_ExperienceGatedSimulation* Sim = Cast<IGSTN_ExperienceGatedSimulation>(Weak.Get()))
		{
			Sim->GSTN_OnExperienceGateOpened(CurrentExperience);
		}
	}
}

void UGSTN_ExperienceManagerComponent::ShutdownExperience()
{
	if (LoadState == EGSTN_ExperienceLoadState::Unloaded && ExecutedActions.IsEmpty())
	{
		return;
	}

	++LoadGeneration; // every in-flight completion is stale from here on
	SetLoadState(EGSTN_ExperienceLoadState::Deactivating);

	if (!ExecutedActions.IsEmpty())
	{
		TArray<UGameFeatureAction*> Actions;
		for (UGameFeatureAction* Action : ExecutedActions)
		{
			Actions.Add(Action);
		}
		GetBackend().DeactivateActions(Actions, GetWorld());
		ExecutedActions.Reset();
	}

	OnExperienceLoadedHighPriority.Clear();
	OnExperienceLoadedNormal.Clear();
	GatedSimulations.Reset();
	PluginsPending = 0;
	SetLoadState(EGSTN_ExperienceLoadState::Unloaded);
}

void UGSTN_ExperienceManagerComponent::CallOrRegister_OnExperienceLoaded_HighPriority(FGSTN_OnExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoadedHighPriority.Add(MoveTemp(Delegate));
	}
}

void UGSTN_ExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FGSTN_OnExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoadedNormal.Add(MoveTemp(Delegate));
	}
}

bool UGSTN_ExperienceManagerComponent::IsSimulationOpen() const
{
	return FGSTN_ExperienceGate::IsOpen(CurrentExperience != nullptr, LoadState);
}

bool UGSTN_ExperienceManagerComponent::TryBeginSimTick()
{
	if (IsSimulationOpen())
	{
		return true;
	}
	++HeldSimTicks;
	return false;
}

void UGSTN_ExperienceManagerComponent::NoteSimTickRan()
{
	if (!IsSimulationOpen())
	{
		++LeakedSimTicks;
		UE_LOG(LogGSTNExperience, Warning, TEXT("Experience: a simulation tick ran before the Experience was Loaded (leak %d)."), LeakedSimTicks);
	}
}

void UGSTN_ExperienceManagerComponent::RegisterGatedSimulation(UObject* GatedSimulation)
{
	IGSTN_ExperienceGatedSimulation* Sim = Cast<IGSTN_ExperienceGatedSimulation>(GatedSimulation);
	if (!ensureMsgf(Sim, TEXT("RegisterGatedSimulation: '%s' does not implement IGSTN_ExperienceGatedSimulation."), *GetNameSafe(GatedSimulation)))
	{
		return;
	}

	if (IsSimulationOpen())
	{
		Sim->GSTN_OnExperienceGateOpened(CurrentExperience);
		return;
	}
	GatedSimulations.AddUnique(GatedSimulation);
}

UGSTN_ExperienceManagerComponent* UGSTN_ExperienceManagerComponent::Find(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UGSTN_ExperienceManagerComponent>() : nullptr;
}

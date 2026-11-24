#include "GSTN_ExperienceLoadBackend.h"
#include "GSTN_ExperienceTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"

namespace
{
	// The only place that resolves a plugin name to the URL the Game Features subsystem wants. If a newer engine renames this call,
	// this is the one line to change (see the handover: first compile of GSTNExperience).
	bool ResolvePluginURL(const FString& PluginName, FString& OutURL)
	{
		return UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, OutURL);
	}

	FGameFeatureActivatingContext MakeContext(UWorld* World)
	{
		FGameFeatureActivatingContext Context;
		if (World && GEngine)
		{
			if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
			{
				Context.SetRequiredWorldContextHandle(WorldContext->ContextHandle);
			}
		}
		return Context;
	}
}

void FGSTN_DefaultExperienceLoadBackend::LoadBundles(const TArray<FPrimaryAssetId>& Assets, const TArray<FName>& Bundles, TFunction<void()> OnDone)
{
	UAssetManager& AssetManager = UAssetManager::Get();

	// Assets the manager does not know (not under a scanned directory, or created at runtime) have nothing to load.
	TArray<FPrimaryAssetId> Known;
	for (const FPrimaryAssetId& Id : Assets)
	{
		if (Id.IsValid() && AssetManager.GetPrimaryAssetPath(Id).IsValid())
		{
			Known.Add(Id);
		}
	}

	if (Known.IsEmpty())
	{
		OnDone();
		return;
	}

	BundleHandle = AssetManager.ChangeBundleStateForPrimaryAssets(
		Known, Bundles, {}, /*bRemoveAllBundles=*/false, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);

	if (!BundleHandle.IsValid() || BundleHandle->HasLoadCompleted())
	{
		OnDone();
		return;
	}

	// A cancelled load still lets the Experience continue: the assets that failed are logged by the streamable manager, and a
	// stuck load would hold every pawn back forever.
	BundleHandle->BindCompleteDelegate(FStreamableDelegate::CreateLambda([OnDone]() { OnDone(); }));
	BundleHandle->BindCancelDelegate(FStreamableDelegate::CreateLambda([OnDone]() { OnDone(); }));
}

void FGSTN_DefaultExperienceLoadBackend::ActivateGameFeaturePlugin(const FString& PluginName, TFunction<void(bool bSuccess)> OnDone)
{
	FString PluginURL;
	if (!ResolvePluginURL(PluginName, PluginURL))
	{
		UE_LOG(LogGSTNExperience, Error, TEXT("Experience: Game Feature plugin '%s' is unknown (not found by name)."), *PluginName);
		OnDone(false);
		return;
	}

	UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL,
		FGameFeaturePluginLoadComplete::CreateLambda([OnDone, PluginName](const UE::GameFeatures::FResult& Result)
		{
			const bool bOk = !Result.HasError();
			if (!bOk)
			{
				UE_LOG(LogGSTNExperience, Error, TEXT("Experience: Game Feature plugin '%s' failed to load and activate."), *PluginName);
			}
			OnDone(bOk);
		}));
}

void FGSTN_DefaultExperienceLoadBackend::ExecuteActions(const TArray<UGameFeatureAction*>& Actions, UWorld* World)
{
	FGameFeatureActivatingContext Context = MakeContext(World);
	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Action->OnGameFeatureRegistering();
			Action->OnGameFeatureLoading();
			Action->OnGameFeatureActivating(Context);
		}
	}
}

void FGSTN_DefaultExperienceLoadBackend::DeactivateActions(const TArray<UGameFeatureAction*>& Actions, UWorld* World)
{
	FGameFeatureDeactivatingContext Context(TEXT("GSTNExperienceActionSet"), [](FStringView) {});
	if (World && GEngine)
	{
		if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
		{
			Context.SetRequiredWorldContextHandle(WorldContext->ContextHandle);
		}
	}
	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Action->OnGameFeatureDeactivating(Context);
			Action->OnGameFeatureUnregistering();
		}
	}
}

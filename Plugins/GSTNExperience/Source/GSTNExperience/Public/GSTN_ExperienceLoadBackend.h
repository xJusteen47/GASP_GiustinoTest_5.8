#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "UObject/PrimaryAssetId.h"

class UGameFeatureAction;
class UWorld;
struct FStreamableHandle;

/**
 * The four engine services an Experience load needs. UGSTN_ExperienceManagerComponent owns the order (bundles, then Game Feature
 * plugins, then actions, then Loaded) and talks to the engine only through this seam, so the order can be tested with a fake that
 * completes steps by hand, and the one engine call most likely to change between engine versions lives in one place.
 *
 * Completion callbacks may run synchronously (asset already loaded) or later; the component copes with both and ignores callbacks
 * that arrive after its load was cancelled.
 */
class GSTNEXPERIENCE_API FGSTN_ExperienceLoadBackend
{
public:
	virtual ~FGSTN_ExperienceLoadBackend() = default;

	/** Load the given bundles of the given primary assets. Assets the Asset Manager does not know are skipped. */
	virtual void LoadBundles(const TArray<FPrimaryAssetId>& Assets, const TArray<FName>& Bundles, TFunction<void()> OnDone) = 0;

	/** Load and activate one Game Feature plugin by plugin name. OnDone(false) on an unknown plugin or a failed load. */
	virtual void ActivateGameFeaturePlugin(const FString& PluginName, TFunction<void(bool bSuccess)> OnDone) = 0;

	/** Run the actions' register, load and activate steps, in order, for the given world. Synchronous. */
	virtual void ExecuteActions(const TArray<UGameFeatureAction*>& Actions, UWorld* World) = 0;

	/** Undo ExecuteActions. Synchronous. */
	virtual void DeactivateActions(const TArray<UGameFeatureAction*>& Actions, UWorld* World) = 0;
};

/** Asset Manager and Game Features subsystem. What the game uses. */
class GSTNEXPERIENCE_API FGSTN_DefaultExperienceLoadBackend : public FGSTN_ExperienceLoadBackend
{
public:
	virtual void LoadBundles(const TArray<FPrimaryAssetId>& Assets, const TArray<FName>& Bundles, TFunction<void()> OnDone) override;
	virtual void ActivateGameFeaturePlugin(const FString& PluginName, TFunction<void(bool bSuccess)> OnDone) override;
	virtual void ExecuteActions(const TArray<UGameFeatureAction*>& Actions, UWorld* World) override;
	virtual void DeactivateActions(const TArray<UGameFeatureAction*>& Actions, UWorld* World) override;

private:
	/** Kept so the bundle load is not dropped before it completes. */
	TSharedPtr<FStreamableHandle> BundleHandle;
};

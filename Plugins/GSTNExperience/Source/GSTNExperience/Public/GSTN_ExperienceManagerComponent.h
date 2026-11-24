#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "GSTN_ExperienceLoadBackend.h"
#include "GSTN_ExperienceTypes.h"
#include "GSTN_ExperienceManagerComponent.generated.h"

class UGameFeatureAction;
class UGSTN_ExperienceDefinition;

/** Fires once when the Experience is Loaded. Registered through CallOrRegister_OnExperienceLoaded, which also covers "already loaded". */
DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnExperienceLoaded, const UGSTN_ExperienceDefinition* /*Experience*/);

/** Fires on every load state change, after the state is set. For tests, debug UI and the harness. */
DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnExperienceLoadStateChanged, EGSTN_ExperienceLoadState /*NewState*/);

/**
 * Loads the match's Experience on the GameState, on the host and on every client (Lyra's ULyraExperienceManagerComponent).
 *
 * The host calls SetCurrentExperience(Definition) once, before it spawns any pawn. CurrentExperience replicates, so a client starts
 * the same load from OnRep. Both machines then run the same steps in the same order:
 *   Unloaded -> Loading (asset bundles) -> LoadingGameFeatures (plugins named by the Experience and its action sets)
 *            -> ExecutingActions (the Experience's actions, then each action set's) -> Loaded.
 * "Loaded" is the spec's "Active". Until then the simulation gate is closed (IsSimulationOpen), and when it opens the registered
 * IGSTN_ExperienceGatedSimulation objects are told.
 *
 * Added to the GameState by UGSTN_ExperienceWorldSubsystem through the Game Framework component manager, which is also the path
 * Game Feature "add components" actions use (spike G4: this must work on a plain GameState on host and clients).
 *
 * An Experience is set once per world: a new mode or level is a map change (game-modes.md 2.5).
 */
UCLASS(ClassGroup = "GSTN", meta = (DisplayName = "GSTN Experience Manager"))
class GSTNEXPERIENCE_API UGSTN_ExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UGSTN_ExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ UActorComponent
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent

	/** Host: resolve the asset through the Asset Manager and start the load. */
	void SetCurrentExperience(FPrimaryAssetId ExperienceId);

	/** Host: start the load of an already resolved definition. Ignored (with a warning) on a client and when one is already set. */
	void SetCurrentExperienceDefinition(const UGSTN_ExperienceDefinition* Definition);

	/** Stop an unfinished load, deactivate the actions that ran, back to Unloaded. Called by EndPlay; public for tests. */
	void ShutdownExperience();

	bool IsExperienceLoaded() const { return LoadState == EGSTN_ExperienceLoadState::Loaded; }
	EGSTN_ExperienceLoadState GetLoadState() const { return LoadState; }
	const UGSTN_ExperienceDefinition* GetCurrentExperience() const { return CurrentExperience; }

	/** Game Feature plugins of the Experience that failed to load or activate. The Experience still reaches Loaded: see the log. */
	const TArray<FString>& GetFailedGameFeaturePlugins() const { return FailedGameFeaturePlugins; }

	/**
	 * Call now if the Experience is already Loaded, otherwise when it becomes Loaded. High priority delegates run before normal
	 * ones; use it for code the normal listeners depend on (applying pawn data before anything grants abilities from it).
	 */
	void CallOrRegister_OnExperienceLoaded_HighPriority(FGSTN_OnExperienceLoaded::FDelegate&& Delegate);
	void CallOrRegister_OnExperienceLoaded(FGSTN_OnExperienceLoaded::FDelegate&& Delegate);

	FGSTN_OnExperienceLoadStateChanged& OnLoadStateChanged() { return LoadStateChanged; }

	// ---- Simulation gate (spike G4) ----

	/** True when nothing is assigned yet (classic GameMode) or the Experience is Loaded. */
	bool IsSimulationOpen() const;

	/**
	 * A gated simulation asks before every tick. Returns true when it may tick. A refusal is counted (GetHeldSimTicks), because
	 * a refusal is the gate doing its job and the harness wants to see it happen.
	 */
	bool TryBeginSimTick();

	/** A gated simulation reports a tick it ran. A tick that ran while the gate was closed is a leak (GetLeakedSimTicks): spike G4 expects zero. */
	void NoteSimTickRan();

	int32 GetHeldSimTicks() const { return HeldSimTicks; }
	int32 GetLeakedSimTicks() const { return LeakedSimTicks; }

	/**
	 * Register a gated simulation (it must implement IGSTN_ExperienceGatedSimulation). It is told when the gate opens; if the gate
	 * is already open it is told now. Held weakly.
	 */
	void RegisterGatedSimulation(UObject* GatedSimulation);

	/** The GameState's manager, or null (no world, no GameState, or the component was not added). */
	static UGSTN_ExperienceManagerComponent* Find(const UObject* WorldContextObject);

	/** Replace the engine services. Tests only; must be called before an Experience is set. */
	void SetLoadBackend(TSharedPtr<FGSTN_ExperienceLoadBackend> InBackend);

private:
	UFUNCTION()
	void OnRep_CurrentExperience();

	void StartExperienceLoad();
	void OnBundlesLoaded(int32 Generation);
	void OnGameFeaturePluginDone(int32 Generation, const FString& PluginName, bool bSuccess);
	void ExecuteActionsAndFinish(int32 Generation);
	void SetLoadState(EGSTN_ExperienceLoadState NewState);
	FGSTN_ExperienceLoadBackend& GetBackend();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperience)
	TObjectPtr<const UGSTN_ExperienceDefinition> CurrentExperience;

	EGSTN_ExperienceLoadState LoadState = EGSTN_ExperienceLoadState::Unloaded;

	/** Bumped whenever a load starts or is cancelled; late completions from an older load compare and drop out. */
	int32 LoadGeneration = 0;
	int32 PluginsPending = 0;

	TArray<FString> FailedGameFeaturePlugins;
	TArray<TObjectPtr<UGameFeatureAction>> ExecutedActions;

	FGSTN_OnExperienceLoaded OnExperienceLoadedHighPriority;
	FGSTN_OnExperienceLoaded OnExperienceLoadedNormal;
	FGSTN_OnExperienceLoadStateChanged LoadStateChanged;

	TArray<TWeakObjectPtr<UObject>> GatedSimulations;
	int32 HeldSimTicks = 0;
	int32 LeakedSimTicks = 0;

	TSharedPtr<FGSTN_ExperienceLoadBackend> Backend;
};

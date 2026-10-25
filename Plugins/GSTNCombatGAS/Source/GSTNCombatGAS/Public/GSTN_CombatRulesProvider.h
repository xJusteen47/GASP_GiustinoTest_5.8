// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GSTN_CombatRulesProvider.generated.h"

struct FComponentRequestHandle;
class AGameStateBase;

/**
 * Puts a UGSTN_CombatRulesComponent on the GameState of every game world without touching the GameState Blueprint (M4 H13a, owner
 * decision "option B"): a component request on the Game Framework Component Manager (the ModularGameplay plugin, the same mechanism a Game
 * Feature "add component" action uses) for AGameStateBase, which the manager fulfils on each registered receiver.
 *
 * A plain AGameStateBase does not register itself as a receiver (only AModularGameStateBase does), so this subsystem registers every
 * game state it sees: it listens to the game state of each world of its game instance and registers it on the authority. A replicated
 * component is created by the manager on the authority only; clients receive it through replication, as for any replicated component.
 * Which rules the component plays is its replicated RulesAsset; empty (what this provider leaves) resolves to the combat settings'
 * DefaultRules, so the settings stay the one place that names the asset.
 *
 * Off with UGSTN_CombatSettings::bAutoAddRulesComponent. Nothing here runs in the simulation.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_CombatRulesProvider : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void OnWorldInitialised(UWorld* World, const UWorld::InitializationValues InitValues);
	void OnGameStateSet(AGameStateBase* GameState);

	TSharedPtr<FComponentRequestHandle> Request;
	TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> WorldHandles;
	FDelegateHandle WorldInitHandle;
};

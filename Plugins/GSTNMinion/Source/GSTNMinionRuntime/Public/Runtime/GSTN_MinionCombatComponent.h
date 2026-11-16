// Minion runtime (authority batch, M8 G3).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Intent/GSTN_MinionIntent.h"
#include "LOD/GSTN_MinionLOD.h"
#include "Store/GSTN_MinionStore.h"
#include "GSTN_MinionCombatComponent.generated.h"

class UGSTN_MinionCombatSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnMinionHitSignature, float, Damage, int32, AttackerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnMinionDiedSignature, int32, KillerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnMinionLODChangedSignature, EGSTN_MinionLODTier, OldTier, EGSTN_MinionLODTier, NewTier);

/**
 * The designer-facing handle of one minion on its pooled AGSTN_MinionActor (combat-m8-minions.md 4). Authority only, no tick and no
 * per-frame state: every read and write goes to the subsystem's store through the handle, so the batch stays the single owner of
 * the minion's state and a Blueprint cannot fork it.
 *
 * The events are broadcast by the subsystem AFTER the batch's last stage of the frame (never from inside a stage), so a listener
 * may despawn, spawn or write intents without touching columns the batch is walking. They are server-side gameplay callbacks:
 * nothing replicates them.
 *
 * Pooled: the subsystem binds a handle on acquire and ResetForPool clears it and every bound event on release, so a listener of a
 * dead minion never hears the next occupant of the actor.
 */
UCLASS(ClassGroup = "GSTN", meta = (BlueprintSpawnableComponent))
class GSTNMINIONRUNTIME_API UGSTN_MinionCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_MinionCombatComponent();

	/** The minion this component speaks for; the null handle while pooled */
	UFUNCTION(BlueprintPure, Category = "Minion")
	FGSTN_MinionHandle GetHandle() const { return Handle; }

	/** Replaces the minion's intent (latest wins). The subsystem stamps it with the current server frame */
	UFUNCTION(BlueprintCallable, Category = "Minion")
	void SetIntent(const FGSTN_MinionIntent& Intent);

	/** Asks for a move by its Command.Minion.* tag, keeping the rest of the intent. Consumed when the move starts */
	UFUNCTION(BlueprintCallable, Category = "Minion")
	void RequestCommand(FGameplayTag Command);

	/** Current health from the store, 0 for a pooled or dead minion */
	UFUNCTION(BlueprintPure, Category = "Minion")
	float GetHealth() const;

	/** Hit delivered by the inbox drain (damage applied, after the batch of the frame) */
	UPROPERTY(BlueprintAssignable, Category = "Minion|Events")
	FGSTN_OnMinionHitSignature OnMinionHit;

	/** Health reached zero; the death timeline starts. KillerId is the pipeline id of the last attacker */
	UPROPERTY(BlueprintAssignable, Category = "Minion|Events")
	FGSTN_OnMinionDiedSignature OnMinionDied;

	/** The simulation tier changed (after hysteresis and dwell) */
	UPROPERTY(BlueprintAssignable, Category = "Minion|Events")
	FGSTN_OnMinionLODChangedSignature OnLODChanged;

	/** Called by the subsystem when the pooled actor is given to a slot */
	void BindToSlot(UGSTN_MinionCombatSubsystem* InSubsystem, const FGSTN_MinionHandle& InHandle);

	/** Called by the subsystem when the actor returns to the pool: forgets the handle and every listener */
	void ResetForPool();

private:
	UPROPERTY(Transient)
	FGSTN_MinionHandle Handle;

	TWeakObjectPtr<UGSTN_MinionCombatSubsystem> Subsystem;
};

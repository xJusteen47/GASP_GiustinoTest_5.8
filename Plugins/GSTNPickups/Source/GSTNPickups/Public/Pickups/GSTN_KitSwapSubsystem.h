// Equipment pickups (M11 G2). Moved here from GSTNCombatGAS in M11 L2 (class redirect in Config/DefaultGSTNPickups.ini).

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_KitSwapSubsystem.generated.h"

class AActor;
class AGSTN_EquipmentPickup;
class AGSTN_KitPickup;
class UGSTN_LoadoutComponent;
struct FGSTN_SimEvent;

/**
 * The kit swap with a kit on the ground (TH-1b, D1). Server only; nothing here runs inside the simulation.
 *
 * Trigger. The hold of interact is the command Command.Interact.PickUp (UGSTN_MovementSlots: a release after InteractHoldFrames). When it
 * executes, the input simulation writes Input.Event.CommandExecuted into its outbox, and the authority publishes it once (never again on a
 * re-simulation, never on a client) through UGSTN_SimEventSubsystem. This subsystem listens for that event: so the hold is decided by the
 * deterministic input layer, and the swap is an out-of-band server write, exactly like a loadout the player asks for from a menu
 * (UGSTN_LoadoutComponent::AcceptLoadout). The new kit reaches the combat simulation as a new LoadoutRevision, swapped at one frame boundary
 * on every machine (FGSTN_LoadoutSwapLogic): the kit change is predicted the way the existing loadout change is, no new rollback state.
 *
 * Requirement: the command only executes when the player's kit binds Command.Interact.PickUp to an ability (the binding is what turns a
 * resolved command into an executed one). A kit that binds it to its own pick-up move still swaps next to a kit on the ground; a kit with no
 * binding for it cannot swap.
 *
 * Swap. The nearest non-empty equipment pickup within its InteractRadius of the pawn is taken (AGSTN_EquipmentPickup::Take): its payload
 * builds the new loadout, the player's loadout component validates and applies it (the rules can refuse it: the player is told, nothing
 * changes), and the payload's leftover stays in the pickup (for a kit, the player's previous loadout). Stats, moves, weapons and skin all
 * follow the kit because they are the loadout.
 *
 * Kit drop on death (M12) is a rule of the game mode, not global: the mode calls DropKit when its rule says so. Nothing calls it here.
 */
UCLASS()
class GSTNPICKUPS_API UGSTN_KitSwapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_KitSwapSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * The nearest non-empty equipment pickup (a kit, a support weapon...) whose InteractRadius reaches Pawn's location, null if none. Pure query,
	 * any machine (a client may use it for a prompt); only the server's answer decides a swap.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Kit Pickup")
	AGSTN_EquipmentPickup* FindPickupFor(const AActor* Pawn) const;

	/**
	 * Server only. Swaps Pawn's loadout with the nearest pickup in reach. False (with the reason in OutErrors) when there is no pickup in reach,
	 * no loadout component for the pawn, or the rules refuse the pickup's kit; nothing changes then. The event listener calls this; a
	 * server-side flow (a scripted test, a bot) can too.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Kit Pickup")
	bool TrySwap(AActor* Pawn, TArray<FString>& OutErrors);

	/**
	 * Server only. The swap itself (AGSTN_EquipmentPickup::Take): for a kit, Component takes the pickup's loadout and the pickup takes the
	 * component's previous one (empty if the player had none). Refused when the pickup is empty or the rules refuse; nothing changes then.
	 */
	static bool SwapWithPickup(UGSTN_LoadoutComponent& Component, AGSTN_EquipmentPickup& Pickup, TArray<FString>& OutErrors);

	/**
	 * Server only. The hook of the kit drop on death (M12, a per-mode rule): spawns a pickup of PickupClass (the base class if null) at Where
	 * holding a copy of From's applied loadout. Whether the player keeps that loadout too is the mode's decision. Null when From has no loadout.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Kit Pickup")
	AGSTN_KitPickup* DropKit(UGSTN_LoadoutComponent* From, const FTransform& Where, TSubclassOf<AGSTN_KitPickup> PickupClass);

	/**
	 * The TH-1b trigger above (a hold of interact near a kit swaps the nearest one). A plugin that offers a better trigger turns it off for its
	 * world: GSTNHeld's kit swap (M11 K1, the framed kit, a longer hold) does when its setting is on, so one hold never swaps twice
	 */
	bool bSwapOnInteractHoldEvent = true;

	/** The player id the race rule sorts by (FGSTN_PickupClaim): the pawn's player state id, INDEX_NONE without one */
	static int32 GetRacePlayerId(const AActor* Pawn);

	/** The loadout component that owns Pawn's kit: on its PlayerState (the usual place), else on the pawn itself. Null if none */
	static UGSTN_LoadoutComponent* FindLoadoutComponent(const AActor* Pawn);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void HandleCommandExecuted(const FGSTN_SimEvent& Event);

	TWeakObjectPtr<UGSTN_SimEventSubsystem> EventSource;
	FGSTN_SimEventListenerHandle ListenerHandle;
};

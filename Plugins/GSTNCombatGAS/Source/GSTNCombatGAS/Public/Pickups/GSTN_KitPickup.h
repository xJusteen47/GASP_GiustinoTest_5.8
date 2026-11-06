// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "GSTN_KitPickup.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A kit lying on the ground (TH-1b, owner decision D1 "pedana in gioco"; the seed of M11 "kit changed by picking it up"). It holds a whole
 * loadout: a player who holds interact next to it swaps loadouts with it, so the player now plays the kit that was on the ground (its stats,
 * moves, weapons and skin) and their old kit stays here, in its place, for someone else to try. See UGSTN_KitSwapSubsystem for the flow.
 *
 * The contents are server state: only the authority writes Loadout, and it replicates so every machine shows what lies here. The pickup never
 * touches the simulation; the player's kit changes through the loadout component, which reaches the combat simulation the usual way (a new
 * LoadoutRevision, swapped at one frame boundary on every machine).
 *
 * An empty pickup (no kit) is hidden and cannot be swapped with: it is what remains when a player with no loadout takes the kit.
 */
UCLASS(ClassGroup = "GSTN Combat", Blueprintable)
class GSTNCOMBATGAS_API AGSTN_KitPickup : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_KitPickup();

	/** How close (from the pawn's location to this actor's) a player must be for the hold of interact to swap with this kit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Pickup", meta = (ClampMin = "10", ForceUnits = "cm"))
	float InteractRadius = 150.f;

	/** What lies here. Set in the level or by DropKit; written only by the authority afterwards */
	UFUNCTION(BlueprintPure, Category = "Kit Pickup")
	const FGSTN_LoadoutData& GetLoadout() const { return Loadout; }

	/** Server only: replaces what lies here (the swap and the drop hook call it). Refreshes the look on this machine */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Kit Pickup")
	void SetLoadout(const FGSTN_LoadoutData& NewLoadout);

	/** True when no kit lies here */
	UFUNCTION(BlueprintPure, Category = "Kit Pickup")
	bool IsEmpty() const { return !Loadout.PrimaryKit.IsValid(); }

	/** The contents changed on this machine (server: set; client: replicated). For a Blueprint look (an icon, a glow) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Kit Pickup")
	void OnKitChanged();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The kit's body on the ground; no mesh by default, a level script or a child Blueprint picks one */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kit Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The kit's name above it, so a tester reads what lies where */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kit Pickup")
	TObjectPtr<UTextRenderComponent> Label;

private:
	UFUNCTION()
	void OnRep_Loadout();

	/** Mesh and label follow the contents */
	void RefreshLook();

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Loadout, Category = "Kit Pickup")
	FGSTN_LoadoutData Loadout;
};

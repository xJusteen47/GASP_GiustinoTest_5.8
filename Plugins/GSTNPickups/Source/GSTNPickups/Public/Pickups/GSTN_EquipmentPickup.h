// Equipment pickups (M11 G2).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_EquipmentPickup.generated.h"

class UGSTN_LoadoutComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FGSTN_PickupPayload;

/**
 * Equipment lying in the world (combat-loadout-mods.md §12): one payload (an FGSTN_PickupPayload child: a kit, a support weapon, a plugin's own
 * kind). A designer makes a Blueprint child, picks the mesh and sets the payload in the details panel; nothing is wired by hand: GSTNHeld's
 * interact bridge gives every equipment pickup its interact record (focus on the framed one, hold of the grab button) when that plugin is on.
 *
 * Server-authoritative: only the authority writes the payload; it replicates so every machine shows what lies here. Taking it (Take) runs the
 * payload's MakeChange, lets the loadout component validate and apply the new loadout (a new LoadoutRevision, swapped at one frame boundary on
 * every machine), then leaves the payload's leftover here. The pickup itself never touches the simulation.
 *
 * An empty pickup is hidden and offered to no one.
 */
UCLASS(ClassGroup = "GSTN Pickups", Blueprintable)
class GSTNPICKUPS_API AGSTN_EquipmentPickup : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_EquipmentPickup();

	/** How close (from the pawn's location to this actor's) a player must be to take this when nothing is framed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "10", ForceUnits = "cm"))
	float InteractRadius = 150.f;

	UFUNCTION(BlueprintPure, Category = "Pickup")
	const FInstancedStruct& GetPayload() const { return Payload; }

	/** The payload as its base type, null when none is set */
	const FGSTN_PickupPayload* GetPayloadBase() const;

	/** Server only: replaces what lies here. Refreshes the look on this machine */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup")
	void SetPayload(const FInstancedStruct& NewPayload);

	/** True when nothing lies here */
	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool IsEmpty() const;

	/** The kit's or the weapon's id, for a label or an icon. Invalid when empty */
	UFUNCTION(BlueprintPure, Category = "Pickup")
	FPrimaryAssetId GetDisplayId() const;

	/**
	 * Server only. Component's player takes what lies here: the payload builds the new loadout, Component validates and applies it, and the
	 * payload's leftover stays here. False with the reasons when the payload or the rules refuse; nothing changes on either side then.
	 * The race between players (FGSTN_PickupClaim) is the caller's: call it once per pickup and frame, for the winner.
	 */
	bool Take(UGSTN_LoadoutComponent& Component, TArray<FString>& OutErrors);

	/** The contents changed on this machine (server: set; client: replicated). For a Blueprint look (an icon, a glow) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup", meta = (DisplayName = "On Payload Changed"))
	void K2_OnPayloadChanged();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Native hook after every change of the payload on this machine (the kit pickup forwards it to its own Blueprint event) */
	virtual void OnPayloadChanged() {}

	/** Mesh and label follow the contents */
	virtual void RefreshLook();

	/** The body on the ground; with no mesh of its own it shows the placeholder box coloured by the display id */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The name above it, so a tester reads what lies where */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UTextRenderComponent> Label;

	/** What lies here. Set in the details panel or by the server */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Payload, Category = "Pickup", meta = (BaseStruct = "/Script/GSTNPickups.GSTN_PickupPayload", ExcludeBaseStruct))
	FInstancedStruct Payload;

private:
	UFUNCTION()
	void OnRep_Payload();

	void NotifyPayloadChanged();
};

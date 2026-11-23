// Equipment pickups (M11 G2). Moved here from GSTNCombatGAS in M11 L2 (class redirect in Config/DefaultGSTNPickups.ini).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Pickups/GSTN_EquipmentPickup.h"
#include "GSTN_KitPickup.generated.h"

/**
 * A kit lying on the ground (TH-1b, owner decision D1 "pedana in gioco"): an equipment pickup whose payload is always a whole loadout
 * (FGSTN_KitPickupPayload). A player who takes it swaps loadouts with it, so they now play the kit that was on the ground (its stats, moves,
 * weapons and skin) and their old kit stays here, in its place, for someone else to try. See UGSTN_KitSwapSubsystem for the TH-1b trigger and
 * GSTNHeld's kit interact bridge for the framed-kit hold of the grab button.
 *
 * GetLoadout/SetLoadout are the kit view of the payload, kept for the callers TH-1b and K1 wrote (scripts, the drop hook, the test hub).
 */
UCLASS(ClassGroup = "GSTN Pickups", Blueprintable)
class GSTNPICKUPS_API AGSTN_KitPickup : public AGSTN_EquipmentPickup
{
	GENERATED_BODY()

public:
	AGSTN_KitPickup();

	/** What lies here; an empty loadout when the payload is not a kit */
	UFUNCTION(BlueprintPure, Category = "Kit Pickup")
	const FGSTN_LoadoutData& GetLoadout() const;

	/** Server only: puts NewLoadout here as a kit payload (an empty loadout empties the pickup) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Kit Pickup")
	void SetLoadout(const FGSTN_LoadoutData& NewLoadout);

	/** The contents changed on this machine (server: set; client: replicated). For a Blueprint look (an icon, a glow) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Kit Pickup")
	void OnKitChanged();

protected:
	virtual void OnPayloadChanged() override { OnKitChanged(); }
};

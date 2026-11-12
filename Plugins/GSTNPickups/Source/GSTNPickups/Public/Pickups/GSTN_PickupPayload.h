// Equipment pickups (M11 G2).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_PickupPayload.generated.h"

/**
 * What an equipment pickup holds (combat-loadout-mods.md §12). The base of the payload FInstancedStruct on AGSTN_EquipmentPickup; a plugin adds
 * its own kind by deriving from it (the backpack plugin registers FGSTN_BackpackPickupPayload; this plugin never names backpacks).
 *
 * A payload is pure data about the loadout: taking it is MakeChange, which builds the loadout the taker would get and what stays on the ground in
 * its place. The loadout component then validates and applies that loadout on the server, out of band, as a new LoadoutRevision (M3 §7.8); the
 * payload never touches the simulation and never decides the rules (the kit's AllowedAdditionalWeapons, MaxAdditionalWeapons and the mode's
 * loadout rules refuse what does not fit).
 */
USTRUCT(BlueprintType)
struct GSTNPICKUPS_API FGSTN_PickupPayload
{
	GENERATED_BODY()

	virtual ~FGSTN_PickupPayload() = default;

	/** Nothing to take: the pickup is hidden and offered to no one */
	virtual bool IsEmpty() const { return true; }

	/** The id the look and the label use (the kit's or the weapon's): the placeholder colour comes from its name */
	virtual FPrimaryAssetId GetDisplayId() const { return FPrimaryAssetId(); }

	/**
	 * Current is the taker's applied loadout. Fills OutNew with what the taker would play after taking this, and OutLeftBehind with what lies here
	 * afterwards (an invalid struct = nothing, the pickup empties). False with a reason when this payload cannot be taken by that loadout; the
	 * loadout rules still run afterwards and may refuse OutNew.
	 */
	virtual bool MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const
	{
		OutErrors.Add(TEXT("Pickup: nothing lies here"));
		return false;
	}
};

/**
 * A whole kit on the ground (TH-1b, D1): the taker plays this loadout (kit, weapons, supers, skin, extensions) and their previous loadout stays
 * here, so another player can try it. Which parts of the old loadout carry over to the new kit (the support weapon, meters, mods) is open
 * (spec §4, questions M11.2-M11.5): until the owner decides, the whole loadout is swapped, as K1 shipped it.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Kit"))
struct GSTNPICKUPS_API FGSTN_KitPickupPayload : public FGSTN_PickupPayload
{
	GENERATED_BODY()

	FGSTN_KitPickupPayload() = default;
	explicit FGSTN_KitPickupPayload(const FGSTN_LoadoutData& InLoadout) : Loadout(InLoadout) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	FGSTN_LoadoutData Loadout;

	virtual bool IsEmpty() const override { return !Loadout.PrimaryKit.IsValid(); }
	virtual FPrimaryAssetId GetDisplayId() const override { return Loadout.PrimaryKit; }
	virtual bool MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const override;
};

/**
 * A support weapon on the ground (spec §6, Helldivers style). One support weapon at a time: the taker's last additional weapon is replaced and
 * stays here in its place; with none, the weapon is added and the pickup empties. The kit's stats stay (a support weapon inherits them, §6).
 * Whether the kit accepts it at all is the loadout rules' call (AllowedAdditionalWeapons, MaxAdditionalWeapons).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Support weapon"))
struct GSTNPICKUPS_API FGSTN_SupportWeaponPickupPayload : public FGSTN_PickupPayload
{
	GENERATED_BODY()

	FGSTN_SupportWeaponPickupPayload() = default;
	explicit FGSTN_SupportWeaponPickupPayload(const FPrimaryAssetId& InWeapon) : Weapon(InWeapon) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (AllowedTypes = "GSTN_Weapon"))
	FPrimaryAssetId Weapon;

	virtual bool IsEmpty() const override { return !Weapon.IsValid(); }
	virtual FPrimaryAssetId GetDisplayId() const override { return Weapon; }
	virtual bool MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const override;
};

/**
 * The race rule for one pickup (spec §12, the same as M10's held objects): when several players ask for the same pickup, the earliest request
 * frame wins, then the lower player id; arrival order on the server never matters. A loser is refused, it never takes what the winner left.
 */
struct GSTNPICKUPS_API FGSTN_PickupClaim
{
	int32 Frame = INDEX_NONE;
	/** APlayerState::GetPlayerId; INDEX_NONE (no player state) sorts last */
	int32 PlayerId = INDEX_NONE;
	/** The caller's index for this claim (its queue position), carried through the sort */
	int32 Index = INDEX_NONE;

	/** Winner order: earliest frame, then lower player id, then queue position (only one request per player and frame reaches the queue) */
	static bool Precedes(const FGSTN_PickupClaim& A, const FGSTN_PickupClaim& B);
	static void SortForRace(TArray<FGSTN_PickupClaim>& Claims) { Claims.StableSort(&FGSTN_PickupClaim::Precedes); }
};

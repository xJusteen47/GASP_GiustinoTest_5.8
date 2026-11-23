// Equipment pickups (M11 G2).

#include "Pickups/GSTN_PickupPayload.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"

namespace
{
	/** The slot customisations of weapon WeaponIndex (0 = the kit's primary, 1.. = the additional weapons), taken out of Data */
	TArray<FGSTN_SlotCustomisation> TakeCustomisations(FGSTN_LoadoutData& Data, int32 WeaponIndex)
	{
		TArray<FGSTN_SlotCustomisation> Taken;
		Data.SlotCustomisations.RemoveAll([WeaponIndex, &Taken](const FGSTN_SlotCustomisation& Entry)
		{
			if (Entry.WeaponIndex != WeaponIndex)
			{
				return false;
			}
			Taken.Add(Entry);
			return true;
		});
		return Taken;
	}

	void PutCustomisations(FGSTN_LoadoutData& Data, TArray<FGSTN_SlotCustomisation> Entries, int32 WeaponIndex)
	{
		for (FGSTN_SlotCustomisation& Entry : Entries)
		{
			Entry.WeaponIndex = WeaponIndex;
			Data.SlotCustomisations.Add(MoveTemp(Entry));
		}
	}
}

bool FGSTN_KitPickupPayload::MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const
{
	return MakeChangeUnder(FGSTN_KitSwapCarryOver(), Current, OutNew, OutLeftBehind, OutErrors);
}

bool FGSTN_KitPickupPayload::MakeChangeUnder(const FGSTN_KitSwapCarryOver& Rules, const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const
{
	if (IsEmpty())
	{
		OutErrors.Add(TEXT("Kit pickup: no kit lies here"));
		return false;
	}
	OutNew = Loadout;
	// The previous loadout stays on the ground; a player with no kit leaves an empty pickup behind. What the mode's rules keep with
	// the player (the support weapon, the mods if the mode says so, a backpack) moves to the new loadout instead; this plugin never
	// learns which extension types those are
	FGSTN_LoadoutData Left = Current;
	if (Rules.bSupportWeaponStaysWithPlayer && Left.PrimaryKit.IsValid())
	{
		CarrySupportWeapon(OutNew, Left);
	}
	GSTNLoadoutExtension::CarryOverOnKitSwap(OutNew.Extensions, Left.Extensions, Rules);
	OutLeftBehind = FInstancedStruct::Make(FGSTN_KitPickupPayload(Left));
	return true;
}

void FGSTN_KitPickupPayload::CarrySupportWeapon(FGSTN_LoadoutData& Taken, FGSTN_LoadoutData& Left)
{
	if (Left.AdditionalWeapons.IsEmpty())
	{
		return;
	}
	const FPrimaryAssetId Carried = Left.AdditionalWeapons.Last();
	if (Taken.AdditionalWeapons.Contains(Carried))
	{
		return;
	}
	// Weapon index 0 is the kit's primary, so the last additional weapon is at index Num()
	const int32 LeftIndex = Left.AdditionalWeapons.Num();
	TArray<FGSTN_SlotCustomisation> CarriedCustomisations = TakeCustomisations(Left, LeftIndex);
	if (Taken.AdditionalWeapons.IsEmpty())
	{
		Taken.AdditionalWeapons.Add(Carried);
		Left.AdditionalWeapons.Pop();
		PutCustomisations(Taken, MoveTemp(CarriedCustomisations), Taken.AdditionalWeapons.Num());
		return;
	}
	// One support weapon at a time: the ground kit's own goes down with the old kit, in the carried one's place
	const int32 TakenIndex = Taken.AdditionalWeapons.Num();
	TArray<FGSTN_SlotCustomisation> GroundCustomisations = TakeCustomisations(Taken, TakenIndex);
	Left.AdditionalWeapons.Last() = Taken.AdditionalWeapons.Last();
	Taken.AdditionalWeapons.Last() = Carried;
	PutCustomisations(Taken, MoveTemp(CarriedCustomisations), TakenIndex);
	PutCustomisations(Left, MoveTemp(GroundCustomisations), LeftIndex);
}

bool FGSTN_SupportWeaponPickupPayload::MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const
{
	if (IsEmpty())
	{
		OutErrors.Add(TEXT("Support weapon pickup: no weapon lies here"));
		return false;
	}
	if (!Current.PrimaryKit.IsValid())
	{
		OutErrors.Add(TEXT("Support weapon pickup: a support weapon needs a kit to carry it"));
		return false;
	}
	if (Current.AdditionalWeapons.Contains(Weapon))
	{
		OutErrors.Add(FString::Printf(TEXT("Support weapon pickup: %s is carried already"), *Weapon.ToString()));
		return false;
	}
	OutNew = Current;
	OutLeftBehind.Reset();
	if (OutNew.AdditionalWeapons.IsEmpty())
	{
		OutNew.AdditionalWeapons.Add(Weapon);
	}
	else
	{
		// One at a time: the last one is swapped out and lies here instead
		FPrimaryAssetId& Replaced = OutNew.AdditionalWeapons.Last();
		OutLeftBehind = FInstancedStruct::Make(FGSTN_SupportWeaponPickupPayload(Replaced));
		Replaced = Weapon;
		// The player's customisations of the weapon that left do not fit the new one (weapon index 0 is the kit's primary)
		const int32 ReplacedIndex = OutNew.AdditionalWeapons.Num();
		OutNew.SlotCustomisations.RemoveAll([ReplacedIndex](const FGSTN_SlotCustomisation& Entry) { return Entry.WeaponIndex == ReplacedIndex; });
	}
	return true;
}

bool FGSTN_PickupClaim::Precedes(const FGSTN_PickupClaim& A, const FGSTN_PickupClaim& B)
{
	if (A.Frame != B.Frame)
	{
		return A.Frame < B.Frame;
	}
	// No player id sorts after every real one
	const uint32 IdA = static_cast<uint32>(A.PlayerId);
	const uint32 IdB = static_cast<uint32>(B.PlayerId);
	if (IdA != IdB)
	{
		return IdA < IdB;
	}
	return A.Index < B.Index;
}

// Equipment pickups (M11 G2).

#include "Pickups/GSTN_PickupPayload.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"

bool FGSTN_KitPickupPayload::MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const
{
	if (IsEmpty())
	{
		OutErrors.Add(TEXT("Kit pickup: no kit lies here"));
		return false;
	}
	OutNew = Loadout;
	// The previous loadout stays on the ground; a player with no kit leaves an empty pickup behind. What says it stays with the
	// player (a backpack, M11 L7) moves to the new loadout instead; this plugin never learns which types those are
	FGSTN_LoadoutData Left = Current;
	GSTNLoadoutExtension::CarryOverOnKitSwap(OutNew.Extensions, Left.Extensions);
	OutLeftBehind = FInstancedStruct::Make(FGSTN_KitPickupPayload(Left));
	return true;
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

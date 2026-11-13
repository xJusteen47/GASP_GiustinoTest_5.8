// Equipment pickups (M11 G2).

#include "Pickups/GSTN_KitPickup.h"

#include "Pickups/GSTN_PickupPayload.h"

AGSTN_KitPickup::AGSTN_KitPickup()
{
	// An empty kit payload, so the details panel of a placed kit shows the loadout fields at once
	Payload = FInstancedStruct::Make(FGSTN_KitPickupPayload());
}

const FGSTN_LoadoutData& AGSTN_KitPickup::GetLoadout() const
{
	static const FGSTN_LoadoutData None;
	const FGSTN_KitPickupPayload* Kit = Payload.GetPtr<FGSTN_KitPickupPayload>();
	return Kit ? Kit->Loadout : None;
}

void AGSTN_KitPickup::SetLoadout(const FGSTN_LoadoutData& NewLoadout)
{
	SetPayload(FInstancedStruct::Make(FGSTN_KitPickupPayload(NewLoadout)));
}

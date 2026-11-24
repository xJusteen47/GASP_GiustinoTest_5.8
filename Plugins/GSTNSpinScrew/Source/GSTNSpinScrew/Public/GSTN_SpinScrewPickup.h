// The screw once it is out: a held object (GSTNHeld) with the screw's greybox look.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_HeldObjectActor.h"

#include "GSTN_SpinScrewPickup.generated.h"

/** An unscrewed screw: picked up, carried and thrown like any held object. Placeholder look: an engine cylinder the size of the screw */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Spin Screw Pickup"))
class GSTNSPINSCREW_API AGSTN_SpinScrewPickup : public AGSTN_HeldObjectActor
{
	GENERATED_BODY()

public:
	AGSTN_SpinScrewPickup();
};

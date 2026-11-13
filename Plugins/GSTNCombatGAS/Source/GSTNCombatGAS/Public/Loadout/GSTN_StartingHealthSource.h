// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_StartingHealthSource.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_StartingHealthSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * A component of a fighter that wants it to start below full health (M8 G9b: a fighter promoted from a minion keeps the minion's health).
 * UGSTN_LoadoutComponent asks the owner's components for it right after the kit's init effect, on the authority, outside the simulation
 * (the same moment the init effect itself is applied), and sets Health = MaxHealth x the fraction. The combat layer does not know who
 * implements it: the minion plugin does, so GSTNCombatGAS keeps no dependency on it.
 */
class IGSTN_StartingHealthSource
{
	GENERATED_BODY()

public:
	/** Health / MaxHealth to start at, in [0, 1]. 1 (or more) leaves the init effect's value untouched */
	virtual float GetStartingHealthFraction() const = 0;
};

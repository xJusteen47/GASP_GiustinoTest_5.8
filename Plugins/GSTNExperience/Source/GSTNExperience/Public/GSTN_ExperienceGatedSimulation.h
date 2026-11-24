#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_ExperienceGatedSimulation.generated.h"

class UGSTN_ExperienceDefinition;

UINTERFACE(MinimalAPI, NotBlueprintable)
class UGSTN_ExperienceGatedSimulation : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by anything that must not simulate before the Experience is Loaded: a Network Prediction pawn, its sim component, a
 * tick extension. The manager component calls it once, on the game thread, when the Experience reaches Loaded: after the
 * Experience's own "loaded" delegates (so pawn data and ability sets are already applied) and before the next simulation frame.
 *
 * The contract for the implementer: from registration until this call, do not run a simulation tick (ask
 * UGSTN_ExperienceGateLibrary::TryBeginSimTick first). Ticking is deferred, not skipped: nothing is queued or replayed.
 *
 * This interface exists so the NP adapters never need the fork changed: the gate is on the caller's side
 * (plan m12 section 5: "a pawn ticks before Active and cannot be held back without a fork change -> stop").
 */
class GSTNEXPERIENCE_API IGSTN_ExperienceGatedSimulation
{
	GENERATED_BODY()

public:
	virtual void GSTN_OnExperienceGateOpened(const UGSTN_ExperienceDefinition* Experience) = 0;
};

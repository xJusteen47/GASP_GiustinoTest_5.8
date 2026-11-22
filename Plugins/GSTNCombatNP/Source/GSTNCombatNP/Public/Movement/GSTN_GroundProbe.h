// Combat simulation (GAS-NPP fork): the floor probe shared by the moves that slide on the ground (momentum slide, run skid).

#pragma once

#include "CoreMinimal.h"

class UMoverComponent;
class UPrimitiveComponent;
struct FMoverTickStartData;
struct FGSTN_SurfaceView;

/**
 * Stateless ground queries for Mover moves, from the start state of the step only (no component cache), so every machine and every
 * re-simulation asks the same question. One probe for every sliding move (plan corsa-mario-piano: "niente sonda nuova").
 */
struct GSTNCOMBATNP_API FGSTN_GroundProbe
{
	/** How far under the feet the ledge probe looks, cm (as the push back) */
	static constexpr double LedgeProbeDepth = 60.0;

	/** True when the next step at Velocity keeps floor under the feet */
	static bool HasFloorAhead(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp, const FVector& Velocity, double StepMs);

	/** The surface the character stands on (the movement base of the start state), false when it has none */
	static bool ResolveFloorSurface(const FMoverTickStartData& StartState, FGSTN_SurfaceView& OutView);
};

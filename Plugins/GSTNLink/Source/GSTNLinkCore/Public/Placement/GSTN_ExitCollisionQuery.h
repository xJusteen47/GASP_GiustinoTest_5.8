// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

/**
 * What an exit placer asks the world (m6 spec 3 and 7.7): whether a capsule fits at a place. The placement code sweeps nothing itself,
 * so the Core module needs no world and the tests use a fake. The Network Prediction module implements it with a capsule overlap
 * against the world, on the server only; a placer that gets no query accepts its first candidate.
 *
 * Deterministic for equal inputs: the answer depends on the capsule and the world at the frame the placer runs, nothing else.
 */
class GSTNLINKCORE_API IGSTN_ExitCollisionQuery
{
public:
	virtual ~IGSTN_ExitCollisionQuery() = default;

	/** Whether a capsule of this radius and half height, centred at Location with this rotation, overlaps no blocking geometry */
	virtual bool IsCapsuleFree(const FVector& Location, const FQuat& Rotation, float Radius, float HalfHeight) const = 0;
};

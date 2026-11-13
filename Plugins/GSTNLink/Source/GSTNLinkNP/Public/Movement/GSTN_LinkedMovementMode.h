// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "GSTN_LinkedMovementMode.generated.h"

/**
 * The parked movement mode of a linked role (m6 spec 7.3), shared by the grabbed, a latcher, a rider and any later parked role: zero velocity,
 * no ground snap, no gravity, no collision response, it does not read input. It keeps the capsule at its last real position: the PARKED
 * TRANSFORM, stable on every machine because nothing moves it. Where the entity seems to be (its mesh on the partner's socket) is
 * presentation (UGSTN_LinkPresentationComponent), and where it is hit (its hurtboxes on the partner's baked socket) is the snapshot's
 * (FGSTN_LinkSnapshotContributor); neither moves the capsule, so the partner's prediction never pulls the parked entity's simulation.
 *
 * Entered and left through UGSTN_LinkedModeTransition on Combat.Mover.Linked (FGSTN_LinkedModifier); the mode itself carries its exit
 * transition. Rotation is kept too: the facing of a parked entity is presentation.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, DisplayName = "GSTN Linked (Parked)")
class GSTNLINKNP_API UGSTN_LinkedMovementMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	UGSTN_LinkedMovementMode(const FObjectInitializer& ObjectInitializer);

	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		FProposedMove& OutProposedMove) const override;

	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
};

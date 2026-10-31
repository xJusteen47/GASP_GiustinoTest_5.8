// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "MovementModeTransition.h"
#include "GSTN_LinkedModeTransition.generated.h"

class UMoverComponent;

/**
 * The transition of the parked link mode (m6 spec 7.3), on Combat.Mover.Linked in Mover's sync state (the FGSTN_LinkedModifier carries it):
 *
 *  - Entry (bExitTransition false): from any other mode into LinkedModeName while the tag is present. A project authors it as a global
 *    transition of the pawn's Mover; EnsureMoverSetup adds one to every mode of a Mover that has none (the harness pawns).
 *  - Exit (bExitTransition true, the linked mode's own): back to ExitModeName once the tag is gone; Falling becomes Walking by itself on the
 *    ground, so an exit never has to know where the entity stands (the exit teleport of F11 places it first).
 *
 * Reads Mover's start state of the tick only, so it decides the same on every machine and in a re-simulation.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, DisplayName = "GSTN Linked Mode Transition")
class GSTNLINKNP_API UGSTN_LinkedModeTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()

public:
	/** False: enters LinkedModeName while the tag is present. True: leaves to ExitModeName once it is gone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	bool bExitTransition = false;

	/** The name the parked mode is registered under on the Mover component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	FName LinkedModeName;

	/** Where the exit goes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	FName ExitModeName;

	UGSTN_LinkedModeTransition();

	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	/** The default name of the parked mode ("GSTN_Linked") */
	static FName GetDefaultLinkedModeName();

	/**
	 * Adds the parked mode (UGSTN_LinkedMovementMode under GetDefaultLinkedModeName, with its exit transition) to Mover when it has none, and
	 * an entry transition to every other mode that has none. Content setup outside any tick, the same on every machine; idempotent
	 */
	static void EnsureMoverSetup(UMoverComponent* Mover);
};

// Copyright Giustino. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_LocomotionStateLibrary.generated.h"

struct FAnimUpdateContext;
struct FAnimNodeReference;

/**
 * Routes our own states of the main locomotion ABP (ABP_GSTN_Locomotion: Skid, Run Stop, Crouch Enter/Idle/Exit, Run Start...) to the visible pose.
 *
 * Why: in GASP's experimental setup (LocomotionSetup = 1, the default here) the "State Controller" state machine outputs NO pose. The Two Way
 * Blend after it lets only the Blend Stack through and keeps the machine updated ("always update children"); each GASP state writes the Blend
 * Stack in its On State Entry function through SetBlendStackAnimFromChooser, keyed by the state's name in CHT_GSTN_LocomotionAnimations.
 * A state that only holds its own Sequence Evaluator is entered but never seen (crouch-veloce-2, 2026-11-19).
 *
 * So our states bind On State Entry to SetBlendStackFromStateName: it reads the name of the state just entered and calls the ABP's own
 * SetBlendStackAnimFromChooser with it, the same call a GASP state makes. The clip, start time and blend time are a chooser row (owner rule:
 * raw clips as chooser rows, switching in the main state machine). One binding serves every state, so a new state only needs its row.
 */
UCLASS()
class GSTNCOMBATNP_API UGSTN_LocomotionStateLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * On State Entry function for a state of the State Controller machine: the Blend Stack plays the chooser row named after the state
	 * (see ChooserKeyForState). Node is the state machine (the engine passes the machine node to state entry functions).
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Locomotion", meta = (BlueprintThreadSafe))
	static void SetBlendStackFromStateName(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	/**
	 * The chooser row key of a state: its name, except "Run Stop" which picks "Run Stop L" / "Run Stop R" by the planted foot of the
	 * owner's run stop (UGSTN_RunPresentationComponent).
	 */
	static FName ChooserKeyForState(const UAnimInstance* AnimInstance, FName StateName);

	/** Name of the ABP function every state entry calls (GASP's experimental state machine) */
	static const FName SetBlendStackFunctionName;
};

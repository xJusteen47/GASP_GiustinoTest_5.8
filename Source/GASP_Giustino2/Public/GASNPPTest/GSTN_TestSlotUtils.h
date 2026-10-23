// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_TestSlotUtils.generated.h"

class UGSTN_InputSlotSet;
class APlayerController;
class UInputAction;

/**
 * Editor and Python helpers to write the harness' slots into a slot set asset.
 *
 * A slot's pattern is made of instanced structs (input conditions), which Python cannot construct or assign, so the test
 * setup (Tools/GAS/setup_gas_assets.py) asks C++ to do it. It lives in the project, not in the input plugins: a plain
 * "press this button" slot for the harness' own test commands (Command.Dash, Command.Buff) is test content, not part of any
 * plugin's standard command set.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_TestSlotUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Adds to Set a slot that starts Command on a plain press of Button (a single step, no stick or crouch condition), unless
	 * the set already has a slot for Command, so a setup script can run again without duplicating it. Marks the set dirty;
	 * saving it is the caller's job.
	 *
	 * @return True if the slot was added; false if it was already there or an argument was invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPPTest|Input")
	static bool AddButtonPressSlot(UGSTN_InputSlotSet* Set, FGameplayTag Command, FGameplayTag Button);

	/**
	 * Input for a pawn that has no GSTN_GASNPPTestComponent (the pawn override of the harness, e.g. SandboxCharacter_Mover): the local player's
	 * enhanced input subsystem injects Action. bPulse = one frame (a press); otherwise a continuous injection that StopInput ends.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPPTest|Input")
	static bool InjectInput(APlayerController* Controller, UInputAction* Action, FVector Value, bool bPulse);

	UFUNCTION(BlueprintCallable, Category = "GASNPPTest|Input")
	static bool StopInput(APlayerController* Controller, UInputAction* Action);
};

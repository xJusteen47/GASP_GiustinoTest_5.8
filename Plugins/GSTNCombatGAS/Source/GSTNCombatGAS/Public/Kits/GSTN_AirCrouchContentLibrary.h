// Ability-system layer of the combat framework.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_AirCrouchContentLibrary.generated.h"

class UGSTN_PrimaryKitDefinition;

/**
 * What Tools/GAS/setup_air_crouch.py needs to give a kit its air crouch (FGSTN_AirCrouchKitExtension) without hand edits: the instanced kit
 * extension is written here, where the type is known, so Python never builds an FInstancedStruct. Content only, never the simulation.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_AirCrouchContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gives Kit an Air Crouch extension with its defaults when it has none (an existing one is kept as the designer tuned it).
	 * Returns whether the kit changed (dirty only then)
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Combat|Content")
	static bool EnsureKitAirCrouch(UGSTN_PrimaryKitDefinition* Kit);

	/** Does Kit have an enabled Air Crouch extension */
	UFUNCTION(BlueprintPure, Category = "GSTN|Combat|Content")
	static bool HasKitAirCrouch(const UGSTN_PrimaryKitDefinition* Kit);
};

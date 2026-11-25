// Project-wide switches of the spin and drill plugin.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "GSTN_SpinDrillSettings.generated.h"

class UGSTN_SpinDrillTuning;

/**
 * Project Settings > Plugins > GSTN Spin and Drill. Read where the loadout is compiled (every machine of a match shares the project settings, so
 * every machine compiles the same binding table), never in the simulation.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "GSTN Spin and Drill"))
class GSTNSPINDRILLGAS_API UGSTN_SpinDrillSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGSTN_SpinDrillSettings& Get() { return *GetDefault<UGSTN_SpinDrillSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * Every kit asset gets the spin and the drill (owner rule: shared systems on all test kits), with the bore and DefaultTuning, unless it
	 * binds a spin command itself or carries its own FGSTN_SpinDrillKitExtension (then the kit decides). Kits built in memory by code (tests)
	 * keep their authored table. Off = only kits with the extension or their own bindings spin
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Kits")
	bool bAddSpinToEveryKit = true;

	/** The keyboard shortcut (A-S-D / D-S-A) next to the stick rotation, for the kits that get the spin from this setting */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Kits")
	bool bKeyboardShortcut = true;

	/** Tuning for the kits that get the spin from this setting. Empty = the defaults of the tuning structs */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Kits")
	TSoftObjectPtr<UGSTN_SpinDrillTuning> DefaultTuning;
};

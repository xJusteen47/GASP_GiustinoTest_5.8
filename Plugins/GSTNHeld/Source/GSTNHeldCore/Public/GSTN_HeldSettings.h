// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_HeldTypes.h"

#include "GSTN_HeldSettings.generated.h"

/**
 * Project defaults of held objects (Project Settings > Plugins > GSTN Held, saved in the plugin's DefaultGSTNHeld.ini). Everything here is a TEST
 * VALUE of the owner's 2026-11-10 button design, kept as data so playtesting changes it without code.
 */
UCLASS(config = GSTNHeld, defaultconfig, meta = (DisplayName = "GSTN Held"))
class GSTNHELDCORE_API UGSTN_HeldSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGSTN_HeldSettings& Get() { return *GetDefault<UGSTN_HeldSettings>(); }

	/** When a holder drops what it holds without wanting to; an object's own tuning can override it (bOverrideBreakRules) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Break")
	FGSTN_HeldBreakRules DefaultBreakRules;

	/** How the strong button throws (tap = stick, held = camera aim with the reticle) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Throw")
	FGSTN_HeldThrowInput ThrowInput;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

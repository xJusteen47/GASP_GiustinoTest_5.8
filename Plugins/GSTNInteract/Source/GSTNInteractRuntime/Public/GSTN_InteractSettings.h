// Interaction project settings (interaction.md §9).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_InteractSettings.generated.h"

class UGSTN_InteractPresenter;

/**
 * Project Settings > Plugins > GSTN Interact. A project overrides the defaults in its own Config/DefaultGSTNInteract.ini. The rules and
 * the meaning table are simulation inputs once a bridge reads them: every machine of a match must hold the same values. The
 * presentation values are local.
 */
UCLASS(config = GSTNInteract, defaultconfig, meta = (DisplayName = "GSTN Interact"))
class GSTNINTERACTRUNTIME_API UGSTN_InteractSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_InteractSettings();

	static const UGSTN_InteractSettings& Get() { return *GetDefault<UGSTN_InteractSettings>(); }

	/** Master switch (also console variable gstn.Interact.Enable) */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	/** Rules used by interactors that name no rules asset */
	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	FGSTN_InteractRules DefaultRules;

	/** The meanings (tag, default priority, verbs, hold frames). Compiled sorted by tag name, so the order here does not matter */
	UPROPERTY(Config, EditAnywhere, Category = "Meanings")
	TArray<FGSTN_InteractMeaning> Meanings;

	/** Server frames per second used to turn server world time into the frame stamps carry. Matches the 62.5 Hz fixed tick */
	UPROPERTY(Config, EditAnywhere, Category = "Frames", meta = (ClampMin = 1, ForceUnits = "Hz"))
	float FrameRateHz = 62.5f;

	/** Extra room around the sticky reach in the registry's grid cells, for candidate radii */
	UPROPERTY(Config, EditAnywhere, Category = "Registry", meta = (ClampMin = 0, ForceUnits = "cm"))
	float GridMarginCm = 100.f;

	/** Presenters created for each local interactor. Empty uses the built-in debug presenter (ring and text) */
	UPROPERTY(Config, EditAnywhere, Category = "Presentation")
	TArray<TSubclassOf<UGSTN_InteractPresenter>> PresenterClasses;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** DefaultRules after Sanitise, so a bad ini value cannot break the maths */
	FGSTN_InteractRules GetSanitisedDefaultRules() const;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_ExperienceSettings.generated.h"

/**
 * Project settings of the Experience plugin (Project Settings > GSTN > Experience). Defaults here, so the plugin works when copied
 * alone into another project (modularity-and-tags.md 3.7 rule 1).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GSTN Experience"))
class GSTNEXPERIENCE_API UGSTN_ExperienceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_ExperienceSettings() { CategoryName = TEXT("GSTN"); }

	/**
	 * Register every GameState as a Game Framework component receiver, so Game Feature "add components" actions can target it. Turn
	 * off when the project's GameState already derives from a modular GameState base (it registers itself; registering twice
	 * would add every component twice).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "GameState")
	bool bRegisterGameStateAsReceiver = true;

	/** Add UGSTN_ExperienceManagerComponent to every GameState (through the same component manager the actions use). Turn off if the GameState class already has one as a default subobject. */
	UPROPERTY(Config, EditAnywhere, Category = "GameState")
	bool bAddManagerToGameState = true;
};

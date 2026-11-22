// Logical buttons and their Enhanced Input actions: who holds a player's input config.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_InputConfigSource.generated.h"

class UGSTN_InputConfig;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_InputConfigSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * A component of a pawn that drives it with a UGSTN_InputConfig (the combat simulation, a standalone input host). Local presentation
 * that needs the player's buttons outside the simulation (a menu reading Input.Button.* through the button layout) finds the config
 * here without depending on the plugin that owns it.
 */
class GSTNINPUTCORE_API IGSTN_InputConfigSource
{
	GENERATED_BODY()

public:
	/** The config the pawn's player input reads, its actions resolved (UGSTN_InputConfig::ResolveActions); null while not set up */
	virtual const UGSTN_InputConfig* GetPlayerInputConfig() const = 0;

	/** The first component of Actor that is a source and has a config, else null */
	static const UGSTN_InputConfig* FindOn(const AActor* Actor);
};

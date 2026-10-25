// Pure part logic (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Registers the plugin's tag ini folder (Config/Tags): the engine does not scan the Config/Tags of a plugin by itself. Nothing
 * else happens at startup; the settings object reads its own config and the tags are native.
 */
class GSTNPARTSCORE_API FGSTNPartsCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
};

// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class SWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogGSTNCombatDebug, Log, All);

/**
 * What other plugins may ask of the debug module. It is an interface so that a plugin that wants a debug widget does not link
 * this module: it asks FModuleManager for the module by name and gets nothing back when the plugin is disabled or this is a
 * Shipping build (the plugin descriptor excludes it). The editor plugin does not use it: the editor tab of the input display is
 * registered by this module itself (see FGSTN_CombatDebugModule), so the editor has no edge to the debug plugin at all.
 */
class IGSTN_CombatDebugModule : public IModuleInterface
{
public:
	/** The module when it is loaded, null when the plugin is disabled or excluded from this build. Never forces a load. */
	static IGSTN_CombatDebugModule* Get();

	/** True when the debug module is loaded */
	static bool IsAvailable();

	/**
	 * A widget showing the pressed inputs of the local player in the notation of the input plugin, with the frame gaps and a box
	 * for a scripted input (E14): the strip that is the corner overlay and the editor tab. It follows the first PIE or game world
	 * that has a local player with a combat component. Null when Slate is not running (a commandlet, a dedicated server), which
	 * callers treat like an absent module.
	 */
	virtual TSharedPtr<SWidget> CreateInputDisplay() = 0;
};

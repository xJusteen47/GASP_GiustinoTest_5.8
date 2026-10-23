// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class SWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogGSTNCombatDebug, Log, All);

/**
 * What other plugins may ask of the debug module. A plugin that wants a debug widget gets the module through Get(), which asks
 * FModuleManager by name and returns null when the plugin is disabled or this is a Shipping build (the plugin descriptor excludes
 * it), so the caller treats absence as normal. The class is exported so that Get() and IsAvailable() link; a plugin that must not
 * depend on this module at all can skip them and call FModuleManager::GetModulePtr<IGSTN_CombatDebugModule>("GSTNCombatDebug")
 * itself, which needs only this header's include path (GSTNCombat.Debug.Module.InterfaceIsExported pins the export). The editor
 * plugin does neither: the editor tab of the input display is registered by this module itself (see FGSTN_CombatDebugModule),
 * so the editor has no edge to the debug plugin at all.
 */
class GSTNCOMBATDEBUG_API IGSTN_CombatDebugModule : public IModuleInterface
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

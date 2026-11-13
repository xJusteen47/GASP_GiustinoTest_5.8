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
 * plugin does neither: the editor tab of the input display is registered by the input debug module (GSTNInputDebug), so the editor
 * has no edge to the debug plugin at all.
 */
class GSTNCOMBATDEBUG_API IGSTN_CombatDebugModule : public IModuleInterface
{
public:
	/** The module when it is loaded, null when the plugin is disabled or excluded from this build. Never forces a load. */
	static IGSTN_CombatDebugModule* Get();

	/** True when the debug module is loaded */
	static bool IsAvailable();

	/**
	 * The input display of the local player (GSTNInputDebug's widget: strip, script box, player picker), with the combat component
	 * registered as something it can read and the standard command set as its default. It follows the play window that has the focus or
	 * had it last. The display itself belongs to the input plugin since the input tools moved there; this forwards to
	 * IGSTN_InputDebugModule::CreateInputDisplay, so a caller of the combat module keeps working. Null when Slate is not running (a
	 * commandlet, a dedicated server) or the input debug module is absent, which callers treat like an absent module.
	 */
	virtual TSharedPtr<SWidget> CreateInputDisplay() = 0;
};

// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class SWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogGSTNInputDebug, Log, All);

/**
 * What other plugins may ask of the input debug module. A plugin that wants the input display gets the module through Get(), which asks
 * FModuleManager by name and returns null when the plugin is disabled or this is a Shipping build (the plugin descriptor excludes it),
 * so the caller treats absence as normal. The class is exported so that Get() and IsAvailable() link; a plugin that must not depend on
 * this module can skip them and call FModuleManager::GetModulePtr<IGSTN_InputDebugModule>("GSTNInputDebug") itself, which needs only this
 * header's include path (GSTNInput.Debug.Module.InterfaceIsExported pins the export).
 *
 * What the display can read is registered with FGSTN_InputSourceRegistry (Sources/GSTN_InputSourceRegistry.h), which is also exported.
 */
class GSTNINPUTDEBUG_API IGSTN_InputDebugModule : public IModuleInterface
{
public:
	/** The module when it is loaded, null when the plugin is disabled or excluded from this build. Never forces a load. */
	static IGSTN_InputDebugModule* Get();

	/** True when the input debug module is loaded */
	static bool IsAvailable();

	/**
	 * The input display: the strip of the local player's input history (buttons as coloured icons, the stick as arrows, frame gaps), the
	 * box for a scripted input and the player picker. It follows the play window that has the focus or had it last. Null when Slate is not
	 * running (a commandlet, a dedicated server), which callers treat like an absent module.
	 */
	virtual TSharedPtr<SWidget> CreateInputDisplay() = 0;
};

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
 * Shipping build (the plugin descriptor excludes it). The editor plugin will use it to embed the input display in its
 * toolkit (combat-m5-editor 3b); the interface lives here, in the plugin that implements it.
 */
class IGSTN_CombatDebugModule : public IModuleInterface
{
public:
	/** The module when it is loaded, null when the plugin is disabled or excluded from this build. Never forces a load. */
	static IGSTN_CombatDebugModule* Get();

	/** True when the debug module is loaded */
	static bool IsAvailable();

	/**
	 * A widget showing the pressed inputs of a pawn in the notation of the input plugin, with the frame gaps (E14). Declared
	 * now so the editor can be written against it; returns null until E14 implements it, which callers treat like an absent
	 * module (they show nothing).
	 */
	virtual TSharedPtr<SWidget> CreateInputDisplay() = 0;
};

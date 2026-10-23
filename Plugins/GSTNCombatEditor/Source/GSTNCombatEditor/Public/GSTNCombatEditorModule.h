// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGSTNCombatEditor, Log, All);

/**
 * The editor module (combat-m5-editor 3.1). StartupModule is where each checkpoint registers its piece: asset definitions and
 * toolkit tab spawners (E4, E5), the built-in track editors (E5), property type layouts (E12, E13), menus and console
 * commands (E8, E11). ShutdownModule reverses each one, in the opposite order. E1 registers nothing: the settings object
 * registers itself with Project Settings through UDeveloperSettings.
 */
class GSTNCOMBATEDITOR_API FGSTN_CombatEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** The module when it is loaded, null otherwise. Never forces a load */
	static FGSTN_CombatEditorModule* Get();
};

// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"

/**
 * The validation panel's place in the editor: the nomad tab (Window > GSTN Combat > Validation) and the console commands that run Validate all
 * (combat-m5-editor 9.1). The module registers it at startup and removes it at shutdown. Without a Slate application (a commandlet) only the
 * console command exists.
 */
namespace GSTNValidationTab
{
	/** The id the tab is registered under */
	FName GetTabId();

	void Register();
	void Unregister();

	/**
	 * gstn.Combat.ValidateAll [path prefix]: runs Validate all, writes the findings to the Message Log and logs the counts. The same run the panel's
	 * button does, for a build script or a commandlet (-ExecCmds).
	 */
	void RunValidateAllCommand(const TArray<FString>& Args);
}

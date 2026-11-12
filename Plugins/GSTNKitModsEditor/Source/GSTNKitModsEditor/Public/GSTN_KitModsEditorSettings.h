// Editor tooling of kit mods (M11 L8). Editor only.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_KitModsEditorSettings.generated.h"

/**
 * Editor preferences of the mod tooling (per user, Editor Preferences > Plugins). Tooling only: nothing here changes a game, which is why
 * it is not a project setting.
 */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "GSTN Kit Mods Editor"))
class GSTNKITMODSEDITOR_API UGSTN_KitModsEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * How far the scaled hitboxes may end past (or short of) the mod's blade mesh before the reach rule warns, as a fraction of the mesh's
	 * reach. The check is an estimate from bounds, so it needs slack
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Validation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ReachMeshTolerance = 0.25f;

	static float GetReachMeshTolerance();
};

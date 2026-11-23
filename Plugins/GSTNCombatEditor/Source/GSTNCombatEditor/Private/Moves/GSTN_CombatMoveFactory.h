// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "Moves/GSTN_CombatMoveAuthoring.h"
#include "GSTN_CombatMoveFactory.generated.h"

/**
 * Content Browser > Add > Gameplay > Combat > Combat Move (CP5 of the Blueprint seams). A dialog asks the template (strike, action, empty), the
 * frames, the montage, whether the timeline is its own asset, whether the move events go in the graph, and optionally the kit or weapon and
 * the command to bind it to; then UGSTN_CombatMoveAuthoring makes the move. The asset is a Gameplay Ability Blueprint child of the combat
 * ability, the same as one made by hand, so nothing downstream knows how it was made.
 *
 * Scripts set Options and call AssetTools.create_asset (which shows no dialog), or call UGSTN_CombatMoveAuthoring::CreateCombatMove directly.
 */
UCLASS(HideCategories = Object)
class UGSTN_CombatMoveFactory : public UFactory
{
	GENERATED_BODY()

public:
	UGSTN_CombatMoveFactory();

	UPROPERTY(EditAnywhere, Category = "Combat Move")
	FGSTN_NewCombatMoveOptions Options;

	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTip() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual const TArray<FText>& GetMenuCategorySubMenus() const override;
	virtual FString GetDefaultNewAssetName() const override;
};

namespace GSTNCombatMoveMenu
{
	/** Adds "Add to Kit or Weapon..." to the asset context menu of a move Blueprint. From the module's startup, through the tool menus startup callback */
	void Register();
	void Unregister();
}

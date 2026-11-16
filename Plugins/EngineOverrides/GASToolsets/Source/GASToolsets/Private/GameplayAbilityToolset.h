// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UObject/ScriptInterface.h"

#include "GameplayAbilityToolset.generated.h"

class UBlueprint;

/// Provides tools for creating Gameplay Ability Blueprints.
UCLASS(BlueprintType, Hidden)
class UGameplayAbilityToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new Gameplay Ability Blueprint asset, the same as the Content Browser's
	 * Gameplay Ability Blueprint factory. Unlike a plain Blueprint with a GameplayAbility parent,
	 * its root ability has a "Gameplay Ability Graph" in place of the default "EventGraph".
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param FolderPath The content browser folder for the asset, e.g. "/Game/Abilities".
	 * @param AssetName The file name for the new asset, e.g. "GA_Dash".
	 * @param ParentClass The parent class: UGameplayAbility or a subclass, including Blueprint subclasses.
	 * @return The new Blueprint.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GameplayAbilities")
	static UBlueprint* CreateGameplayAbility(const FString& FolderPath, const FString& AssetName, UClass* ParentClass);

	//~ Begin UToolsetDefinition Interface
	virtual bool IsToolsetHidden_Implementation(const TScriptInterface<IToolsetPermissionProvider>& Provider) const override;
	//~ End UToolsetDefinition Interface

private:
	friend class FGameplayAbilityToolsetSpec;
};

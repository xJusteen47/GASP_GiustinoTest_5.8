// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilityToolset.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/Blueprint.h"
#include "GameplayAbilitiesBlueprintFactory.h"
#include "GameplayAbilityBlueprint.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ToolsetRegistry/AssetHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayAbilityToolset)

UBlueprint* UGameplayAbilityToolset::CreateGameplayAbility(
	const FString& FolderPath, const FString& AssetName, UClass* ParentClass)
{
	// Checked up front because the factory asks the user about an invalid parent in a dialog
	if (!ParentClass || !ParentClass->IsChildOf(UGameplayAbility::StaticClass())
		|| !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("Cannot create a Gameplay Ability Blueprint based on '%s'. ParentClass must be UGameplayAbility or a Blueprintable subclass."),
			ParentClass ? *ParentClass->GetName() : TEXT("null")));
		return nullptr;
	}

	UGameplayAbilitiesBlueprintFactory* Factory = NewObject<UGameplayAbilitiesBlueprintFactory>();
	Factory->ParentClass = ParentClass;
	Factory->BlueprintType = BPTYPE_Normal;
	return Cast<UBlueprint>(UE::ToolsetRegistry::CreateAsset(
		FolderPath, AssetName, UGameplayAbilityBlueprint::StaticClass(), Factory));
}

bool UGameplayAbilityToolset::IsToolsetHidden_Implementation(const TScriptInterface<IToolsetPermissionProvider>& Provider) const
{
	return Provider->IsRestricted();
}

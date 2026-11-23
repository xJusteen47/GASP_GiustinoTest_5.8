// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCueToolset.h"

#include "AbilitySystemGlobals.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "GameplayCueManager.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueSet.h"
#include "GameplayTagsEditorModule.h"
#include "GameplayTagsManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Modules/ModuleManager.h"
#include "Selection.h"
#include "ToolsetRegistry/AssetHelpers.h"
#include "ToolsetRegistry/ToolsetLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayCueToolset)

namespace
{
	static const FString GameplayCueRootTag = TEXT("GameplayCue");

	TArray<FString> CollectDescendantTagNames(const TSharedPtr<FGameplayTagNode>& ParentNode)
	{
		TArray<FString> TagNames;
		TArray<TSharedPtr<FGameplayTagNode>> Queue = ParentNode->GetChildTagNodes();
		while (Queue.Num() > 0)
		{
			TSharedPtr<FGameplayTagNode> Node = Queue.Pop(EAllowShrinking::No);
			if (Node.IsValid())
			{
				TagNames.Add(Node->GetCompleteTagName().ToString());
				Queue.Append(Node->GetChildTagNodes());
			}
		}
		return TagNames;
	}

	UGameplayCueManager* GetCueManager()
	{
		return UAbilitySystemGlobals::Get().GetGameplayCueManager();
	}

	// The cue manager's editor cue set indexes the notifies in the project's gameplay cue notify
	// paths. Raises and returns null if the manager has not built it.
	const UGameplayCueSet* GetEditorCueSet()
	{
		UGameplayCueManager* Manager = GetCueManager();
		const UGameplayCueSet* CueSet = Manager ? Manager->GetEditorCueSet() : nullptr;
		if (!CueSet)
		{
			UKismetSystemLibrary::RaiseScriptError(TEXT("The gameplay cue editor library is not initialized."));
		}
		return CueSet;
	}

	// The set's tag map redirects a tag without a notify to its parent's notify, so look the
	// notify up by exact tag instead.
	const FGameplayCueNotifyData* FindNotifyForTag(const UGameplayCueSet& CueSet, const FGameplayTag& Tag)
	{
		return CueSet.GameplayCueData.FindByPredicate([&Tag](const FGameplayCueNotifyData& Data)
		{
			return Data.GameplayCueTag == Tag;
		});
	}
}

TArray<FString> UGameplayCueToolset::ListCues(const FString& ParentTag)
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();

	const FString& RootName = ParentTag.IsEmpty() ? GameplayCueRootTag : ParentTag;
	TSharedPtr<FGameplayTagNode> RootNode = TagManager.FindTagNode(FName(*RootName));

	if (!RootNode.IsValid())
	{
		if (!ParentTag.IsEmpty())
		{
			UKismetSystemLibrary::RaiseScriptError(
				FString::Printf(TEXT("Gameplay tag '%s' does not exist."), *ParentTag));
		}
		return {};
	}

	TArray<FString> TagNames = CollectDescendantTagNames(RootNode);
	TagNames.Sort();
	return TagNames;
}

FGameplayCueInfo UGameplayCueToolset::GetCueInfo(const FString& CueTag)
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
	if (!TagManager.FindTagNode(FName(*CueTag)).IsValid())
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("Gameplay cue tag '%s' does not exist."), *CueTag));
		return FGameplayCueInfo();
	}

	const UGameplayCueSet* CueSet = GetEditorCueSet();
	if (!CueSet)
	{
		return FGameplayCueInfo();
	}

	FGameplayCueInfo Info;
	Info.Tag = CueTag;
	if (const FGameplayCueNotifyData* Notify = FindNotifyForTag(*CueSet, FGameplayTag::RequestGameplayTag(FName(*CueTag))))
	{
		// The set holds the notify's generated class; the asset path is the same without the "_C" suffix.
		Info.NotifyAssetPath = Notify->GameplayCueNotifyObj.ToString();
		Info.NotifyAssetPath.RemoveFromEnd(TEXT("_C"));
	}
	return Info;
}

bool UGameplayCueToolset::ExecuteCueOnSelectedActor(
	const FString& CueTag, float NormalizedMagnitude, FVector Location, FVector Normal)
{
	if (!GEditor)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Editor not found."));
		return false;
	}

	AActor* TargetActor = nullptr;
	for (FSelectionIterator Iter(*GEditor->GetSelectedActors()); Iter; ++Iter)
	{
		if (AActor* Actor = Cast<AActor>(*Iter))
		{
			TargetActor = Actor;
			break;
		}
	}

	if (!TargetActor)
	{
		UKismetSystemLibrary::RaiseScriptError(
			TEXT("No actor selected. Select an actor in the editor before executing the cue."));
		return false;
	}

	const FGameplayTag Tag =
		UGameplayTagsManager::Get().RequestGameplayTag(FName(*CueTag), false);
	if (!Tag.IsValid())
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("Gameplay cue tag '%s' does not exist."), *CueTag));
		return false;
	}

	FGameplayCueParameters Params;
	Params.NormalizedMagnitude = NormalizedMagnitude;
	Params.Location = Location;
	Params.Normal = Normal;

	UGameplayCueManager::ExecuteGameplayCue_NonReplicated(TargetActor, Tag, Params);
	return true;
}

FString UGameplayCueToolset::CreateCueNotifyAsset(
	const FString& CueTag, const FString& PackagePath,
	const FString& AssetName, bool bIsActor)
{
	if (CueTag.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("CueTag cannot be empty."));
		return FString();
	}
	if (PackagePath.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("PackagePath cannot be empty."));
		return FString();
	}
	if (AssetName.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("AssetName cannot be empty."));
		return FString();
	}

	const FGameplayTag Tag =
		UGameplayTagsManager::Get().RequestGameplayTag(FName(*CueTag), false);
	if (!Tag.IsValid())
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("Gameplay cue tag '%s' does not exist. Add it first with AddCueTag."), *CueTag));
		return FString();
	}

	if (!UToolsetLibrary::ValidateAssetPath(PackagePath))
	{
		return FString();
	}

	// The cue manager only registers notifies in its valid cue paths, and otherwise asks the user
	// about it in a dialog, which the guard suppresses.
	UGameplayCueManager* CueManager = GetCueManager();
	if (!CueManager)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("The gameplay cue manager is not available."));
		return FString();
	}
	bool bInValidCuePath = false;
	{
		TGuardValue<bool> NoDialogGuard(GIsRunningUnattendedScript, true);
		bInValidCuePath = CueManager->VerifyNotifyAssetIsInValidPath(PackagePath / AssetName);
	}
	if (!bInValidCuePath)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("'%s' is not in a gameplay cue notify path, so a notify created there would not be registered."),
			*PackagePath));
		return FString();
	}

	// The cue set keeps one notify per tag and skips any later one, so a second notify would never be routed.
	const UGameplayCueSet* CueSet = GetEditorCueSet();
	if (!CueSet)
	{
		return FString();
	}
	if (const FGameplayCueNotifyData* ExistingNotify = FindNotifyForTag(*CueSet, Tag))
	{
		FString ExistingPath = ExistingNotify->GameplayCueNotifyObj.ToString();
		ExistingPath.RemoveFromEnd(TEXT("_C"));
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("Gameplay cue tag '%s' already has a notify: %s."), *CueTag, *ExistingPath));
		return FString();
	}

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = bIsActor
		? AGameplayCueNotify_Actor::StaticClass()
		: UGameplayCueNotify_Static::StaticClass();

	UBlueprint* NewBP = Cast<UBlueprint>(
		UE::ToolsetRegistry::CreateAsset(PackagePath, AssetName, UBlueprint::StaticClass(), Factory));
	if (!NewBP)
	{
		return FString();
	}
	if (!NewBP->GeneratedClass || !NewBP->GeneratedClass->GetDefaultObject())
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("Failed to create GameplayCueNotify Blueprint '%s'."), *AssetName));
		return FString();
	}

	if (!bIsActor)
	{
		UGameplayCueNotify_Static* CDO = Cast<UGameplayCueNotify_Static>(NewBP->GeneratedClass->GetDefaultObject());
		CDO->GameplayCueTag = Tag;
		CDO->GameplayCueName = Tag.GetTagName();
	}
	else
	{
		AGameplayCueNotify_Actor* CDO = Cast<AGameplayCueNotify_Actor>(NewBP->GeneratedClass->GetDefaultObject());
		CDO->GameplayCueTag = Tag;
		CDO->GameplayCueName = Tag.GetTagName();
	}

	NewBP->MarkPackageDirty();

	// The cue manager registered the notify when the Blueprint was created, before the cue tag was
	// set above, so replace that registration.
	CueManager->HandleAssetDeleted(NewBP);
	CueManager->HandleAssetAdded(NewBP);
	return NewBP->GetPathName();
}

bool UGameplayCueToolset::AddCueTag(const FString& CueTag, const FString& Comment)
{
	if (CueTag.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("CueTag cannot be empty."));
		return false;
	}

	if (!CueTag.StartsWith(GameplayCueRootTag + TEXT(".")))
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("Gameplay cue tags must begin with '%s.' (got '%s')."),
			*GameplayCueRootTag, *CueTag));
		return false;
	}

	IGameplayTagsEditorModule& EditorModule =
		FModuleManager::LoadModuleChecked<IGameplayTagsEditorModule>("GameplayTagsEditor");

	const bool bSuccess = EditorModule.AddNewGameplayTagToINI(CueTag, Comment, NAME_None);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("Failed to add gameplay cue tag '%s'."), *CueTag));
	}
	return bSuccess;
}

bool UGameplayCueToolset::RemoveCueTag(const FString& CueTag)
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
	TSharedPtr<FGameplayTagNode> TagNode = TagManager.FindTagNode(FName(*CueTag));

	if (!TagNode.IsValid())
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("Gameplay cue tag '%s' does not exist."), *CueTag));
		return false;
	}

	IGameplayTagsEditorModule& EditorModule =
		FModuleManager::LoadModuleChecked<IGameplayTagsEditorModule>("GameplayTagsEditor");

	const bool bSuccess = EditorModule.DeleteTagFromINI(TagNode);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("Failed to remove gameplay cue tag '%s'."), *CueTag));
	}
	return bSuccess;
}

TArray<FString> UGameplayCueToolset::FindCueTagsWithoutNotifies()
{
	TArray<FString> AllCueTags = ListCues(FString());
	if (AllCueTags.IsEmpty())
	{
		return {};
	}

	const UGameplayCueSet* CueSet = GetEditorCueSet();
	if (!CueSet)
	{
		return {};
	}

	TSet<FName> TagsWithNotifies;
	TagsWithNotifies.Reserve(CueSet->GameplayCueData.Num());
	for (const FGameplayCueNotifyData& Notify : CueSet->GameplayCueData)
	{
		if (Notify.GameplayCueTag.IsValid())
		{
			TagsWithNotifies.Add(Notify.GameplayCueTag.GetTagName());
		}
	}

	// ListCues returns sorted tags, so filtering in order keeps the result sorted. A find-only lookup
	// doesn't add names to the name table, and a name that doesn't exist can't be in the set.
	AllCueTags.RemoveAll([&TagsWithNotifies](const FString& Tag)
	{
		return TagsWithNotifies.Contains(FName(*Tag, FNAME_Find));
	});
	return AllCueTags;
}

bool UGameplayCueToolset::IsToolsetHidden_Implementation(const TScriptInterface<IToolsetPermissionProvider>& Provider) const
{
	return Provider->IsRestricted();
}

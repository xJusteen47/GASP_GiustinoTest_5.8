// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Math/Vector.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UObject/ScriptInterface.h"

#include "GameplayCueToolset.generated.h"

/// Describes a registered gameplay cue and its associated notify asset.
USTRUCT(BlueprintType)
struct FGameplayCueInfo
{
	GENERATED_BODY()

	/// The fully-qualified gameplay cue tag (e.g. "GameplayCue.Character.Death").
	UPROPERTY(BlueprintReadWrite, Category = "GameplayCue")
	FString Tag;

	/// The object path of the GameplayCueNotify Blueprint asset registered for exactly this tag
	/// (e.g. "/Game/Effects/GCN_Death.GCN_Death"). Empty if the tag has no notify of its own.
	UPROPERTY(BlueprintReadWrite, Category = "GameplayCue")
	FString NotifyAssetPath;
};

/// Provides tools for inspecting, executing, and managing gameplay cues and their notify assets.
UCLASS(BlueprintType, Hidden)
class UGameplayCueToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns gameplay cue tags registered in the project.
	 * @param ParentTag If non-empty, only cues descending from this tag are returned.
	 *   For example, passing "GameplayCue.Character" returns all cues under that namespace.
	 *   Pass an empty string to return all tags under the "GameplayCue" root.
	 * @return A sorted list of fully-qualified gameplay cue tag names.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GameplayCues")
	static TArray<FString> ListCues(const FString& ParentTag);

	/**
	 * Returns information about a specific gameplay cue, including its notify asset. Only notifies
	 * in the project's gameplay cue notify paths are known.
	 * @param CueTag The fully-qualified gameplay cue tag, e.g. "GameplayCue.Character.Death".
	 * @return The cue's tag and notify asset path.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GameplayCues")
	static FGameplayCueInfo GetCueInfo(const FString& CueTag);

	/**
	 * Executes a gameplay cue non-replicated on the currently selected actor in the editor.
	 * Useful for previewing cue effects without network replication.
	 * Requires a PIE session or a configured GameplayCueManager to produce visible results.
	 * @param CueTag The fully-qualified tag of the cue to execute, e.g. "GameplayCue.Character.Death".
	 * @param NormalizedMagnitude A normalized (0.0-1.0) magnitude value passed to the cue.
	 * @param Location World-space location parameter passed to the cue.
	 * @param Normal World-space direction parameter passed to the cue.
	 * @return True if the cue was dispatched.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Execute"), Category = "GameplayCues")
	static bool ExecuteCueOnSelectedActor(
		const FString& CueTag, float NormalizedMagnitude, FVector Location, FVector Normal);

	/**
	 * Creates a new GameplayCueNotify Blueprint asset at the specified content browser location.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param CueTag The gameplay cue tag the notify should respond to.
	 *   The tag must already exist; create it first with AddCueTag if needed.
	 * @param PackagePath The content browser folder for the asset, e.g. "/Game/Effects/Cues".
	 *   Must be in one of the project's gameplay cue notify paths.
	 * @param AssetName The file name for the new asset, e.g. "GCN_CharacterDeath".
	 * @param bIsActor If true, creates a GameplayCueNotify_Actor (spawned actor in the world).
	 *   If false, creates a GameplayCueNotify_Static (instant effect, no spawned actor).
	 * @return The object path of the created asset, or an empty string on failure.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GameplayCues")
	static FString CreateCueNotifyAsset(
		const FString& CueTag, const FString& PackagePath,
		const FString& AssetName, bool bIsActor);

	/**
	 * Adds a new gameplay cue tag to the project.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param CueTag The fully-qualified tag to add. Must begin with "GameplayCue."
	 *   (e.g. "GameplayCue.Character.Death").
	 * @param Comment An optional developer comment describing the cue's purpose.
	 * @return True if the tag was added successfully.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GameplayCues")
	static bool AddCueTag(const FString& CueTag, const FString& Comment = TEXT(""));

	/**
	 * Removes a gameplay cue tag from the project.
	 * This should ONLY be called after getting explicit direction or permission from the user.
	 * @param CueTag The fully-qualified cue tag to remove (e.g. "GameplayCue.Character.Death").
	 * @return True if the tag was removed successfully.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GameplayCues")
	static bool RemoveCueTag(const FString& CueTag);

	/**
	 * Returns gameplay cue tags that have no GameplayCueNotify of their own.
	 * @return A sorted list of gameplay cue tag names.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GameplayCues")
	static TArray<FString> FindCueTagsWithoutNotifies();

	//~ Begin UToolsetDefinition Interface
	virtual bool IsToolsetHidden_Implementation(const TScriptInterface<IToolsetPermissionProvider>& Provider) const override;
	//~ End UToolsetDefinition Interface

private:
	friend class FGameplayCueToolsetSpec;
};

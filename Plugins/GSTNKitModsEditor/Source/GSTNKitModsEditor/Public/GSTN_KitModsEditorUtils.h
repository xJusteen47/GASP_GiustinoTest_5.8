// Editor tooling of kit mods (M11 L8). Editor only.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_KitModsEditorUtils.generated.h"

class UGSTN_CombatTimeline;
class UGSTN_KitModDefinition;
class UGSTN_PrimaryKitDefinition;

/**
 * The mod preview (spec 15 "preview a timeline with a mod list applied"): the derived timeline of one move, built as the loadout compile
 * bakes it, put in a /Temp asset and opened in the M5 timeline editor, which shows its phases, hitboxes and montage like any timeline.
 * Open the base next to it to compare. The preview is a copy: edits to it are saved nowhere (the mod and the base are the sources), and
 * building it again replaces it. Reachable from the mod's context menu in the Content Browser (Preview on kit), the console
 * (GSTN.KitMods.Preview) and editor scripts. Development only.
 */
UCLASS()
class GSTNKITMODSEDITOR_API UGSTN_KitModsEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The commands of the moves the mods change on the kit (kit bindings first, then the primary weapon), in compile order */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Mods|Editor", meta = (DevelopmentOnly))
	static TArray<FGameplayTag> GetModdedCommands(UGSTN_PrimaryKitDefinition* Kit, const TArray<UGSTN_KitModDefinition*>& Mods);

	/**
	 * The preview timeline of the first modded move bound to Command, in /Temp/GSTNModPreview (named like the compiled derived timeline).
	 * Null, with OutMessage saying why, when the mods change no move on that command
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Mods|Editor", meta = (DevelopmentOnly))
	static UGSTN_CombatTimeline* MakePreviewTimeline(UGSTN_PrimaryKitDefinition* Kit, const TArray<UGSTN_KitModDefinition*>& Mods, FGameplayTag Command, FString& OutMessage);

	/** MakePreviewTimeline, then opens it in the timeline editor */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Mods|Editor", meta = (DevelopmentOnly))
	static bool OpenPreview(UGSTN_PrimaryKitDefinition* Kit, const TArray<UGSTN_KitModDefinition*>& Mods, FGameplayTag Command, FString& OutMessage);

	/** GSTN.KitMods.Preview KitPath ModPath[+ModPath...] CommandTag */
	static void RunConsoleCommand(const TArray<FString>& Args);
};

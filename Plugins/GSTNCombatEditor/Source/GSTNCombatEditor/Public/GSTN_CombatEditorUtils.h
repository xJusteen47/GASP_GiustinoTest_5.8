// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CombatEditorUtils.generated.h"

class UGSTN_CombatTimeline;
class USkeletalMesh;

/**
 * What an Editor Utility Widget or an editor script can ask of the combat tooling (combat-m5-editor 7, plan m5 E8): the tick rate the bake uses,
 * the bake itself and whether a bake is stale. Static, stateless, development only: nothing here exists in a game.
 *
 * Every function works on the asset as it is in memory and writes only through the same code the Bake sockets button runs, so a script and a
 * click give the same frames.
 */
UCLASS()
class GSTNCOMBATEDITOR_API UGSTN_CombatEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Frames per second of the combat simulation: the fixed tick rate of the Network Prediction settings (62.5 Hz today), read without a world.
	 * Never hard-coded; the fallback in the editor settings is used only when the settings object cannot be read
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Editor", meta = (DevelopmentOnly))
	static float GetTickRateHz();

	/**
	 * Bakes the sockets of the timeline from Mesh and stores the frames and the bake record in the asset (marks it dirty; does not save).
	 * False, with OutMessage naming what is wrong (a hitbox that uses a socket the mesh lacks, no length, a montage that does not load), when
	 * nothing was baked: the previous bake is then untouched
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Editor", meta = (DevelopmentOnly))
	static bool BakeTimeline(UGSTN_CombatTimeline* Timeline, USkeletalMesh* Mesh, FString& OutMessage);

	/**
	 * True when a hitbox needs baked frames that are missing, or the frames no longer match their sources (montage, section, play rate, length,
	 * sockets, mesh, tick rate). Mesh is the mesh that would be baked from; null uses the one the last bake recorded
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Editor", meta = (DevelopmentOnly))
	static bool IsBakeStale(UGSTN_CombatTimeline* Timeline, USkeletalMesh* Mesh);
};

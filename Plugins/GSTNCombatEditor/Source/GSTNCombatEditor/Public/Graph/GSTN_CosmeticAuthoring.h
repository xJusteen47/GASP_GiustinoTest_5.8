// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CosmeticAuthoring.generated.h"

class UGSTN_GA_BaseCombat;

/**
 * Lets an editor script set the cosmetic data of a move (CP4 of the Blueprint seams) where Python cannot reach it: the cue tag of the hitboxes
 * (inside the timeline's instanced tracks) and the frames the hitboxes are open (to lay a trail window over them). The move's Cosmetic Windows
 * are a plain property: a script sets them on the class default object. Static, stateless, development only; nothing here saves.
 */
UCLASS()
class GSTNCOMBATEDITOR_API UGSTN_CosmeticAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sets CueTag on every hitbox of the move's inline timeline (the hit's cue: a GCN Combat Hit). Returns the hitboxes changed, -1 when the move
	 * uses a Timeline Asset (change the asset instead) or the class is not a combat move. Marks the Blueprint modified
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Cosmetic Authoring", meta = (DevelopmentOnly))
	static int32 SetHitCueTag(TSubclassOf<UGSTN_GA_BaseCombat> Move, FGameplayTag CueTag);

	/** The first and last frame any hitbox of the move is open (the active frames), false when it has none */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Cosmetic Authoring", meta = (DevelopmentOnly))
	static bool GetActiveFrames(TSubclassOf<UGSTN_GA_BaseCombat> Move, int32& OutFirst, int32& OutLast);
};

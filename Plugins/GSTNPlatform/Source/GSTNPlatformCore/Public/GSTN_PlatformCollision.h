// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_PlatformCollision.generated.h"

/**
 * One line of the table that turns a shape's surface behaviour into collision responses (traversal-surfaces.md 4.1): when the
 * shape's behaviours match the query, its collider blocks these channels. The table lives in the plugin settings, so the channels
 * (the project's) and the behaviours (tags) are inputs, never names in the code (modularity-and-tags 3.7 rule 4).
 */
USTRUCT(BlueprintType)
struct GSTNPLATFORMCORE_API FGSTN_PlatformCollisionRule
{
	GENERATED_BODY()

	/** Matched against the shape's Surface.Behaviour.* tags. An empty query matches nothing (a rule must say when it applies) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	FGameplayTagQuery When;

	/** Channels the collider blocks when the rule matches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	TArray<TEnumAsByte<ECollisionChannel>> BlockChannels;
};

namespace GSTNPlatformCollision
{
	/**
	 * The shipped table (traversal-surfaces.md 4.1):
	 *  - Stand, WallSlide, WallClimb or Launch: block the Mover capsule's channels (a character stands on, slides on or lands on it);
	 *  - WallSlide or WallClimb: also block the channels the wall modes probe on;
	 *  - Stand or Launch: also block the extra channels (the depth marker's trace, depth-marker.md 2.8, a project setting).
	 * A shape with none of these behaviours ignores the characters. The wall modes of this project probe on the capsule's own
	 * object type (Pawn), so the first two rules block the same channel by default: what keeps a Stand top from being grabbed as
	 * a wall is the surface the wall entry resolves (no WallSlide), not collision
	 */
	GSTNPLATFORMCORE_API TArray<FGSTN_PlatformCollisionRule> MakeDefaultRules(TConstArrayView<TEnumAsByte<ECollisionChannel>> MoverChannels,
		TConstArrayView<TEnumAsByte<ECollisionChannel>> WallProbeChannels, TConstArrayView<TEnumAsByte<ECollisionChannel>> StandAndLaunchExtraChannels);

	/**
	 * Responses of a collider with this behaviour: Ignore on every channel, then Block on the channels of every matching rule.
	 * True if any channel blocks (the collider has a reason to exist for queries)
	 */
	GSTNPLATFORMCORE_API bool DeriveResponses(const FGSTN_SurfaceBehaviour& Behaviour, TConstArrayView<FGSTN_PlatformCollisionRule> Rules,
		FCollisionResponseContainer& OutResponses);
}

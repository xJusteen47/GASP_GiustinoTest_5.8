// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_HitReachUtils.generated.h"

/**
 * Reach scaling of hitboxes along their authored ReachAxis (combat-loadout-mods 8, M11 L4): the "longer sword" of a kit mod.
 *
 * Baked, never evaluated in the simulation: the loadout compiler (M11 L5, derived timelines) calls this on a copy of the base timeline
 * and registers the result, so the rollback simulation only ever reads static data. Same inputs give the same bits on every machine
 * (double math on authored values only, no state).
 *
 * The rule, per hitbox whose ReachAxis is not None (the others are left byte-identical):
 * - The shape's centre, in the hitbox's space (the root, the baked socket, the start-of-move frame), has its coordinate along the
 *   axis multiplied by the scale. A blade authored from the grip socket along +X therefore runs from R*a to R*b instead of a to b.
 * - With bScaleExtentWithReach the extent along that axis is multiplied too, so the stretched blade has no gap; without it the
 *   shape keeps its size and only moves: a tip sweetspot follows the new tip (owner, 2026-10-23). The sweet/sour choice between
 *   overlapping hitboxes of a group stays M4's priority rule, untouched.
 * - With keys the change is written into every key (offset and scale), because each key may turn the shape; without keys into the
 *   shape's Local transform. The extent factor is exact when the axis lies along one of the shape's axes (the authored case: a
 *   capsule along the blade) and blends by the axis' components otherwise.
 * - Projectile hitboxes are skipped: the projectile owns its shape after the spawn.
 *
 * The scale is clamped to [MinReachScale, MaxReachScale] as a last safety net; a kit's own ModLimits (L5) clamp tighter before this.
 * Baked socket tracks are not changed: they are the character's bones, and the blade's length lives in the hitbox offsets from them.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_HitReachUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr float MinReachScale = 0.25f;
	static constexpr float MaxReachScale = 4.f;

	/** Does a reach scale change this hitbox at all? False for ReachAxis None and for projectile hitboxes */
	UFUNCTION(BlueprintPure, Category = "Combat|Hits|Reach")
	static bool IsReachScalable(const FGSTN_HitboxSpec& Spec);

	/** A copy of Spec stretched along its ReachAxis by ReachScale (see the class comment). Unchanged when not scalable or the scale is 1 */
	UFUNCTION(BlueprintPure, Category = "Combat|Hits|Reach")
	static FGSTN_HitboxSpec ScaleHitboxReach(const FGSTN_HitboxSpec& Spec, float ReachScale);

	/** A copy of Data with every hitbox of every hitbox track scaled by ReachScale. OutScaledHitboxes = how many hitboxes changed */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hits|Reach")
	static FGSTN_CombatTimelineData ScaleTimelineReach(const FGSTN_CombatTimelineData& Data, float ReachScale, int32& OutScaledHitboxes);

	/** In place form of ScaleTimelineReach for the loadout compiler; returns how many hitboxes changed */
	static int32 ApplyReachScale(FGSTN_CombatTimelineData& Data, float ReachScale);

	/**
	 * The shape's centre in the hitbox's space at a key (KeyIndex INDEX_NONE or no keys = the Local placement alone). Used by the
	 * validator to tell a hitbox whose reach can never move (centre at the space origin along the axis on every key)
	 */
	static FVector GetCentreInSpace(const FGSTN_HitboxSpec& Spec, int32 KeyIndex);

	/** The unit vector of a reach axis; zero for None */
	static FVector AxisVector(EGSTN_ReachAxis Axis);
};

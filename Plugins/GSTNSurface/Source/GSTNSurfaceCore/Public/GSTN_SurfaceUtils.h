// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceUtils.generated.h"

class FGSTN_SurfaceTable;
class UPhysicalMaterial;
class UPrimitiveComponent;

/**
 * Stateless surface resolution and launch math (traversal-surfaces.md §2.2), safe to call inside the simulation and from
 * Blueprint hooks.
 *
 * Resolution order, first match wins:
 *   1. a UGSTN_SurfaceBehaviourComponent on the hit collider's owner (its PerComponent entry for that collider, else Default);
 *   2. an IGSTN_SurfaceProvider on the hit collider, then on its owner;
 *   3. the physical material table of UGSTN_SurfaceSettings;
 *   4. the settings' Default, so unknown geometry behaves as before the plugin was enabled.
 * An empty SurfaceType falls back to the material table's type, then to the default's. The surface velocity always comes from
 * the provider when there is one, whichever step supplied the behaviour: velocity is a fact of the pose, not configuration.
 *
 * Everything read is static configuration (components, settings, collider mobility) or what a provider answers from replicated
 * state, so a resimulated frame resolves the same surface. Nothing is cached across calls; consumers that probe often cache per
 * collider within their own tick (the wall filter, T5).
 */
UCLASS()
class GSTNSURFACECORE_API UGSTN_SurfaceUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The surface of a sweep or contact hit */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static FGSTN_SurfaceView Resolve(const FHitResult& Hit);

	/** The surface of a collider without a hit (an overlap, a validator): no contact point or normal */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static FGSTN_SurfaceView ResolveComponent(const UPrimitiveComponent* Component, FName BoneName = NAME_None);

	/**
	 * The resolution itself, with the material and the tables given explicitly (what Resolve calls with the settings' table;
	 * tests call it with their own)
	 */
	static FGSTN_SurfaceView ResolveQuery(const FGSTN_SurfaceQuery& Query, const UPhysicalMaterial* Material,
		const FGSTN_SurfaceTable& Table, const FGSTN_SurfaceBehaviour& Default);

	/** The provider query of a hit */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static FGSTN_SurfaceQuery MakeQuery(const FHitResult& Hit);

	/** The physical material of a hit: the one the query returned, else the collider's simple material for that body */
	static const UPhysicalMaterial* GetHitPhysicalMaterial(const FHitResult& Hit);

	/** True if the view's behaviours match the query; an empty query matches everything */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static bool MatchesBehaviours(const FGSTN_SurfaceView& View, const FGameplayTagQuery& Query) { return View.MatchesBehaviours(Query); }

	/** Index of a Surface.Type tag in the settings' table (what a sync state stores), 0 for none */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static uint8 GetSurfaceTypeIndex(FGameplayTag SurfaceType);

	/** Surface.Type tag of an index of the settings' table, empty for 0 or an unknown index */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static FGameplayTag GetSurfaceTypeFromIndex(uint8 Index);

	/**
	 * Unit launch direction in world space. SurfaceTransform is the surface's frame (its local up is the launch face, so
	 * rotating the surface rotates LocalDirection and Tilt launches); ContactNormal is used by SurfaceNormal mode and falls back to
	 * the local up when zero. Scale is ignored
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static FVector ComputeLaunchDirection(const FGSTN_SurfaceLaunchParams& Params, const FTransform& SurfaceTransform,
		const FVector& ContactNormal);

	/** Launch speed for an arrival speed into the surface (the positive speed along the inward normal), never negative */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface")
	static float ComputeLaunchSpeed(const FGSTN_SurfaceLaunchParams& Params, float IncomingSpeed);
};

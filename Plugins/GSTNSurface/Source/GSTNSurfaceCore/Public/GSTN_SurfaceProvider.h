// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceProvider.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UGSTN_SurfaceProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * A collider or an actor that answers for its own surface (traversal-surfaces.md §2.2, step 2 of the resolution order). It is
 * the one interface for impact surfaces (H11, was IGSTN_ImpactSurface) and launch surfaces (H11b, was IGSTN_LaunchSurface);
 * platform shapes implement it on their collider, a level object such as the trampoline may implement it on the actor.
 *
 * Implemented on the hit component wins over the actor. A UGSTN_SurfaceBehaviourComponent on the same actor wins over both,
 * so a designer can override a provider's answer without touching its class.
 *
 * Rollback contract: GetSurfaceBehaviour and GetSurfaceVelocity are called inside the simulation and may run again on a
 * resimulated frame. They answer from static configuration and replicated or confirmed state only (a platform's pose
 * history), never from the live pose of an animated mesh, a timer or a random, and they write nothing.
 */
class GSTNSURFACECORE_API IGSTN_SurfaceProvider
{
	GENERATED_BODY()

public:
	/** What the surface offers at this contact */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Surface")
	FGSTN_SurfaceBehaviour GetSurfaceBehaviour(const FGSTN_SurfaceQuery& Query) const;

	/**
	 * Velocity of the surface at the contact point, for launches with bAddSurfaceVelocity and for consumers that compose base
	 * velocity. Default: zero (a static surface)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Surface")
	FVector GetSurfaceVelocity(const FGSTN_SurfaceQuery& Query) const;

	/**
	 * Asks the surface to break (H11 BreakSurface, the spec's Break()). Called outside the simulation, when a frame is
	 * finalised, never on a resimulated frame. Default: nothing
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Surface")
	void BreakSurface();

	virtual FGSTN_SurfaceBehaviour GetSurfaceBehaviour_Implementation(const FGSTN_SurfaceQuery& Query) const { return FGSTN_SurfaceBehaviour(); }
	virtual FVector GetSurfaceVelocity_Implementation(const FGSTN_SurfaceQuery& Query) const { return FVector::ZeroVector; }
	virtual void BreakSurface_Implementation() {}
};

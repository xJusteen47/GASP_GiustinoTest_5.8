// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_SurfaceTypes.generated.h"

class UPrimitiveComponent;

/** Where a launch from a surface points (traversal-surfaces.md §2.1, M4 H11b). */
UENUM(BlueprintType)
enum class EGSTN_SurfaceLaunchDirection : uint8
{
	/** Along the contact normal of the face that was touched. */
	SurfaceNormal,
	/** Along a fixed direction in the surface's local frame, so rotating the surface rotates the launch. */
	LocalDirection,
	/** The surface's local up tilted by a pitch away from it and a yaw around it. */
	Tilt
};

/** How fast a launch from a surface is (traversal-surfaces.md §2.1, M4 H11b). */
UENUM(BlueprintType)
enum class EGSTN_SurfaceLaunchSpeed : uint8
{
	/** Always the same speed, whatever the arrival speed. */
	Fixed,
	/** The arrival speed into the surface times a restitution, clamped to a range. */
	ScaledByIncoming
};

/**
 * Launch parameters of a surface that has Surface.Behaviour.Launch: the H11b trampoline and the trampoline parts of enemies
 * (T3/T4) share them. Static configuration, identical on every machine: the simulation reads it when it queues the launch move
 * and nothing writes it at runtime.
 *
 * The direction is computed from the surface's own frame (or the contact normal), never from world up, so a trampoline works
 * under any gravity (game-design-direction §2d).
 */
USTRUCT(BlueprintType)
struct GSTNSURFACECORE_API FGSTN_SurfaceLaunchParams
{
	GENERATED_BODY()

	/** Where the launch points */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch")
	EGSTN_SurfaceLaunchDirection DirectionMode = EGSTN_SurfaceLaunchDirection::SurfaceNormal;

	/** Launch direction in the surface's local frame (LocalDirection mode). Normalised when used; must not be zero */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch",
		meta = (EditCondition = "DirectionMode == EGSTN_SurfaceLaunchDirection::LocalDirection", EditConditionHides))
	FVector LocalDirection = FVector::UpVector;

	/** Angle between the launch and the surface's local up (Tilt mode). 0 launches straight along local up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch",
		meta = (EditCondition = "DirectionMode == EGSTN_SurfaceLaunchDirection::Tilt", EditConditionHides,
			ClampMin = "0", ClampMax = "89", ForceUnits = "Degrees"))
	float TiltPitch = 0.f;

	/** Rotation of the tilt around the surface's local up, from its local forward (+X) towards its local right (+Y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch",
		meta = (EditCondition = "DirectionMode == EGSTN_SurfaceLaunchDirection::Tilt", EditConditionHides,
			ClampMin = "-180", ClampMax = "180", ForceUnits = "Degrees"))
	float TiltYaw = 0.f;

	/** How fast the launch is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch")
	EGSTN_SurfaceLaunchSpeed SpeedMode = EGSTN_SurfaceLaunchSpeed::Fixed;

	/** Launch speed (Fixed mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch",
		meta = (EditCondition = "SpeedMode == EGSTN_SurfaceLaunchSpeed::Fixed", EditConditionHides,
			ClampMin = "0", ForceUnits = "cm/s"))
	float Speed = 1500.f;

	/** Arrival speed into the surface multiplied by this (ScaledByIncoming mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch",
		meta = (EditCondition = "SpeedMode == EGSTN_SurfaceLaunchSpeed::ScaledByIncoming", EditConditionHides, ClampMin = "0"))
	float Restitution = 1.f;

	/** Lowest launch speed (ScaledByIncoming mode), so a soft landing still launches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch",
		meta = (EditCondition = "SpeedMode == EGSTN_SurfaceLaunchSpeed::ScaledByIncoming", EditConditionHides,
			ClampMin = "0", ForceUnits = "cm/s"))
	float MinSpeed = 800.f;

	/** Highest launch speed (ScaledByIncoming mode); must not be below MinSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch",
		meta = (EditCondition = "SpeedMode == EGSTN_SurfaceLaunchSpeed::ScaledByIncoming", EditConditionHides,
			ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 3000.f;

	/** Keep the velocity along the surface; otherwise the launch replaces the whole velocity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch")
	bool bKeepTangentialVelocity = true;

	/** Give the air jumps back on launch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch")
	bool bResetAirJumps = true;

	/** Restart the coyote window on launch, so a jump right after the launch still counts as a ground jump */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch")
	bool bResetCoyote = false;

	/**
	 * Add the surface's own velocity at the contact (a trampoline part of a moving enemy). The velocity comes from the surface
	 * provider (FGSTN_SurfaceView::SurfaceVelocity), a sample of replicated poses; a static surface adds nothing
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch")
	bool bAddSurfaceVelocity = true;

	/** Optional tag a kit or ability can react to when this surface launches its owner. Never stored in a sync state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface|Launch")
	FGameplayTag LaunchTag;

	/** Appends a message per invalid field (non-finite values, zero direction, inverted speed range). True if none */
	bool Validate(TArray<FText>& OutErrors) const;
};

/**
 * What a surface offers (traversal-surfaces.md §2.1): the traversal behaviours, the impact class and the launch parameters.
 * Level objects and enemy body parts carry the same struct; no consumer knows which one it is reading.
 *
 * Static configuration: read by the simulation as data identical on all machines and never written at runtime. Its fields are
 * BlueprintReadOnly so a graph cannot change a surface during a match (what changes at runtime is only whether a collider
 * exists for a sweep, traversal-surfaces.md §4).
 */
USTRUCT(BlueprintType)
struct GSTNSURFACECORE_API FGSTN_SurfaceBehaviour
{
	GENERATED_BODY()

	/**
	 * Surface.Behaviour.* leaves this surface offers (WallSlide, WallClimb, Launch, Stand). There is no "NoWallSlide" tag: a
	 * surface without WallSlide forbids it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface", meta = (Categories = "Surface.Behaviour"))
	FGameplayTagContainer Behaviours;

	/** Impact class (Surface.Type.*) for hit impacts. Empty: taken from the physical material table, then the default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface", meta = (Categories = "Surface.Type"))
	FGameplayTag SurfaceType;

	/** Used when Behaviours has Surface.Behaviour.Launch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	FGSTN_SurfaceLaunchParams Launch;

	/** Free identity for kits, abilities and AI queries (a boss "back plate"). Never read by core code */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	FGameplayTagContainer DesignerTags;

	/** True if the surface offers this behaviour (or a child of it) */
	bool HasBehaviour(const FGameplayTag& Behaviour) const { return Behaviours.HasTag(Behaviour); }

	/** Appends a message per invalid field (launch parameters are checked only when the surface launches). True if none */
	bool Validate(TArray<FText>& OutErrors) const;
};

/** Which step of the resolution order produced a surface view (traversal-surfaces.md §2.2). */
UENUM(BlueprintType)
enum class EGSTN_SurfaceSource : uint8
{
	/** A UGSTN_SurfaceBehaviourComponent on the hit component's owner */
	Component,
	/** An IGSTN_SurfaceProvider on the hit component or its owner */
	Provider,
	/** The physical material table of UGSTN_SurfaceSettings */
	PhysicalMaterial,
	/** Nothing marked the surface: the settings' default, so unknown geometry behaves as before the plugin */
	Default
};

/**
 * What a provider is asked about (traversal-surfaces.md §2.2): enough for one provider to answer per face or per collider.
 * Built from a hit by UGSTN_SurfaceUtils; a provider must answer from static configuration only.
 */
USTRUCT(BlueprintType)
struct GSTNSURFACECORE_API FGSTN_SurfaceQuery
{
	GENERATED_BODY()

	/** The collider that was hit */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	TObjectPtr<UPrimitiveComponent> Component = nullptr;

	/** Name of the collider that was hit, so a provider can answer per collider without comparing pointers */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FName ComponentName;

	/** Bone or body of the hit, if the collider has several */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FName BoneName;

	/** World contact point */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FVector ContactPoint = FVector::ZeroVector;

	/** World contact normal of the touched face */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FVector ContactNormal = FVector::ZeroVector;
};

/**
 * The resolved surface of a contact (traversal-surfaces.md §2.2): what every consumer reads. A const view for one use; no
 * consumer keeps it across frames, and nothing in it is simulation state (the simulation stores only SurfaceTypeIndex).
 */
USTRUCT(BlueprintType)
struct GSTNSURFACECORE_API FGSTN_SurfaceView
{
	GENERATED_BODY()

	/** Surface.Behaviour.* leaves the surface offers */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FGameplayTagContainer Behaviours;

	/** Impact class after the fallbacks (behaviour, then physical material, then default). May be empty if nothing names one */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FGameplayTag SurfaceType;

	/** Index of SurfaceType in the surface type table, 0 when empty: what a sync state stores instead of the tag */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	uint8 SurfaceTypeIndex = 0;

	/** Launch parameters, meaningful when Behaviours has Surface.Behaviour.Launch */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FGSTN_SurfaceLaunchParams Launch;

	/** Designer identity of the surface */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FGameplayTagContainer DesignerTags;

	/**
	 * Velocity of the surface at the contact, given by its provider from replicated poses (a platform shape's pose history).
	 * Zero for anything that is not a provider: the resolution never reads a live component's velocity
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FVector SurfaceVelocity = FVector::ZeroVector;

	/** The hit collider is Movable (a platform shape, a moving wall). Static configuration of the collider */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	bool bMoving = false;

	/** Which step of the resolution order produced this view */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	EGSTN_SurfaceSource Source = EGSTN_SurfaceSource::Default;

	/** True if the surface offers this behaviour (or a child of it) */
	bool HasBehaviour(const FGameplayTag& Behaviour) const { return Behaviours.HasTag(Behaviour); }

	/** True if the behaviours match the query; an empty query matches everything (the consumers' "no filter" default) */
	bool MatchesBehaviours(const FGameplayTagQuery& Query) const { return Query.IsEmpty() || Query.Matches(Behaviours); }
};

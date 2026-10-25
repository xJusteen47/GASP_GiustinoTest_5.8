// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/Interface.h"
#include "GSTN_PlatformColliders.generated.h"

class UGSTN_EnemyPlatformComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_PlatformCollider : public UInterface
{
	GENERATED_BODY()
};

/**
 * A primitive created by UGSTN_EnemyPlatformComponent for one shape of its spec: which platform and which shape id it is.
 * Implemented by the three collider classes below (one per primitive, because the engine's box, sphere and capsule components
 * are separate classes and the surface resolution looks for the provider interface on the hit component's class).
 */
class GSTNPLATFORMRUNTIME_API IGSTN_PlatformCollider
{
	GENERATED_BODY()

public:
	virtual UGSTN_EnemyPlatformComponent* GetPlatform() const = 0;
	virtual int32 GetShapeId() const = 0;

	/** The platform and shape id of a component if it is a platform collider */
	static bool Find(const UPrimitiveComponent* Component, UGSTN_EnemyPlatformComponent*& OutPlatform, int32& OutShapeId);
};

/** Who a platform collider belongs to. Set once when the platform creates it */
USTRUCT()
struct GSTNPLATFORMRUNTIME_API FGSTN_PlatformColliderBinding
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UGSTN_EnemyPlatformComponent> Platform;

	UPROPERTY()
	int32 ShapeId = INDEX_NONE;
};

/** Box collider of a platform shape (the flat-topped primitive: Stand shapes use it) */
UCLASS(ClassGroup = (GSTN), NotBlueprintable, meta = (DisplayName = "GSTN Platform Box Collider"))
class GSTNPLATFORMRUNTIME_API UGSTN_PlatformBoxCollider : public UBoxComponent, public IGSTN_PlatformCollider
{
	GENERATED_BODY()

public:
	virtual UGSTN_EnemyPlatformComponent* GetPlatform() const override { return Binding.Platform.Get(); }
	virtual int32 GetShapeId() const override { return Binding.ShapeId; }

	UPROPERTY()
	FGSTN_PlatformColliderBinding Binding;
};

/** Sphere collider of a platform shape */
UCLASS(ClassGroup = (GSTN), NotBlueprintable, meta = (DisplayName = "GSTN Platform Sphere Collider"))
class GSTNPLATFORMRUNTIME_API UGSTN_PlatformSphereCollider : public USphereComponent, public IGSTN_PlatformCollider
{
	GENERATED_BODY()

public:
	virtual UGSTN_EnemyPlatformComponent* GetPlatform() const override { return Binding.Platform.Get(); }
	virtual int32 GetShapeId() const override { return Binding.ShapeId; }

	UPROPERTY()
	FGSTN_PlatformColliderBinding Binding;
};

/** Capsule collider of a platform shape */
UCLASS(ClassGroup = (GSTN), NotBlueprintable, meta = (DisplayName = "GSTN Platform Capsule Collider"))
class GSTNPLATFORMRUNTIME_API UGSTN_PlatformCapsuleCollider : public UCapsuleComponent, public IGSTN_PlatformCollider
{
	GENERATED_BODY()

public:
	virtual UGSTN_EnemyPlatformComponent* GetPlatform() const override { return Binding.Platform.Get(); }
	virtual int32 GetShapeId() const override { return Binding.ShapeId; }

	UPROPERTY()
	FGSTN_PlatformColliderBinding Binding;
};

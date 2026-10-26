// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_PlatformColliders.h"

#include "GSTN_EnemyPlatformComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformColliders)

bool IGSTN_PlatformCollider::Find(const UPrimitiveComponent* Component, UGSTN_EnemyPlatformComponent*& OutPlatform, int32& OutShapeId)
{
	OutPlatform = nullptr;
	OutShapeId = INDEX_NONE;
	const IGSTN_PlatformCollider* Collider = Cast<IGSTN_PlatformCollider>(const_cast<UPrimitiveComponent*>(Component));
	if (!Collider || !Collider->GetPlatform())
	{
		return false;
	}
	OutPlatform = Collider->GetPlatform();
	OutShapeId = Collider->GetShapeId();
	return true;
}

FGSTN_SurfaceBehaviour FGSTN_PlatformColliderBinding::GetSurfaceBehaviour() const
{
	const UGSTN_EnemyPlatformComponent* Owner = Platform.Get();
	return Owner ? Owner->GetShapeBehaviour(ShapeId) : FGSTN_SurfaceBehaviour();
}

FVector FGSTN_PlatformColliderBinding::GetSurfaceVelocity(const FGSTN_SurfaceQuery& Query) const
{
	const UGSTN_EnemyPlatformComponent* Owner = Platform.Get();
	return Owner ? Owner->GetSurfaceVelocityAt(ShapeId, Query.ContactPoint) : FVector::ZeroVector;
}

void FGSTN_PlatformColliderBinding::BreakSurface() const
{
	if (UGSTN_EnemyPlatformComponent* Owner = Platform.Get())
	{
		Owner->OnShapeBreakRequested(ShapeId);
	}
}

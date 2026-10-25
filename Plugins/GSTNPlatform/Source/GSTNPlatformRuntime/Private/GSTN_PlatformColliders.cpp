// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_PlatformColliders.h"

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

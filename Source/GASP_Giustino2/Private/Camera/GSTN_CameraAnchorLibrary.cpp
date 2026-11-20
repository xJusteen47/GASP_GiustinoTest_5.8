#include "Camera/GSTN_CameraAnchorLibrary.h"

#include "Camera/GSTN_CameraAnchor.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_CameraAnchorLibrary)

bool UGSTN_CameraAnchorLibrary::GetCameraPivot(const AActor* Character, FVector& Location)
{
	return UGSTN_CameraAnchorComponent::ComputeStandingRoot(Character, Location);
}

FVector UGSTN_CameraAnchorLibrary::GetDampedCameraPivot(const AActor* Character, bool& bDamped)
{
	const UGSTN_CameraAnchorComponent* Anchor = Character ? Character->FindComponentByClass<UGSTN_CameraAnchorComponent>() : nullptr;
	bDamped = Anchor && Anchor->IsActiveForOwner() && Anchor->HasDampedAnchor();
	if (bDamped)
	{
		return Anchor->GetDampedAnchor();
	}
	FVector Location;
	UGSTN_CameraAnchorComponent::ComputeStandingRoot(Character, Location);
	return Location;
}

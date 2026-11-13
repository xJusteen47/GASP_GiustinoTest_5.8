// The generic subject adapters (camera.md §8): any actor, and a character with a CharacterMovementComponent.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraSubjectAdapters.generated.h"

/** Any actor: its location is the pivot, world Z is Up, its velocity is the actor velocity */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraSubjectAdapter_Actor : public UGSTN_CameraSubjectAdapter
{
	GENERATED_BODY()

public:
	virtual bool CanRead(const AActor* Actor) const override { return Actor != nullptr; }
	virtual int32 GetPriority() const override { return 0; }
	virtual bool Read(const AActor* Actor, FGSTN_CameraSubjectState& OutState) const override;
};

/** A character: the capsule point is the pivot, Up opposes the movement component's gravity direction, grounded from the movement mode */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraSubjectAdapter_Character : public UGSTN_CameraSubjectAdapter
{
	GENERATED_BODY()

public:
	virtual bool CanRead(const AActor* Actor) const override;
	virtual int32 GetPriority() const override { return 10; }
	virtual bool Read(const AActor* Actor, FGSTN_CameraSubjectState& OutState) const override;
};

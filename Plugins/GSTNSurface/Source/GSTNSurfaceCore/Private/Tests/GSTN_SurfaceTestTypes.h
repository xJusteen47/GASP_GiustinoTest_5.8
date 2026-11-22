// Surface providers used only by the GSTNSurface.Core automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_SurfaceProvider.h"
#include "GSTN_SurfaceTestTypes.generated.h"

/** A collider that answers for itself, as a platform shape does */
UCLASS(Hidden)
class UGSTN_TestSurfaceProviderBox : public UBoxComponent, public IGSTN_SurfaceProvider
{
	GENERATED_BODY()

public:
	FGSTN_SurfaceBehaviour Answer;
	FVector Velocity = FVector::ZeroVector;

	virtual FGSTN_SurfaceBehaviour GetSurfaceBehaviour_Implementation(const FGSTN_SurfaceQuery& Query) const override { return Answer; }
	virtual FVector GetSurfaceVelocity_Implementation(const FGSTN_SurfaceQuery& Query) const override { return Velocity; }
};

/** An actor that answers for all its colliders, as a level object may */
UCLASS(Hidden)
class AGSTN_TestSurfaceProviderActor : public AActor, public IGSTN_SurfaceProvider
{
	GENERATED_BODY()

public:
	FGSTN_SurfaceBehaviour Answer;

	virtual FGSTN_SurfaceBehaviour GetSurfaceBehaviour_Implementation(const FGSTN_SurfaceQuery& Query) const override { return Answer; }
};

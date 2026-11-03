// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_PlatformSpec.h"
#include "GSTN_PlatformDefinition.generated.h"

/**
 * A fighter's platform spec as a data asset (many-enemies-replication.md 4.2): a designer gives an enemy its shapes without C++
 * and several enemy Blueprints share one. Read through UGSTN_EnemyPlatformComponent::Definition. Static data the simulation
 * reads, so it must be identical on every machine of a match.
 */
UCLASS(BlueprintType, meta = (DisplayName = "GSTN Platform Definition"))
class GSTNPLATFORMRUNTIME_API UGSTN_PlatformDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (ShowOnlyInnerProperties))
	FGSTN_PlatformSpec Spec;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

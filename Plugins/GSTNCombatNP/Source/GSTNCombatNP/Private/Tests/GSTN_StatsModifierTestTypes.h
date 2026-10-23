// Movement modes used only by the stats automation tests of the NP module.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "GSTN_StatsModifierTestTypes.generated.h"

/** A walking mode like the Blueprint ones of the project: gait speed variables, and a MaxSpeedOverride that its archetype sets */
UCLASS(Hidden)
class UGSTN_TestGaitWalkingMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float MaxSpeedOverride = 375.f;

	UPROPERTY()
	float WalkSpeed = 165.f;

	UPROPERTY()
	float RunSpeed = 375.f;

	UPROPERTY()
	float SprintSpeed = 585.f;
};

/** A mode whose archetype leaves the override at -1, which means "read the shared settings" */
UCLASS(Hidden)
class UGSTN_TestPlainMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float MaxSpeedOverride = -1.f;
};

// Movement modes used only by the stats automation tests of the NP module.

#pragma once

#include "CoreMinimal.h"
#include "Effects/GSTN_GE_WeaponStats.h"
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

/** Weapon stats effects of the swap test: +300 and -300 on the walk speed */
UCLASS(Hidden)
class UGSTN_TestWeaponStatsPlus : public UGSTN_GE_WeaponStats
{
	GENERATED_BODY()

public:
	UGSTN_TestWeaponStatsPlus();
};

UCLASS(Hidden)
class UGSTN_TestWeaponStatsMinus : public UGSTN_GE_WeaponStats
{
	GENERATED_BODY()

public:
	UGSTN_TestWeaponStatsMinus();
};

// Object with the property types the stats scaling reads, used only by the Core automation tests.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GSTN_StatsTestTypes.generated.h"

UCLASS(Hidden)
class UGSTN_TestSpeedHolder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float RunSpeed = 0.f;

	UPROPERTY()
	double SprintSpeed = 0.0;

	UPROPERTY()
	float Unlisted = 0.f;

	UPROPERTY()
	FString Label;
};

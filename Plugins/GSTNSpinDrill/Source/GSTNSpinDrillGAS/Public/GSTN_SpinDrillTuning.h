// Spin and drill, ability-system half.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_SpinDrillTypes.h"
#include "Engine/DataAsset.h"
#include "GSTN_SpinDrillTuning.generated.h"

/**
 * Tuning of the spin and drill, one asset per kit that has the feature (spin-drill.md 6). Data only: the groups are the Core
 * structs, and the abilities copy the values they need into their activation data, so the simulation never reads this asset at
 * tick time. Referenced softly by the kit extension (later checkpoint); the target side is FGSTN_DrillTargetResponse, not here.
 */
UCLASS(BlueprintType)
class GSTNSPINDRILLGAS_API UGSTN_SpinDrillTuning : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpinDrill|AirSpin")
	FGSTN_AirSpinTuning AirSpin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpinDrill|Cancel")
	FGSTN_SpinCancelTuning Cancel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpinDrill|Dive")
	FGSTN_DiveTuning Dive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpinDrill|Bore")
	FGSTN_BoreTuning Bore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpinDrill|Launch")
	FGSTN_DrillLaunchTuning Launch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpinDrill|Wobble")
	FGSTN_DrillWobbleTuning Wobble;
};

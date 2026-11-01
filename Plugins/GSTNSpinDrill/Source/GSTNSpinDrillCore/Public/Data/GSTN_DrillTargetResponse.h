// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_DrillTargetResponse.generated.h"

/**
 * What a target changes about the drill (spin-drill.md 6 revised by 13b). Whether a part is drillable is NOT a flag here: it is
 * balance, decided by the tip's penetration against the part's hardness (combat-parts 3.4). The response keeps only overrides and
 * the special-case refusal.
 *
 * Copied into the per-step snapshot by the drill snapshot extension (NP module), so the attacker decides from snapshot data only.
 * It must stay a small POD: the lag-compensation history gives each extension 20 bytes (checked below).
 */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_DrillTargetResponse
{
	GENERATED_BODY()

	/** Multiplier of the bore's SeatFrames on this target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Target", meta = (ClampMin = "0", ClampMax = "4"))
	float SeatFramesScale = 1.f;

	/** Longest bore on this target, frames; 0 = the bore tuning's MaxBoreFrames (13b: may depend on what is drilled) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Target", meta = (ClampMin = "0"))
	int32 MaxBoreFramesOverride = 0;

	/** Multiplier of the bore hits' damage on this target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Target", meta = (ClampMin = "0", ClampMax = "4"))
	float DamagePerHitScale = 1.f;

	/** The target may move while latched (a walking enemy); false for static props */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Target")
	bool bLatchMoves = false;

	/** Never drillable whatever the penetration (special cases); the drill bounces as off a hittable target (7.6) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Target")
	bool bNeverDrillable = false;
};

static_assert(sizeof(FGSTN_DrillTargetResponse) <= 20, "The drill target response must fit the 20 byte snapshot extension slot");

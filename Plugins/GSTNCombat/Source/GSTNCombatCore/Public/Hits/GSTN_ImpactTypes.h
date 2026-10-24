// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_ImpactTypes.generated.h"

/** What a launched victim undergoes on a surface (combat-m4-hits 7b) */
UENUM(BlueprintType)
enum class EGSTN_ImpactOutcome : uint8
{
	/** Slide or land as without impacts */
	None,
	/** Pinned to the surface for a number of frames */
	Splat,
	/** Reflected about the contact normal */
	Bounce,
	/** The surface object is destroyed, the victim continues */
	BreakSurface
};

/**
 * The attack's say in a surface impact: a finisher that forces a slam, a jab that never splats. The victim and surface layers
 * and the resolution (H3) are not part of this struct.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactProfile
{
	GENERATED_BODY()

	/** Outcome the attack asks for; None = no preference */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	EGSTN_ImpactOutcome Force = EGSTN_ImpactOutcome::None;

	/** Outcomes this attack never causes (an enum, since outcomes are not tags) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	TArray<EGSTN_ImpactOutcome> Forbid;

	/** Scales the impact speed the contact is judged with */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float SpeedScale = 1.f;
};

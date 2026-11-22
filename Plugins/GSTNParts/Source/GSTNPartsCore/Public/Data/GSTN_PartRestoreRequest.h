// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PartRestoreRequest.generated.h"

/**
 * An explicit restore of broken parts (a healer enemy, a reset; parts never regenerate by themselves). Plain data: how it travels to the
 * victim's tick is the NP half's business (an infinite effect carrying the restore component sits on the ability system until the parts
 * tick extension consumes it), what it means is UGSTN_PartMath::RestoreParts.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartRestoreRequest
{
	GENERATED_BODY()

	/** Bit i = part i; 0 = every part. The broken ancestors of the masked parts are restored with them (a child cannot stay intact under a broken parent) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part Restore", meta = (ClampMin = 0, ClampMax = 65535))
	int32 PartMask = 0;

	/** Damage a restored part keeps, as a fraction of its cap: 0 = full repair. A restore never makes a part worse than it was */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part Restore", meta = (ClampMin = 0, ClampMax = 1))
	float DamageFraction = 0.f;

	/** Simulation frame the request was consumed on (filled by the consumer, not by the sender); -1 = not consumed yet */
	UPROPERTY(BlueprintReadOnly, Category = "Part Restore")
	int32 Frame = -1;

	uint16 GetMask16() const { return static_cast<uint16>(FMath::Clamp(PartMask, 0, 0xFFFF)); }
};

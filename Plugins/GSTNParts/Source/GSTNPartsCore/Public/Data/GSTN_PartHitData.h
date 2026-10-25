// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
#include "GSTN_PartHitData.generated.h"

/** Switches of a hit's part data (combat-parts.md §3.2) */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EGSTN_PartHitFlags : uint8
{
	None = 0 UMETA(Hidden),
	/** The hit takes the top penetration band whatever the hardness (a magic or piercing attack) */
	IgnoreHardness = 1 << 0,
	/** The hit damages the main pool only and leaves the part's own damage alone */
	MainOnly = 1 << 1
};
ENUM_CLASS_FLAGS(EGSTN_PartHitFlags);

/**
 * What a hit says about parts, authored per hitbox (combat-parts.md §3.2). The hit's penetration against the hardness of the
 * part it lands on picks a band of the penetration table. A hit that carries none of this has penetration 0 and scales of 1,
 * so unannotated attacks still work, mostly ricocheting off hard parts.
 *
 * There is no stagger field: the stagger force is the hit's own, one source of truth that also works without parts.
 *
 * An entry of the hit data's extension array (FGSTN_HitData::ExtensionData): it derives from the hit extension base of the hit
 * pipeline, so the combat core carries it, a relationship variant can replace it by struct type, and the part resolve step finds it
 * by type.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartHitData : public FGSTN_HitExtension
{
	GENERATED_BODY()

	/** Penetration tier 0..15, compared with the part's hardness. Heavy attacks, projectiles and thrown enemies set a higher one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part Hit", meta = (ClampMin = 0, ClampMax = 15))
	uint8 Penetration = 0;

	/** Multiplier of the damage that goes to the part */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part Hit", meta = (ClampMin = 0))
	float PartDamageScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part Hit", meta = (Bitmask, BitmaskEnum = "/Script/GSTNPartsCore.EGSTN_PartHitFlags"))
	uint8 Flags = 0;

	bool HasFlag(EGSTN_PartHitFlags Flag) const { return (Flags & static_cast<uint8>(Flag)) != 0; }
};

// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_PenetrationTable.generated.h"

/**
 * One row of the penetration table (combat-parts.md §3.4): what a hit does when its penetration exceeds a part's hardness by at
 * least MinDelta. Hardness acts on damage only (owner direction §2h), so the stagger and hitstop scales of the damaging bands
 * are 1, and a hit that does not penetrate ricochets instead of damaging.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PenetrationBand
{
	GENERATED_BODY()

	/** The band applies when penetration minus hardness is at least this. Bands are matched from the highest MinDelta down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band")
	int32 MinDelta = 0;

	/** Multiplier of the damage that goes to the part */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band", meta = (ClampMin = 0))
	float PartScale = 1.f;

	/** Multiplier of the damage that goes to the main pool */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band", meta = (ClampMin = 0))
	float MainScale = 1.f;

	/** Multiplier of the hit's stagger force before the part's threshold test. 1 keeps hardness out of hitstun */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band", meta = (ClampMin = 0))
	float StaggerScale = 1.f;

	/** Multiplier of the automatic hitstop of the hit. 1 keeps hardness out of hitstop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band", meta = (ClampMin = 0))
	float HitstopScale = 1.f;

	/**
	 * The hit bounces: no damage, no stagger, no hitstop on either side, no hit confirm. The attacker recoils with a montage
	 * chosen per attack and the victim only gets the spark cue and the Combat.Event.Part.Ricochet event
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band")
	bool bRicochet = false;

	/** Cue played for this band (the ricochet spark...). Empty = the presentation set's default for the kind of band */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band", meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;
};

/**
 * The penetration rule as data (combat-parts.md §3.4): Delta = Penetration - Hardness, both integers, bands sorted by MinDelta
 * descending, the first band with Delta >= MinDelta wins. Changing how armour works is editing this asset, never code.
 *
 * A new table starts as the default of the spec and the owner's direction: above hardness full damage, equal about 65 % (the
 * Helldivers value), below ricochet. Bands are sorted on load and on every edit, and validation rejects an empty table and two
 * bands with the same MinDelta. A table with fewer bands is valid.
 */
UCLASS(BlueprintType)
class GSTNPARTSCORE_API UGSTN_PenetrationTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_PenetrationTable();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration")
	TArray<FGSTN_PenetrationBand> Bands;

	/** The three-band default (above, equal, below ricochets). The cue tags are left empty: they are designer vocabulary */
	static TArray<FGSTN_PenetrationBand> MakeDefaultBands();

	/**
	 * Index of the band a delta falls in: the first band whose MinDelta is at most Delta, or the last band when none is (a
	 * table that does not reach down to the lowest possible delta; validation warns). Bands must be sorted by MinDelta
	 * descending, which Normalize guarantees. INDEX_NONE only for an empty array. Allocation free
	 */
	static int32 LookupBandIndex(TConstArrayView<FGSTN_PenetrationBand> SortedBands, int32 Delta);

	/**
	 * The band of a hit: Delta = Penetration - Hardness (integers). IgnoreHardness forces the top band (index 0), the way a
	 * piercing attack ignores armour. Same preconditions and result as LookupBandIndex. Allocation free
	 */
	static int32 LookupBand(TConstArrayView<FGSTN_PenetrationBand> SortedBands, int32 Penetration, int32 Hardness, bool bIgnoreHardness);

	/** Sorts the bands by MinDelta descending (stable). Called on load and on edit; a table built in code calls it itself */
	void Normalize();

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

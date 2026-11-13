#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PenetrationTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PartBalanceReport.generated.h"

class UGSTN_PartSetDefinition;

/** One part of the balance report. A count of INDEX_NONE means the condition is never reached (or not within the loop cap) */
USTRUCT(BlueprintType)
struct GSTNPARTSEDITOR_API FGSTN_PartBalanceRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Balance")
	int32 PartIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Balance")
	FName Name = NAME_None;

	/** Identical hits on this part from a fresh state until the part is broken */
	UPROPERTY(BlueprintReadOnly, Category = "Balance")
	int32 HitsToBreak = INDEX_NONE;

	/** Identical hits on this part until the damage the main pool takes reaches the pool */
	UPROPERTY(BlueprintReadOnly, Category = "Balance")
	int32 HitsToKill = INDEX_NONE;

	/** 1 when one hit's stagger force, scaled by the band, reaches the part's threshold; INDEX_NONE when it never does */
	UPROPERTY(BlueprintReadOnly, Category = "Balance")
	int32 HitsToStagger = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Balance")
	bool bUnbreakable = false;
};

/**
 * Balance report of a part set against one hit (plan P13, combat-parts.md §8.5 balance explosion risk): how many identical hits
 * break each part, kill through it and stagger it. It runs the real UGSTN_PartMath::ApplyHit on a fresh state per part, so the
 * numbers are the simulation's own, quantisation included. Editor only: a designer's tool, never read by the simulation.
 */
UCLASS()
class GSTNPARTSEDITOR_API UGSTN_PartBalanceReport : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Loop cap of every count; a condition not met within it reads as never */
	static constexpr int32 MaxHits = 10000;

	/**
	 * The report over plain views. MainPool is the damage the main pool takes before the entity dies (negative = the sum of the
	 * breakable parts' caps, the set's own measure of its toughness); StaggerForce is the hit's stagger force before the band's
	 * scale (negative = Damage); BodyStaggerThreshold is the threshold a part with none of its own inherits (negative = none).
	 */
	static TArray<FGSTN_PartBalanceRow> ComputeFromParts(TConstArrayView<FGSTN_PartDefinition> Parts,
		TConstArrayView<FGSTN_PenetrationBand> Bands, const FGSTN_PartHitData& HitData, float Damage, float MainPool = -1.f,
		float StaggerForce = -1.f, float BodyStaggerThreshold = -1.f);

	/** The report for a set. Table null = the set's table, then the default bands */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Parts|Editor")
	static TArray<FGSTN_PartBalanceRow> Compute(const UGSTN_PartSetDefinition* PartSet, const UGSTN_PenetrationTable* Table,
		const FGSTN_PartHitData& HitData, float Damage, float MainPool = -1.f, float StaggerForce = -1.f);

	/** The rows as an aligned text table, one line per part */
	UFUNCTION(BlueprintPure, Category = "GSTN|Parts|Editor")
	static FString FormatReport(const TArray<FGSTN_PartBalanceRow>& Rows);

	/** GSTN.Parts.BalanceReport <PartSetPath> <TablePath|None> <Damage> <Penetration> [StaggerForce] */
	static void RunConsoleCommand(const TArray<FString>& Args);
};

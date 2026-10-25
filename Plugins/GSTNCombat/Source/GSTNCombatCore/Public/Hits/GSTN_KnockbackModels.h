// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "GSTN_KnockbackModels.generated.h"

/**
 * The standard model: the victim's weight decides the strength of the knockback.
 *   K = (BaseKnockback + KnockbackGrowth x Damage) x WeightReference / EffectiveWeight
 * Damage is this hit's, the attack's power. The victim's accumulated damage enters only through WeightScale (the health weight
 * curve), so it is counted once. FixedKnockback, when used, replaces the bracket and the weight still applies.
 */
UCLASS(meta = (DisplayName = "Weight Driven"))
class GSTNCOMBATCORE_API UGSTN_KnockbackModel_WeightDriven : public UGSTN_KnockbackModel
{
	GENERATED_BODY()

protected:
	virtual double ComputeKnockback(const FGSTN_KnockbackInput& Input) const override;
};

/**
 * The platform fighter formula, every constant a property: the damage percent (plus this hit's damage) and the hit's damage
 * grow K, the weight lowers it, then base and growth. Needs a flat health weight curve (the percent is a term of its own).
 *   P = Percent + Damage
 *   K = ((P / PercentDivisor + P x Damage / DamageDivisor) x WeightNumerator / (EffectiveWeight + WeightOffset) x TermScale
 *        + TermBase) x KnockbackGrowth / GrowthDivisor + BaseKnockback) x UnitScale
 * FixedKnockback, when used, is the knockback itself (no percent, no weight), as in the games the formula comes from.
 */
UCLASS(meta = (DisplayName = "Percent Scaling"))
class GSTNCOMBATCORE_API UGSTN_KnockbackModel_PercentScaling : public UGSTN_KnockbackModel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.001"))
	float PercentDivisor = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.001"))
	float DamageDivisor = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.0"))
	float WeightNumerator = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.0"))
	float WeightOffset = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.0"))
	float TermScale = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.0"))
	float TermBase = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.001"))
	float GrowthDivisor = 100.f;

	/** cm/s per unit of the formula */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float UnitScale = 10.f;

	virtual bool UsesPercentTerm() const override { return true; }

protected:
	virtual double ComputeKnockback(const FGSTN_KnockbackInput& Input) const override;

	/** The victim's percent before this hit, as the model counts it */
	virtual double GetPercent(const FGSTN_KnockbackInput& Input) const;
};

/** PercentScaling with the percent clamped to PercentCap (the capped damage mode of combat parts, where percent is a bounded bar) */
UCLASS(meta = (DisplayName = "Capped Percent"))
class GSTNCOMBATCORE_API UGSTN_KnockbackModel_CappedPercent : public UGSTN_KnockbackModel_PercentScaling
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Percent", meta = (ClampMin = "0.0"))
	float PercentCap = 100.f;

protected:
	virtual double GetPercent(const FGSTN_KnockbackInput& Input) const override;
};

/** Hit point games: K is the authored knockback (FixedKnockback when used, else BaseKnockback). No percent, no growth, no weight */
UCLASS(meta = (DisplayName = "Fixed"))
class GSTNCOMBATCORE_API UGSTN_KnockbackModel_Fixed : public UGSTN_KnockbackModel
{
	GENERATED_BODY()

protected:
	virtual double ComputeKnockback(const FGSTN_KnockbackInput& Input) const override;
};

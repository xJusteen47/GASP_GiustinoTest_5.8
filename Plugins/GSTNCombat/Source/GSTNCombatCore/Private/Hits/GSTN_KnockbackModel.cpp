// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_KnockbackModel.h"
#include "Hits/GSTN_KnockbackModels.h"

FGSTN_KnockbackResult UGSTN_KnockbackModel::Compute_Implementation(const FGSTN_KnockbackInput& Input) const
{
	return ResolveRules(Input, ComputeKnockback(Input));
}

double UGSTN_KnockbackModel::GetEffectiveWeight(const FGSTN_KnockbackInput& Input) const
{
	return FMath::Max(static_cast<double>(Input.Weight) * Input.Reaction.WeightScale, static_cast<double>(Tuning.MinWeight));
}

double UGSTN_KnockbackModel::GetNominalKnockback(const FGSTN_HitData& Hit)
{
	return Hit.FixedKnockback >= 0.f ? Hit.FixedKnockback : Hit.BaseKnockback;
}

double UGSTN_KnockbackModel::GetAuthoredBracket(const FGSTN_HitData& Hit)
{
	return Hit.FixedKnockback >= 0.f ? static_cast<double>(Hit.FixedKnockback) : static_cast<double>(Hit.BaseKnockback) + static_cast<double>(Hit.KnockbackGrowth) * Hit.Damage;
}

double UGSTN_KnockbackModel::ComputeStaggerForce(const FGSTN_HitData& Hit) const
{
	const double Authored = Hit.StaggerForceMode == EGSTN_StaggerForceMode::Override
		? static_cast<double>(Hit.StaggerForce)
		: static_cast<double>(Hit.Damage) * Tuning.StaggerPerDamage + GetNominalKnockback(Hit) * Tuning.StaggerPerKnockback;
	return Authored * Hit.StaggerForceScale;
}

FGSTN_KnockbackResult UGSTN_KnockbackModel::ResolveRules(const FGSTN_KnockbackInput& Input, double Knockback) const
{
	const FGSTN_HitData& Hit = Input.Hit;
	const FGSTN_HitReactionModifiers& Modifiers = Input.Reaction;

	FGSTN_KnockbackResult Result;

	// Knockback magnitude after the hurtbox's scale (KnockbackResistance is the velocity's business at hitstop end, not here)
	const double K = FMath::Max(Knockback, 0.0) * Modifiers.KnockbackScale;
	Result.Knockback = static_cast<float>(K);

	// Stagger test: the attack's force against the victim's threshold. Infinite never staggers; unset staggers on every hit
	const double Force = ComputeStaggerForce(Hit);
	Result.StaggerForce = static_cast<float>(Force);
	Result.bStaggered = !Modifiers.bNoFlinch && !Modifiers.StaggerThreshold.bInfinite
		&& Force * Modifiers.StaggerScale >= static_cast<double>(Modifiers.StaggerThreshold.GetFiniteOrZero());

	if (Result.bStaggered)
	{
		// Tier from K, capped by the victim's MaxReaction (the enum is ordered)
		EGSTN_HitReaction Tier = K < Tuning.PushBackThreshold ? EGSTN_HitReaction::StaggerInPlace
			: K < Tuning.LaunchThreshold ? EGSTN_HitReaction::PushBack : EGSTN_HitReaction::Launch;
		Tier = static_cast<EGSTN_HitReaction>(FMath::Min(static_cast<uint8>(Tier), static_cast<uint8>(Modifiers.MaxReaction)));
		Result.Reaction = Tier;

		const FVector Direction = Input.LaunchDirection.GetSafeNormal();
		if (Tier == EGSTN_HitReaction::Launch)
		{
			Result.LaunchVelocity = Direction * K;
			Result.bTumble = K >= Tuning.TumbleThreshold;
		}
		else if (Tier == EGSTN_HitReaction::PushBack)
		{
			// Horizontal on the victim's own plane; a launch straight along the up axis has no horizontal direction to push along
			const FVector Up = Input.Up.GetSafeNormal();
			const FVector Horizontal = (Direction - Up * FVector::DotProduct(Direction, Up)).GetSafeNormal();
			Result.LaunchVelocity = Horizontal * FMath::Min(K * Tuning.PushBackSpeedScale, static_cast<double>(Tuning.MaxPushBackSpeed));
		}

		const double Frames = K * Tuning.HitstunFactor * Hit.HitstunMultiplier * Modifiers.HitstunScale;
		Result.HitstunFrames = FMath::Max(Tuning.MinHitstunFrames, FMath::FloorToInt(Frames));
	}

	// Hitstop applies whether or not the victim staggered (a hurtbox that must not even pause sets HitstopScale to 0)
	const int32 MaxHitstop = FMath::Max(Tuning.MaxHitstop, 0);
	if (Hit.HitstopMode == EGSTN_HitstopMode::Override)
	{
		Result.HitstopFrames = FMath::Clamp(Hit.HitstopFramesOverride, 0, MaxHitstop);
		Result.AttackerHitstopFrames = FMath::Clamp(Hit.GetAttackerHitstopFramesOverride(), 0, MaxHitstop);
	}
	else
	{
		const double BaseFrames = static_cast<double>(Hit.Damage) * Tuning.HitstopPerDamage + K * Tuning.HitstopPerKnockback + Tuning.HitstopBase;
		const int32 Base = FMath::Clamp(FMath::FloorToInt(BaseFrames), 0, MaxHitstop);
		Result.HitstopFrames = FMath::Clamp(FMath::FloorToInt(Base * static_cast<double>(Hit.HitstopMultiplier) * Modifiers.HitstopScale), 0, MaxHitstop);
		Result.AttackerHitstopFrames = FMath::Clamp(FMath::FloorToInt(Base * static_cast<double>(Hit.AttackerHitstopMultiplier) * Modifiers.HitstopScale), 0, MaxHitstop);
	}
	return Result;
}

double UGSTN_KnockbackModel_WeightDriven::ComputeKnockback(const FGSTN_KnockbackInput& Input) const
{
	return GetAuthoredBracket(Input.Hit) * Tuning.WeightReference / GetEffectiveWeight(Input);
}

double UGSTN_KnockbackModel_PercentScaling::GetPercent(const FGSTN_KnockbackInput& Input) const
{
	return FMath::Max(static_cast<double>(Input.DamagePercent), 0.0);
}

double UGSTN_KnockbackModel_PercentScaling::ComputeKnockback(const FGSTN_KnockbackInput& Input) const
{
	const FGSTN_HitData& Hit = Input.Hit;
	if (Hit.FixedKnockback >= 0.f)
	{
		return Hit.FixedKnockback;
	}
	const double Damage = Hit.Damage;
	const double Percent = GetPercent(Input) + Damage;
	const double Term = (Percent / PercentDivisor + Percent * Damage / DamageDivisor) * WeightNumerator / (GetEffectiveWeight(Input) + WeightOffset) * TermScale + TermBase;
	return (Term * Hit.KnockbackGrowth / GrowthDivisor + Hit.BaseKnockback) * UnitScale;
}

double UGSTN_KnockbackModel_CappedPercent::GetPercent(const FGSTN_KnockbackInput& Input) const
{
	return FMath::Clamp(static_cast<double>(Input.DamagePercent), 0.0, static_cast<double>(PercentCap));
}

double UGSTN_KnockbackModel_Fixed::ComputeKnockback(const FGSTN_KnockbackInput& Input) const
{
	return GetNominalKnockback(Input.Hit);
}

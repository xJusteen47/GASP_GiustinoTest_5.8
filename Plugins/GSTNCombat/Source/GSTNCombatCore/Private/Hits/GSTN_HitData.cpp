// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_CombatHitTags.h"

bool FGSTN_HitData::IsNormalHitType() const
{
	return !HitType.IsValid() || HitType == GSTNCombatHitTags::HitType_Normal.GetTag();
}

FGSTN_HitData UGSTN_HitDataUtils::ApplyVariant(const FGSTN_HitData& Hit, const FGSTN_HitVariant& Variant)
{
	FGSTN_HitData Result = Hit;
	const FGSTN_HitDataOverride& Override = Variant.Override;
	const FGSTN_HitData& Values = Override.Values;

#define GSTN_TAKE_FIELD(Field) if (Override.Overrides(EGSTN_HitDataField::Field)) { Result.Field = Values.Field; }
	GSTN_TAKE_FIELD(Damage)
	GSTN_TAKE_FIELD(BaseKnockback)
	GSTN_TAKE_FIELD(KnockbackGrowth)
	GSTN_TAKE_FIELD(FixedKnockback)
	GSTN_TAKE_FIELD(AngleDeg)
	GSTN_TAKE_FIELD(SpecialAngle)
	GSTN_TAKE_FIELD(YawSource)
	GSTN_TAKE_FIELD(HitstunMultiplier)
	GSTN_TAKE_FIELD(HitstopMode)
	GSTN_TAKE_FIELD(HitstopMultiplier)
	GSTN_TAKE_FIELD(AttackerHitstopMultiplier)
	GSTN_TAKE_FIELD(HitstopFramesOverride)
	GSTN_TAKE_FIELD(AttackerHitstopFramesOverride)
	GSTN_TAKE_FIELD(ShieldDamage)
	GSTN_TAKE_FIELD(Priority)
	GSTN_TAKE_FIELD(HitType)
	GSTN_TAKE_FIELD(ExtraEffects)
	GSTN_TAKE_FIELD(CueTag)
	GSTN_TAKE_FIELD(StaggerForceMode)
	GSTN_TAKE_FIELD(StaggerForce)
	GSTN_TAKE_FIELD(StaggerForceScale)
	GSTN_TAKE_FIELD(Impact)
#undef GSTN_TAKE_FIELD

	// Multipliers act on the result of the replacement, in a fixed order
	Result.Damage *= Override.DamageMultiplier;
	Result.BaseKnockback *= Override.KnockbackMultiplier;
	Result.KnockbackGrowth *= Override.KnockbackMultiplier;
	if (Result.FixedKnockback >= 0.f)
	{
		Result.FixedKnockback *= Override.KnockbackMultiplier;
	}
	Result.HitstunMultiplier *= Override.HitstunMultiplier;
	Result.HitstopMultiplier *= Override.HitstopMultiplier;
	Result.AttackerHitstopMultiplier *= Override.HitstopMultiplier;
	Result.ShieldDamage *= Override.ShieldDamageMultiplier;
	Result.StaggerForceScale *= Override.StaggerForceMultiplier;

	// An extension is replaced as a whole by its struct type: the core does not know its fields
	for (const FInstancedStruct& Replacement : Override.ExtensionOverrides)
	{
		const UScriptStruct* Type = Replacement.GetScriptStruct();
		if (!Type)
		{
			continue;
		}
		FInstancedStruct* Existing = Result.ExtensionData.FindByPredicate([Type](const FInstancedStruct& Entry) { return Entry.GetScriptStruct() == Type; });
		if (Existing)
		{
			*Existing = Replacement;
		}
		else
		{
			Result.ExtensionData.Add(Replacement);
		}
	}
	return Result;
}

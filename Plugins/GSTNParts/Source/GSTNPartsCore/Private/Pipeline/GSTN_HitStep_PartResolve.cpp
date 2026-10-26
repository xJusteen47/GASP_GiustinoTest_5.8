// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Pipeline/GSTN_HitStep_PartResolve.h"

#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsRulesExtension.h"
#include "GSTN_PartsSettings.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Math/GSTN_PartMath.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"

namespace
{
	// The part reaction tiers are the hit reaction tiers value for value (the part data predates the combat enum)
	static_assert(static_cast<uint8>(EGSTN_PartReactionTier::StaggerInPlace) == static_cast<uint8>(EGSTN_HitReaction::StaggerInPlace)
		&& static_cast<uint8>(EGSTN_PartReactionTier::PushBack) == static_cast<uint8>(EGSTN_HitReaction::PushBack)
		&& static_cast<uint8>(EGSTN_PartReactionTier::Launch) == static_cast<uint8>(EGSTN_HitReaction::Launch),
		"EGSTN_PartReactionTier must mirror EGSTN_HitReaction");

	EGSTN_HitReaction ToHitReaction(EGSTN_PartReactionTier Tier)
	{
		return static_cast<EGSTN_HitReaction>(static_cast<uint8>(Tier));
	}

	const FGSTN_PartsRulesExtension* FindRulesExtension(const UGSTN_CombatRules* Rules)
	{
		if (Rules)
		{
			for (const FInstancedStruct& Entry : Rules->RulesExtensions)
			{
				if (const FGSTN_PartsRulesExtension* Found = Entry.GetPtr<FGSTN_PartsRulesExtension>())
				{
					return Found;
				}
			}
		}
		return nullptr;
	}

	/** The bands of the hit: the set's table, the rules extension's, the settings', the built-in three. Soft pointers are read, never loaded (content resolves before the first tick) */
	TConstArrayView<FGSTN_PenetrationBand> ResolveBands(const UGSTN_PartSetDefinition& Set, const FGSTN_PartsRulesExtension* RulesExtension)
	{
		if (const UGSTN_PenetrationTable* Table = Set.Table.Get(); Table && !Table->Bands.IsEmpty())
		{
			return Table->Bands;
		}
		if (RulesExtension)
		{
			if (const UGSTN_PenetrationTable* Table = RulesExtension->DefaultPenetrationTable.Get(); Table && !Table->Bands.IsEmpty())
			{
				return Table->Bands;
			}
		}
		if (const UGSTN_PenetrationTable* Table = UGSTN_PartsSettings::Get().DefaultPenetrationTable.Get(); Table && !Table->Bands.IsEmpty())
		{
			return Table->Bands;
		}
		static const TArray<FGSTN_PenetrationBand> BuiltIn = UGSTN_PenetrationTable::MakeDefaultBands();
		return BuiltIn;
	}

	/** What ApplyDamage writes for a hit, with the damage the parts decided */
	void WriteDamage(const FGSTN_HitContextView& View, float Damage, FGSTN_StepResult& Out)
	{
		if (Damage > 0.f)
		{
			Out.DamageAdd = Damage;

			FGSTN_HitRequest& Request = Out.Requests.AddDefaulted_GetRef();
			Request.Kind = EGSTN_HitRequestKind::Damage;
			Request.Target = EGSTN_HitRequestTarget::Victim;
			Request.Effect = View.Rules ? View.Rules->DamageEffect : TSoftClassPtr<UObject>();
			Request.Magnitude = Damage;
		}
		Out.Effects.Append(View.Incoming.Hit.ExtraEffects);
	}
}

EGSTN_StepResult UGSTN_HitStep_PartResolve::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	// A hit that did not land, was blocked or parried, or met an invincible window reaches no part (and no damage: ApplyDamage skips these too)
	if (!View.Outcome.bHit || View.bNoDamage || View.Outcome.bBlocked || View.Outcome.bParried)
	{
		return EGSTN_StepResult::Continue;
	}

	const FGSTN_HitData& Hit = View.Incoming.Hit;
	const FGSTN_PartsSnapshotExt* Ext = FGSTN_PartsSnapshotExt::FromView(View.Victim);
	const UGSTN_PartSetDefinition* Set = Ext ? Ext->PartSet : nullptr;
	if (!Set || Set->Parts.IsEmpty())
	{
		// No part set: the body takes the whole hit, as without the plugin
		WriteDamage(View, Hit.Damage, Out);
		return EGSTN_StepResult::Continue;
	}

	// The part comes from the hurtbox the attacker chose; a hurtbox no part names belongs to the body
	const TConstArrayView<FGSTN_PartDefinition> Parts = Set->GetParts();
	int32 PartIndex = INDEX_NONE;
	if (View.HurtboxSet && View.HurtboxSet->Hurtboxes.IsValidIndex(View.HurtboxIndex))
	{
		PartIndex = Set->FindPartByHurtbox(View.HurtboxSet->Hurtboxes[View.HurtboxIndex].Name);
	}

	const FGSTN_PartHitData* PartHit = nullptr;
	for (const FInstancedStruct& Entry : Hit.ExtensionData)
	{
		if (const FGSTN_PartHitData* Found = Entry.GetPtr<FGSTN_PartHitData>())
		{
			PartHit = Found;
			break;
		}
	}

	const FGSTN_PartsRulesExtension* RulesExtension = FindRulesExtension(View.Rules);
	const FGSTN_PartsState Before = Ext->ToState();
	FGSTN_PartsState After = Before;
	const FGSTN_PartHitResult Result = UGSTN_PartMath::ApplyHit(Parts, ResolveBands(*Set, RulesExtension), After, FGSTN_PartHitInput::Make(Hit.Damage, PartIndex, PartHit));

	FGSTN_PartHitOutcome Outcome;
	Outcome.PartIndex = Result.PartIndex;
	Outcome.BandIndex = Result.BandIndex;
	Outcome.NewPartDamage = Result.NewPartDamage;
	Outcome.NewlyBrokenMask = Result.NewlyBrokenMask;
	Outcome.ResultBrokenMask = After.BrokenMask;
	FMemory::Memcpy(Outcome.ResultPartDamage, After.PartDamage, sizeof(Outcome.ResultPartDamage));
	Outcome.ResultBreakSerial = After.BreakSerial;
	Outcome.MainDelta = Result.MainDelta;
	Outcome.Flags = (Result.bRicochet ? GSTNPartOutcomeFlags::Ricochet : 0)
		| (Result.bPartDamaged ? GSTNPartOutcomeFlags::PartDamaged : 0)
		| (Result.bWeakPointHit ? GSTNPartOutcomeFlags::WeakPointHit : 0)
		| (Result.bKill ? GSTNPartOutcomeFlags::Kill : 0)
		| (Result.bAllBrokenReached ? GSTNPartOutcomeFlags::AllBroken : 0)
		| (Result.bPassedToBody ? GSTNPartOutcomeFlags::PassedToBody : 0);
	Out.OutcomeExtensions.Add(FInstancedStruct::Make(Outcome));

	if (Result.bRicochet)
	{
		// Nothing lands: no damage, no knockback, no freeze on either side, no flinch. The pipeline ends here, but the hit still HAPPENED: the
		// attacker's loop drops a hit the pipeline refused (bHit false) before it asks the attacker outcome seam, and the ricochet is exactly what
		// that seam must see to start the recoil. The entry above is what it reads
		return EGSTN_StepResult::Stop;
	}

	// The modifiers. The weight is read from the state BEFORE the hit (like the health weight: the attacker's prediction cannot know the break)
	const FGSTN_PartReaction& Body = Set->BodyReaction;
	const FGSTN_PartReaction* PartReaction = Parts.IsValidIndex(Result.PartIndex) ? &Parts[Result.PartIndex].Reaction : nullptr;
	const float Progress = View.Rules ? View.Rules->GetDamageProgress(View.Victim) : 0.f;
	const float MinWeightScale = RulesExtension && RulesExtension->MinWeightScale >= 0.f ? RulesExtension->MinWeightScale : UGSTN_PartsSettings::Get().MinWeightScale;

	float Weight = UGSTN_PartMath::EffectiveWeightScale(Parts, Set->GetWeightCurve(), Progress, Before.BrokenMask, MinWeightScale) * Body.JuggleFactor;
	if (PartReaction)
	{
		Weight *= PartReaction->JuggleFactor;
	}
	// No airborne term here: the combat rules' AirborneJuggleScale already scales the knockback of a victim that is tumbling (ComputeKnockback), and
	// the per-part juggle factor above is the "where" of the same dial, so the two multiply with no second global in the parts plugin (A9)
	Out.WeightScale = Weight;

	const FGSTN_PartReaction& Hitting = PartReaction ? *PartReaction : Body;
	Out.KnockbackScale = Hitting.KnockbackScale;
	Out.HitstopScale = Result.HitstopScale * Hitting.HitstopScale;
	Out.StaggerScale = Result.StaggerScale;

	// The hitstun condition: the part's threshold, else the body's, else unset (the hurtbox step's and the rules' value stand)
	if (PartReaction && PartReaction->StaggerThreshold >= 0.f)
	{
		Out.StaggerThreshold.Value = PartReaction->StaggerThreshold;
	}
	else if (Body.StaggerThreshold >= 0.f)
	{
		Out.StaggerThreshold.Value = Body.StaggerThreshold;
	}

	const EGSTN_PartReactionTier Cap = PartReaction ? FMath::Min(PartReaction->MaxReaction, Body.MaxReaction) : Body.MaxReaction;
	Out.MaxReaction = ToHitReaction(Cap);

	WriteDamage(View, Result.MainDelta, Out);
	return EGSTN_StepResult::Continue;
}

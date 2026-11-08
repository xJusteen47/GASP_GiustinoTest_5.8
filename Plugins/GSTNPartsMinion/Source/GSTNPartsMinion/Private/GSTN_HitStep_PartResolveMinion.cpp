// Bridge between parts and minions (plan P16).

#include "GSTN_HitStep_PartResolveMinion.h"

#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsRulesExtension.h"
#include "Data/GSTN_PenetrationTable.h"
#include "GSTN_PartsSettings.h"
#include "HAL/IConsoleManager.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Math/GSTN_PartMath.h"
#include "Misc/ScopeExit.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/ObjectKey.h"

namespace
{
	/**
	 * What one (part set, rules) pair gives every hit and never changes during a run: the pipeline flags the compensation reads, the bands
	 * the parent resolves through soft pointers on each hit, and the minimum weight scale. Keys are TObjectKey (index + serial), so a freed
	 * set whose address is reused never matches a stale entry
	 */
	struct FStepCacheEntry
	{
		TObjectKey<UGSTN_PartSetDefinition> Set;
		TObjectKey<UGSTN_CombatRules> Rules;
		TArray<FGSTN_PenetrationBand> Bands;
		float MinWeightScale = 0.f;
		bool bHasPartResolve = false;
		bool bHasApplyDamage = false;
		bool bHasHealthWeight = false;
	};

	TArray<FStepCacheEntry> GStepCache;
	int32 GStepCacheLast = 0;
	bool GStepCacheEnabled = true;

	FAutoConsoleVariableRef CVarStepCache(
		TEXT("gstn.PartsMinion.StepCache"), GStepCacheEnabled,
		TEXT("1 (default): the minion part step resolves hits on a per (part set, rules) cache; 0: the first, uncached path (reference, for A/B timing)."));

	/** Same lookup the parent step does per hit (its copy is private to GSTNPartsCore): done once per cache entry */
	const FGSTN_PartsRulesExtension* FindRulesExtension(const UGSTN_CombatRules* Rules)
	{
		for (const FInstancedStruct& Entry : Rules->RulesExtensions)
		{
			if (const FGSTN_PartsRulesExtension* Found = Entry.GetPtr<FGSTN_PartsRulesExtension>())
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** Same order as the parent: the set's table, the rules extension's, the settings', the built-in three. Soft pointers are read, never loaded */
	void ResolveBands(const UGSTN_PartSetDefinition& Set, const FGSTN_PartsRulesExtension* RulesExtension, TArray<FGSTN_PenetrationBand>& OutBands)
	{
		if (const UGSTN_PenetrationTable* Table = Set.Table.Get(); Table && !Table->Bands.IsEmpty())
		{
			OutBands = Table->Bands;
			return;
		}
		if (RulesExtension)
		{
			if (const UGSTN_PenetrationTable* Table = RulesExtension->DefaultPenetrationTable.Get(); Table && !Table->Bands.IsEmpty())
			{
				OutBands = Table->Bands;
				return;
			}
		}
		if (const UGSTN_PenetrationTable* Table = UGSTN_PartsSettings::Get().DefaultPenetrationTable.Get(); Table && !Table->Bands.IsEmpty())
		{
			OutBands = Table->Bands;
			return;
		}
		OutBands = UGSTN_PenetrationTable::MakeDefaultBands();
	}

	const FStepCacheEntry& FindOrBuildEntry(const UGSTN_PartSetDefinition& Set, const UGSTN_CombatRules& Rules)
	{
		const TObjectKey<UGSTN_PartSetDefinition> SetKey(&Set);
		const TObjectKey<UGSTN_CombatRules> RulesKey(&Rules);
		if (GStepCache.IsValidIndex(GStepCacheLast) && GStepCache[GStepCacheLast].Set == SetKey && GStepCache[GStepCacheLast].Rules == RulesKey)
		{
			return GStepCache[GStepCacheLast];
		}
		for (int32 Index = 0; Index < GStepCache.Num(); ++Index)
		{
			if (GStepCache[Index].Set == SetKey && GStepCache[Index].Rules == RulesKey)
			{
				GStepCacheLast = Index;
				return GStepCache[Index];
			}
		}

		FStepCacheEntry& Entry = GStepCache.AddDefaulted_GetRef();
		Entry.Set = SetKey;
		Entry.Rules = RulesKey;
		const FGSTN_PartsRulesExtension* RulesExtension = FindRulesExtension(&Rules);
		ResolveBands(Set, RulesExtension, Entry.Bands);
		Entry.MinWeightScale = RulesExtension && RulesExtension->MinWeightScale >= 0.f ? RulesExtension->MinWeightScale : UGSTN_PartsSettings::Get().MinWeightScale;
		for (const TSubclassOf<UGSTN_HitResolutionStep>& StepClass : Rules.GetResolvedHitPipeline())
		{
			if (!StepClass || StepClass->IsChildOf(UGSTN_HitStep_PartResolveMinion::StaticClass()))
			{
				continue;
			}
			Entry.bHasPartResolve |= StepClass->IsChildOf(UGSTN_HitStep_PartResolve::StaticClass());
			Entry.bHasApplyDamage |= StepClass->IsChildOf(UGSTN_HitStep_ApplyDamage::StaticClass());
			Entry.bHasHealthWeight |= StepClass->IsChildOf(UGSTN_HitStep_HealthWeight::StaticClass());
		}
		GStepCacheLast = GStepCache.Num() - 1;
		return Entry;
	}

	EGSTN_HitReaction ToHitReaction(EGSTN_PartReactionTier Tier)
	{
		return static_cast<EGSTN_HitReaction>(static_cast<uint8>(Tier));
	}

	/** The weight scale HealthWeight merged into the pipeline before this slot, evaluated without a scratch step result (1 when it writes none) */
	float HealthWeightScale(const FGSTN_HitContextView& View, float Progress)
	{
		const FGSTN_BakedCurve* Curve = nullptr;
		if (View.HurtboxSet && View.HurtboxSet->WeightCurve.IsAuthored())
		{
			Curve = &View.HurtboxSet->WeightCurve;
		}
		else if (View.Rules->DefaultWeightCurve.IsAuthored())
		{
			Curve = &View.Rules->DefaultWeightCurve;
		}
		return Curve ? Curve->Evaluate(Progress) : 1.f;
	}

	/**
	 * The parent's step plus the compensation of ResolveUncached, in one pass on the cache. Differences from the first path, none of them
	 * visible to the minion batch: no FGSTN_HitRequest is written (the batch has no ability system and never reads Requests; the first path
	 * left one next to ApplyDamage's), no ExtraEffects are copied only to be dropped again when ApplyDamage is listed, and HealthWeight is
	 * evaluated for its number instead of into a second full step result. Every float operation is the same, in the same order
	 */
	EGSTN_StepResult ResolveCached(const FGSTN_HitContextView& View, FGSTN_StepResult& Out, const FGSTN_PartsSnapshotExt& Ext, const UGSTN_PartSetDefinition& Set,
		const FStepCacheEntry& Cache)
	{
		if (!View.Outcome.bHit || View.bNoDamage || View.Outcome.bBlocked || View.Outcome.bParried)
		{
			return EGSTN_StepResult::Continue;
		}

		const FGSTN_HitData& Hit = View.Incoming.Hit;
		const TConstArrayView<FGSTN_PartDefinition> Parts = Set.GetParts();
		int32 PartIndex = INDEX_NONE;
		FName HurtboxName = NAME_None;
		if (View.HurtboxSet && View.HurtboxSet->Hurtboxes.IsValidIndex(View.HurtboxIndex))
		{
			HurtboxName = View.HurtboxSet->Hurtboxes[View.HurtboxIndex].Name;
			PartIndex = Set.FindPartByHurtbox(HurtboxName);
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

		const FGSTN_PartsState Before = Ext.ToState();
		FGSTN_PartsState After = Before;
		FGSTN_PartHitInput Input = FGSTN_PartHitInput::Make(Hit.Damage, PartIndex, PartHit);
		Input.TurnSense = FMath::Clamp(Hit.TurnSense, -1, 1);
		Input.TurnSteps = FMath::Clamp(Hit.TurnSteps, 0, 255);
		if (Parts.IsValidIndex(PartIndex))
		{
			const FGSTN_PartDefinition& Part = Parts[PartIndex];
			if (!Part.HurtboxHardnessOverride.IsEmpty())
			{
				Input.HardnessOverride = Part.GetHardnessFor(HurtboxName);
			}
		}
		const FGSTN_PartHitResult Result = UGSTN_PartMath::ApplyHit(Parts, Cache.Bands, After, Input);

		// Built in place in the result's slot: Make(Outcome) would construct a local, then copy it through the reflected struct
		FInstancedStruct& OutcomeSlot = Out.OutcomeExtensions.AddDefaulted_GetRef();
		OutcomeSlot.InitializeAs<FGSTN_PartHitOutcome>();
		FGSTN_PartHitOutcome& Outcome = OutcomeSlot.GetMutable<FGSTN_PartHitOutcome>();
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

		if (Result.bRicochet)
		{
			return EGSTN_StepResult::Stop;
		}

		const FGSTN_PartReaction& Body = Set.BodyReaction;
		const FGSTN_PartReaction* PartReaction = Parts.IsValidIndex(Result.PartIndex) ? &Parts[Result.PartIndex].Reaction : nullptr;
		const float Progress = View.Rules->GetDamageProgress(View.Victim);

		float Weight = UGSTN_PartMath::EffectiveWeightScale(Parts, Set.GetWeightCurve(), Progress, Before.BrokenMask, Cache.MinWeightScale) * Body.JuggleFactor;
		if (PartReaction)
		{
			Weight *= PartReaction->JuggleFactor;
		}
		Out.WeightScale = Weight;

		const FGSTN_PartReaction& Hitting = PartReaction ? *PartReaction : Body;
		Out.KnockbackScale = Hitting.KnockbackScale;
		Out.HitstopScale = Result.HitstopScale * Hitting.HitstopScale;
		Out.StaggerScale = Result.StaggerScale;

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

		// What the parent wrote (its WriteDamage minus the request)
		if (Result.MainDelta > 0.f)
		{
			Out.DamageAdd = Result.MainDelta;
		}
		if (!Cache.bHasApplyDamage)
		{
			// Nobody else writes the hit's effects: they are the parent's, kept (the first path keeps them too)
			Out.Effects.Append(Hit.ExtraEffects);
		}

		// The compensation of the first path (the parent step returned Continue and the hit is damaging)
		if (Cache.bHasApplyDamage && Hit.Damage > 0.f)
		{
			Out.DamageAdd -= Hit.Damage;
		}
		if (Cache.bHasHealthWeight)
		{
			const float HealthWeight = HealthWeightScale(View, Progress);
			if (HealthWeight > UE_KINDA_SMALL_NUMBER)
			{
				Out.WeightScale /= HealthWeight;
			}
		}
		return EGSTN_StepResult::Continue;
	}
}

void UGSTN_HitStep_PartResolveMinion::InvalidateStepCache()
{
	GStepCache.Reset();
	GStepCacheLast = 0;
}

void UGSTN_HitStep_PartResolveMinion::SetStepCacheEnabled(bool bEnabled)
{
	GStepCacheEnabled = bEnabled;
}

bool UGSTN_HitStep_PartResolveMinion::IsStepCacheEnabled()
{
	return GStepCacheEnabled;
}

FGSTN_PartsMinionStepTiming& UGSTN_HitStep_PartResolveMinion::GetTiming()
{
	static FGSTN_PartsMinionStepTiming Timing;
	return Timing;
}

EGSTN_StepResult UGSTN_HitStep_PartResolveMinion::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	// A minion type without a part set: the rules' own steps already do everything
	const FGSTN_PartsSnapshotExt* Ext = FGSTN_PartsSnapshotExt::FromView(View.Victim);
	if (!Ext)
	{
		return EGSTN_StepResult::Continue;
	}
	// Bench timing of the whole step for a part-bearing victim (debug counter, not state)
	const uint64 Start = FPlatformTime::Cycles64();
	ON_SCOPE_EXIT
	{
		FGSTN_PartsMinionStepTiming& Timing = GetTiming();
		Timing.Cycles += FPlatformTime::Cycles64() - Start;
		++Timing.Count;
	};

	const UGSTN_PartSetDefinition* Set = Ext->PartSet;
	if (GStepCacheEnabled && View.Rules && Set && !Set->Parts.IsEmpty())
	{
		const FStepCacheEntry& Cache = FindOrBuildEntry(*Set, *View.Rules);
		if (Cache.bHasPartResolve)
		{
			// Parts-aware rules: their PartResolve (validated before ComputeKnockback) already resolved the part on this same view
			return EGSTN_StepResult::Continue;
		}
		return ResolveCached(View, Out, *Ext, *Set, Cache);
	}
	return ResolveUncached(View, Out);
}

EGSTN_StepResult UGSTN_HitStep_PartResolveMinion::ResolveUncached(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!FGSTN_PartsSnapshotExt::FromView(View.Victim))
	{
		return EGSTN_StepResult::Continue;
	}

	bool bRulesHavePartResolve = false;
	bool bRulesHaveApplyDamage = false;
	bool bRulesHaveHealthWeight = false;
	if (View.Rules)
	{
		for (const TSubclassOf<UGSTN_HitResolutionStep>& StepClass : View.Rules->GetResolvedHitPipeline())
		{
			if (!StepClass || StepClass->IsChildOf(UGSTN_HitStep_PartResolveMinion::StaticClass()))
			{
				continue;
			}
			bRulesHavePartResolve |= StepClass->IsChildOf(UGSTN_HitStep_PartResolve::StaticClass());
			bRulesHaveApplyDamage |= StepClass->IsChildOf(UGSTN_HitStep_ApplyDamage::StaticClass());
			bRulesHaveHealthWeight |= StepClass->IsChildOf(UGSTN_HitStep_HealthWeight::StaticClass());
		}
	}
	if (bRulesHavePartResolve)
	{
		// Parts-aware rules: their PartResolve (validated before ComputeKnockback) already resolved the part on this same view
		return EGSTN_StepResult::Continue;
	}

	const EGSTN_StepResult Verdict = Super::Resolve_Implementation(View, Out);

	// The parent wrote the damage only past its own early-out (the same conditions ApplyDamage tests), and a Stop (ricochet) ends the
	// pipeline before ApplyDamage: compensate only when both steps write
	const bool bDamaging = View.Outcome.bHit && !View.bNoDamage && !View.Outcome.bBlocked && !View.Outcome.bParried;
	if (Verdict == EGSTN_StepResult::Continue && bDamaging)
	{
		if (bRulesHaveApplyDamage)
		{
			// ApplyDamage adds the hit's own damage after ComputeKnockback; the outcome must end at the part's share (ComputeKnockback reads the
			// hit, never Outcome.Damage, so the transient negative sum changes no knockback)
			const float HitDamage = View.Incoming.Hit.Damage;
			if (HitDamage > 0.f)
			{
				Out.DamageAdd -= HitDamage;
			}
			// ApplyDamage appends the extra effects again (minions ignore them, but they are counted): keep one copy
			Out.Effects.Reset();
			// Out.Requests keeps the part's damage request next to ApplyDamage's: the minion batch never reads requests (no ability system)
		}
		if (bRulesHaveHealthWeight)
		{
			// HealthWeight merged the progress weight before this slot; the parent's weight already contains it (EffectiveWeightScale)
			FGSTN_StepResult HealthWeight;
			GetDefault<UGSTN_HitStep_HealthWeight>()->Resolve(View, HealthWeight);
			if (HealthWeight.WeightScale > UE_KINDA_SMALL_NUMBER)
			{
				Out.WeightScale /= HealthWeight.WeightScale;
			}
		}
	}
	return Verdict;
}

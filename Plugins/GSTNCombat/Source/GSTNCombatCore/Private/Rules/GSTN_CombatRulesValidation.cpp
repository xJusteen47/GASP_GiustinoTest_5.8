// Pure combat logic (no GAS, no Network Prediction).

#include "Rules/GSTN_CombatRulesValidation.h"

#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Rules/GSTN_CombatRules.h"

namespace
{
	// Steps are compared by their short name (the class name without the GSTN_HitStep_ prefix), so a placeholder written either way matches
	const FName StepComputeKnockback(TEXT("ComputeKnockback"));
	const FName StepHealthWeight(TEXT("HealthWeight"));
	const FName StepHitstop(TEXT("Hitstop"));
	const FName StepPartResolve(TEXT("PartResolve"));

	FName ShortName(FName Name)
	{
		FString Text = Name.ToString();
		Text.RemoveFromStart(TEXT("GSTN_HitStep_"));
		return FName(*Text);
	}

	/** Steps that write what the knockback model reads: after ComputeKnockback they would never reach it */
	bool WritesModelInput(FName Name)
	{
		static const TArray<FName> Names = {
			FName(TEXT("HurtState")), FName(TEXT("HurtboxReaction")), FName(TEXT("HealthWeight")),
			FName(TEXT("Shield")), FName(TEXT("Parry")), FName(TEXT("PartResolve")) };
		return Names.Contains(Name);
	}

	void Add(TArray<FGSTN_ReactionMessage>& Out, bool bError, const FString& Text)
	{
		FGSTN_ReactionMessage& Message = Out.AddDefaulted_GetRef();
		Message.bError = bError;
		Message.Text = Text;
	}

	/** The cell of one victim and one hit */
	FString Cell(const UGSTN_KnockbackModel& Model, const FGSTN_ReactionMatrixVictim& Victim, const FGSTN_HitData& Hit)
	{
		FGSTN_KnockbackInput Input;
		Input.Hit = Hit;
		Input.Weight = Victim.Weight;
		Input.LaunchDirection = FVector(1.0, 0.0, 1.0).GetSafeNormal();
		Input.Reaction.StaggerThreshold = Victim.StaggerThreshold;
		Input.Reaction.MaxReaction = Victim.MaxReaction;

		const FGSTN_KnockbackResult Result = Model.Compute(Input);
		if (!Result.bStaggered)
		{
			return TEXT("-");
		}
		switch (Result.Reaction)
		{
		case EGSTN_HitReaction::StaggerInPlace: return TEXT("In");
		case EGSTN_HitReaction::PushBack: return TEXT("Push");
		default: return Result.bTumble ? TEXT("LaunchT") : TEXT("Launch");
		}
	}
}

bool FGSTN_CombatRulesValidation::IsKnownPlaceholderName(FName StepName)
{
	const FName Short = ShortName(StepName);
	return Short == TEXT("Shield") || Short == TEXT("Parry") || Short == StepPartResolve;
}

FName FGSTN_CombatRulesValidation::GetStepName(const TSoftClassPtr<UGSTN_HitResolutionStep>& Entry)
{
	return FName(*Entry.ToSoftObjectPath().GetAssetName());
}

void FGSTN_CombatRulesValidation::Validate(const UGSTN_CombatRules& Rules, TArray<FGSTN_ReactionMessage>& OutMessages)
{
	// Pipeline entries: usable, placeholder or unknown
	TArray<FName> Names;
	for (int32 Index = 0; Index < Rules.HitPipeline.Num(); ++Index)
	{
		const TSoftClassPtr<UGSTN_HitResolutionStep>& Entry = Rules.HitPipeline[Index];
		if (Entry.IsNull())
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hit step %d: no class set. It is skipped at run time"), Index));
			Names.Add(NAME_None);
			continue;
		}

		const UClass* Class = Entry.LoadSynchronous();
		const FName Name = ShortName(Class ? FName(*Class->GetName()) : GetStepName(Entry));
		Names.Add(Name);
		if (!Class)
		{
			if (IsKnownPlaceholderName(Name))
			{
				Add(OutMessages, false, FString::Printf(TEXT("Hit step %d (%s) is provided by an optional plugin that is not loaded. It is skipped at run time"), Index, *Name.ToString()));
			}
			else
			{
				Add(OutMessages, true, FString::Printf(TEXT("Hit step %d: class %s was not found (is its plugin loaded?). It is skipped at run time"), Index, *Entry.ToString()));
			}
		}
		else if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hit step %d: class %s is abstract. It is skipped at run time"), Index, *Name.ToString()));
		}
		else if (!Class->IsChildOf(UGSTN_HitResolutionStep::StaticClass()))
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hit step %d: class %s is not a hit resolution step. It is skipped at run time"), Index, *Name.ToString()));
		}
	}

	// Order: what feeds the model must run before it
	const int32 KnockbackIndex = Names.IndexOfByKey(StepComputeKnockback);
	if (KnockbackIndex != INDEX_NONE)
	{
		for (int32 Index = KnockbackIndex + 1; Index < Names.Num(); ++Index)
		{
			if (WritesModelInput(Names[Index]))
			{
				Add(OutMessages, true, FString::Printf(TEXT("Hit step %d (%s) runs after ComputeKnockback, so what it writes never reaches the knockback model"), Index, *Names[Index].ToString()));
			}
		}
	}
	const int32 HitstopIndex = Names.IndexOfByKey(StepHitstop);
	if (HitstopIndex != INDEX_NONE && KnockbackIndex != INDEX_NONE && HitstopIndex < KnockbackIndex)
	{
		Add(OutMessages, false, TEXT("The Hitstop step runs before ComputeKnockback: the hitstop frames do not exist yet, so it requests nothing"));
	}

	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		if (Names[Index] != NAME_None && Names.IndexOfByKey(Names[Index]) != Index)
		{
			Add(OutMessages, false, FString::Printf(TEXT("Hit step %d (%s) is listed more than once"), Index, *Names[Index].ToString()));
		}
	}

	const bool bHasHealthWeight = Names.Contains(StepHealthWeight);
	if (bHasHealthWeight && Names.Contains(StepPartResolve))
	{
		Add(OutMessages, false, TEXT("HealthWeight and PartResolve are both in the pipeline: they both write the weight scale, and the lost mass of PartResolve is meant to replace the health weight"));
	}

	if (Rules.MaxHealth <= 0.f)
	{
		Add(OutMessages, true, TEXT("MaxHealth must be positive: the health weight and the damage progress divide by it"));
	}
	if (Rules.bUseLagCompensation && Rules.MaxRewindMs <= 0)
	{
		Add(OutMessages, true, TEXT("Lag compensation is on with a MaxRewindMs of 0: nothing could be rewound"));
	}

	if (Rules.KnockbackModel)
	{
		FGSTN_ReactionValidation::ValidateTuning(Rules.KnockbackModel->Tuning, OutMessages);
		if (bHasHealthWeight)
		{
			FGSTN_ReactionValidation::ValidateModelWithCurve(Rules.KnockbackModel, Rules.DefaultWeightCurve, OutMessages);
		}
	}
	else if (KnockbackIndex != INDEX_NONE)
	{
		Add(OutMessages, false, TEXT("ComputeKnockback is in the pipeline but the rules name no knockback model: it writes nothing"));
	}
}

void FGSTN_CombatRulesValidation::ValidateHitstopOverride(const FGSTN_HitData& Hit, const FGSTN_ReactionTuning& Tuning, TArray<FGSTN_ReactionMessage>& OutMessages)
{
	if (Hit.HitstopMode != EGSTN_HitstopMode::Override)
	{
		return;
	}
	if (Hit.HitstopFramesOverride > Tuning.MaxHitstop || Hit.GetAttackerHitstopFramesOverride() > Tuning.MaxHitstop)
	{
		Add(OutMessages, false, FString::Printf(TEXT("A hitstop override above MaxHitstop (%d) is clamped to it"), Tuning.MaxHitstop));
	}
}

FString FGSTN_CombatRulesValidation::PrintReactionMatrix(const UGSTN_CombatRules& Rules, const TArray<FGSTN_ReactionMatrixVictim>& Victims, const TArray<FGSTN_ReactionMatrixHit>& Hits)
{
	if (!Rules.KnockbackModel)
	{
		return TEXT("No knockback model in the rules");
	}

	FString Text = TEXT("Victim");
	for (const FGSTN_ReactionMatrixHit& Hit : Hits)
	{
		Text += TEXT(" | ") + Hit.Name;
	}
	for (const FGSTN_ReactionMatrixVictim& Victim : Victims)
	{
		Text += FString::Printf(TEXT("\n%s (weight %g)"), *Victim.Name, Victim.Weight);
		for (const FGSTN_ReactionMatrixHit& Hit : Hits)
		{
			Text += TEXT(" | ") + Cell(*Rules.KnockbackModel, Victim, Hit.Hit);
		}
	}
	return Text;
}

TArray<FGSTN_ReactionMatrixHit> FGSTN_CombatRulesValidation::MakeStandardMatrixHits()
{
	TArray<FGSTN_ReactionMatrixHit> Hits;
	auto AddHit = [&Hits](const TCHAR* Name, float Damage, float Base, float Growth)
	{
		FGSTN_ReactionMatrixHit& Hit = Hits.AddDefaulted_GetRef();
		Hit.Name = Name;
		Hit.Hit.Damage = Damage;
		Hit.Hit.BaseKnockback = Base;
		Hit.Hit.KnockbackGrowth = Growth;
	};
	AddHit(TEXT("Light"), 4.f, 450.f, 0.f);
	AddHit(TEXT("Medium"), 9.f, 900.f, 0.f);
	AddHit(TEXT("Heavy"), 16.f, 1000.f, 10.f);
	return Hits;
}

FString FGSTN_CombatRulesValidation::PrintStandardMatrix(const UGSTN_CombatRules& Rules)
{
	TArray<FGSTN_ReactionMatrixVictim> Victims;
	auto AddVictim = [&Victims](const TCHAR* Name, float Weight)
	{
		FGSTN_ReactionMatrixVictim& Victim = Victims.AddDefaulted_GetRef();
		Victim.Name = Name;
		Victim.Weight = Weight;
	};
	AddVictim(TEXT("Light"), 60.f);
	AddVictim(TEXT("Medium"), 100.f);
	AddVictim(TEXT("Heavy"), 160.f);
	return PrintReactionMatrix(Rules, Victims, MakeStandardMatrixHits());
}

FString FGSTN_CombatRulesValidation::PrintHurtboxSetMatrix(const UGSTN_HurtboxSetDefinition& Set, const UGSTN_CombatRules& Rules)
{
	TArray<FGSTN_ReactionMatrixVictim> Victims;
	for (const FGSTN_Hurtbox& Hurtbox : Set.Hurtboxes)
	{
		FGSTN_ReactionMatrixVictim& Victim = Victims.AddDefaulted_GetRef();
		Victim.Name = Hurtbox.Name.ToString();
		Victim.Weight = 100.f * Hurtbox.ReactionScale.WeightScale;
		Victim.StaggerThreshold = Hurtbox.ReactionScale.StaggerThreshold.IsSet() ? Hurtbox.ReactionScale.StaggerThreshold : Set.StaggerThreshold;
		Victim.MaxReaction = static_cast<EGSTN_HitReaction>(FMath::Min(static_cast<uint8>(Hurtbox.ReactionScale.MaxReaction), static_cast<uint8>(Set.MaxReaction)));
	}
	return PrintReactionMatrix(Rules, Victims, MakeStandardMatrixHits());
}

void FGSTN_CombatRulesValidation::ValidateHurtboxSet(const UGSTN_HurtboxSetDefinition& Set, TArray<FGSTN_ReactionMessage>& OutMessages)
{
	if (Set.Hurtboxes.IsEmpty())
	{
		Add(OutMessages, true, TEXT("The set has no hurtbox: nothing could be hit"));
	}
	if (Set.Hurtboxes.Num() > 255)
	{
		Add(OutMessages, true, FString::Printf(TEXT("The set has %d hurtboxes, the index of a hit is a byte (255 at most)"), Set.Hurtboxes.Num()));
	}

	TSet<FName> Names;
	for (int32 Index = 0; Index < Set.Hurtboxes.Num(); ++Index)
	{
		const FGSTN_Hurtbox& Hurtbox = Set.Hurtboxes[Index];
		if (Hurtbox.Name.IsNone())
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hurtbox %d has no name: a modifier track cannot address it"), Index));
		}
		else if (Names.Contains(Hurtbox.Name))
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hurtbox %d: the name %s is used twice"), Index, *Hurtbox.Name.ToString()));
		}
		Names.Add(Hurtbox.Name);

		bool bVolume = false;
		switch (Hurtbox.Shape.Type)
		{
		case EGSTN_ShapeType::Sphere: bVolume = Hurtbox.Shape.Extent.X > 0.0; break;
		case EGSTN_ShapeType::Capsule: bVolume = Hurtbox.Shape.Extent.X > 0.0 && Hurtbox.Shape.Extent.Z >= Hurtbox.Shape.Extent.X; break;
		case EGSTN_ShapeType::Box: bVolume = Hurtbox.Shape.Extent.X > 0.0 && Hurtbox.Shape.Extent.Y > 0.0 && Hurtbox.Shape.Extent.Z > 0.0; break;
		}
		if (!bVolume)
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hurtbox %d (%s): the shape has no volume"), Index, *Hurtbox.Name.ToString()));
		}
		if (Hurtbox.Tags.IsEmpty())
		{
			Add(OutMessages, false, FString::Printf(TEXT("Hurtbox %d (%s) has no Combat.Hurtbox tag: a modifier track cannot address it by tag"), Index, *Hurtbox.Name.ToString()));
		}
		const FGSTN_HitReactionModifiers& Scale = Hurtbox.ReactionScale;
		if (Scale.WeightScale < 0.f || Scale.KnockbackScale < 0.f || Scale.HitstunScale < 0.f || Scale.HitstopScale < 0.f || Scale.StaggerScale < 0.f)
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hurtbox %d (%s): a reaction scale is negative"), Index, *Hurtbox.Name.ToString()));
		}
	}

	if (Set.WeightCurve.Samples.Num() != 0 && !Set.WeightCurve.IsAuthored())
	{
		Add(OutMessages, true, FString::Printf(TEXT("The weight curve needs %d samples, this one has %d"), FGSTN_BakedCurve::SampleCount, Set.WeightCurve.Samples.Num()));
	}
}

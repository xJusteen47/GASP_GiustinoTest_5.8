// Pure combat logic (no GAS, no Network Prediction).

#include "Rules/GSTN_CombatRulesValidation.h"

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

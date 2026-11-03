#include "Rules/GSTN_CombatRules.h"

#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_KnockbackModels.h"
#include "Rules/GSTN_CombatRulesValidation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatRules, Log, All);

namespace
{
	/** Null when the entry names nothing usable; Reason says why */
	template <typename StepT>
	UClass* LoadStepClass(const TSoftClassPtr<StepT>& Entry, FString& Reason)
	{
		if (Entry.IsNull())
		{
			Reason = TEXT("no class set");
			return nullptr;
		}

		UClass* Class = Entry.LoadSynchronous();
		if (!Class)
		{
			Reason = FString::Printf(TEXT("class %s was not found (is its plugin loaded?)"), *Entry.ToString());
		}
		else if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Reason = FString::Printf(TEXT("class %s is abstract"), *Class->GetName());
			Class = nullptr;
		}
		return Class;
	}

	template <typename StepT>
	TSoftClassPtr<UGSTN_HitResolutionStep> HitStep()
	{
		return TSoftClassPtr<UGSTN_HitResolutionStep>(StepT::StaticClass());
	}
}

UGSTN_CombatRules::UGSTN_CombatRules()
{
	Pipeline.Add(TSoftClassPtr<UGSTN_CombatStep>(UGSTN_CombatStep_Activation::StaticClass()));
	NonBlockableHitTypes.Add(GSTNCombatHitTags::HitType_Unblockable.GetTag());
}

void UGSTN_CombatRules::ResolvePipeline()
{
	ResolvedPipeline.Reset();
	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		FString Reason;
		if (UClass* Class = LoadStepClass(Pipeline[Index], Reason))
		{
			ResolvedPipeline.Add(Class);
		}
		else
		{
			UE_LOG(LogGSTNCombatRules, Warning, TEXT("%s: pipeline step %d skipped, %s"), *GetPathName(), Index, *Reason);
		}
	}
}

void UGSTN_CombatRules::ResolveHitPipeline(TArray<FString>* OutReports)
{
	ResolvedHitPipeline.Reset();
	for (int32 Index = 0; Index < HitPipeline.Num(); ++Index)
	{
		FString Reason;
		UClass* Class = LoadStepClass(HitPipeline[Index], Reason);
		if (Class && !Class->IsChildOf(UGSTN_HitResolutionStep::StaticClass()))
		{
			Reason = FString::Printf(TEXT("class %s is not a hit resolution step"), *Class->GetName());
			Class = nullptr;
		}

		if (Class)
		{
			ResolvedHitPipeline.Add(Class);
		}
		else
		{
			// A placeholder whose plugin is not loaded is expected in a rules asset shared between games; it is still skipped
			const bool bPlaceholder = FGSTN_CombatRulesValidation::IsKnownPlaceholderName(FGSTN_CombatRulesValidation::GetStepName(HitPipeline[Index]));
			UE_LOG(LogGSTNCombatRules, Log, TEXT("%s: hit step %d skipped, %s%s"), *GetPathName(), Index, *Reason, bPlaceholder ? TEXT(" (a placeholder step of an optional plugin)") : TEXT(""));
			if (OutReports)
			{
				OutReports->Add(FString::Printf(TEXT("hit step %d skipped, %s"), Index, *Reason));
			}
		}
	}
}

FGSTN_CombatStepResult UGSTN_CombatRules::RunPipeline(const FGSTN_CombatStepView& View) const
{
	for (const TSubclassOf<UGSTN_CombatStep>& StepClass : ResolvedPipeline)
	{
		const UGSTN_CombatStep* Step = StepClass ? GetDefault<UGSTN_CombatStep>(StepClass) : nullptr;
		if (!Step)
		{
			continue;
		}

		const FGSTN_CombatStepResult Result = Step->Evaluate(View);
		if (Result.Verdict == EGSTN_CombatStepVerdict::Reject)
		{
			return Result;
		}
	}
	return FGSTN_CombatStepResult();
}

FGameplayTag UGSTN_CombatRules::ResolveRelationship(const FGSTN_RelationshipQuery& Query) const
{
	return RelationshipResolver ? RelationshipResolver->Resolve(Query) : UGSTN_RelationshipResolver::DefaultRule(Query);
}

bool UGSTN_CombatRules::IsRelationshipHittable(const FGameplayTag& Relationship) const
{
	const FGameplayTag Ally = GSTNCombatHitTags::Relationship_Ally.GetTag();
	const bool bListed = HittableRelationships.IsEmpty()
		? (Relationship == GSTNCombatHitTags::Relationship_Enemy.GetTag() || Relationship == GSTNCombatHitTags::Relationship_Neutral.GetTag())
		: HittableRelationships.HasTagExact(Relationship);
	return bListed || (bFriendlyFire && Relationship == Ally);
}

float UGSTN_CombatRules::GetDamageProgress(const FGSTN_HitVictimView& Victim) const
{
	if (MaxHealth <= 0.f)
	{
		return 0.f;
	}
	if (DamageMode == EGSTN_DamageMode::Health)
	{
		return FMath::Clamp(1.f - Victim.HealthFraction, 0.f, 1.f);
	}
	return FMath::Clamp(Victim.ReactionState.GetDamagePercent() / MaxHealth, 0.f, 1.f);
}

UGSTN_CombatRules* UGSTN_CombatRules::CreateStandard(UObject* Outer)
{
	UGSTN_CombatRules* Rules = NewObject<UGSTN_CombatRules>(Outer);
	FillStandard(Rules);
	return Rules;
}

void UGSTN_CombatRules::FillStandard(UGSTN_CombatRules* Rules)
{
	if (Rules == nullptr)
	{
		return;
	}

	Rules->HittableRelationships.Reset();
	Rules->HitPipeline = {
		HitStep<UGSTN_HitStep_TeamFilter>(), HitStep<UGSTN_HitStep_HurtState>(), HitStep<UGSTN_HitStep_HurtboxReaction>(),
		HitStep<UGSTN_HitStep_HealthWeight>(), HitStep<UGSTN_HitStep_ComputeKnockback>(), HitStep<UGSTN_HitStep_ApplyDamage>(),
		HitStep<UGSTN_HitStep_Hitstop>(), HitStep<UGSTN_HitStep_MeterGain>() };

	// The tier thresholds are fractions of the tumble threshold, so a game that moves the tumble point moves them with it by
	// changing one number here. HitstunFactor keeps the model default (0.02 frames per cm/s): 0.3 x 1800 = 540 cm/s staggers for
	// about 11 frames, a launch at 1080 for 21, a tumble at 1800 for 36, against MinHitstunFrames 6.
	UGSTN_KnockbackModel_WeightDriven* Model = NewObject<UGSTN_KnockbackModel_WeightDriven>(Rules);
	Model->Tuning.PushBackThreshold = 0.3f * Model->Tuning.TumbleThreshold;
	Model->Tuning.LaunchThreshold = 0.6f * Model->Tuning.TumbleThreshold;
	Rules->KnockbackModel = Model;

	Rules->DamageMode = EGSTN_DamageMode::Percent;
	Rules->bFriendlyFire = false;
	Rules->HittableRelationships.AddTag(GSTNCombatHitTags::Relationship_Enemy.GetTag());
	Rules->HittableRelationships.AddTag(GSTNCombatHitTags::Relationship_Neutral.GetTag());

	// 1.0 at no damage falling to 0.6 at MaxHealth: a battered victim flies further and climbs the reaction tiers
	Rules->DefaultWeightCurve.BakeLinear(1.0, 0.6);

	Rules->ResolvePipeline();
	Rules->ResolveHitPipeline();
}

#if WITH_EDITOR
EDataValidationResult UGSTN_CombatRules::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		FString Reason;
		if (!LoadStepClass(Pipeline[Index], Reason))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Pipeline step %d: %s. It is skipped at run time."), Index, *Reason)));
			Result = EDataValidationResult::Invalid;
		}
	}

	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_CombatRulesValidation::Validate(*this, Messages);
	for (const FGSTN_ReactionMessage& Message : Messages)
	{
		if (Message.bError)
		{
			Context.AddError(FText::FromString(Message.Text));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			Context.AddWarning(FText::FromString(Message.Text));
		}
	}

	// The matrix is for the designer: which weight and which hit stagger how far under these rules
	if (KnockbackModel != nullptr)
	{
		Context.AddMessage(EMessageSeverity::Info, FText::FromString(TEXT("Reaction matrix:\n") + FGSTN_CombatRulesValidation::PrintStandardMatrix(*this)));
	}

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

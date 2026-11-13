// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimSetUtils.h"

#include "Animation/AnimInstance.h"
#include "Algo/StableSort.h"
#include "Animation/AnimMontage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimSetUtils)

#define LOCTEXT_NAMESPACE "GSTNAnimSet"

namespace GSTNAnimSetPrivate
{
	struct FCandidate
	{
		UGSTN_AnimSetDefinition* Set = nullptr;
		int32 SourcePriority = 0;
		int32 RulePriority = 0;
	};

	/** The competing sets of every slot, best first (stable: on a tie the earlier source, then the earlier rule, stays in front) */
	static TMap<FGameplayTag, TArray<FCandidate>> RankCandidates(TConstArrayView<FGSTN_AnimSetSource> Sources, const FGameplayTagContainer& CosmeticTags)
	{
		TMap<FGameplayTag, TArray<FCandidate>> BySlot;
		for (const FGSTN_AnimSetSource& Source : Sources)
		{
			if (!Source.Collection)
			{
				continue;
			}

			for (const FGSTN_AnimSetRule& Rule : Source.Collection->Rules)
			{
				UGSTN_AnimSetDefinition* Set = Rule.Set.Get();
				if (!Set || !Set->SetTag.IsValid())
				{
					continue;
				}

				if (!Rule.When.IsEmpty() && !Rule.When.Matches(CosmeticTags))
				{
					continue;
				}

				BySlot.FindOrAdd(Set->SetTag).Add({ Set, Source.SourcePriority, Rule.Priority });
			}
		}

		for (TPair<FGameplayTag, TArray<FCandidate>>& Slot : BySlot)
		{
			Algo::StableSort(Slot.Value, [](const FCandidate& A, const FCandidate& B)
			{
				return A.SourcePriority != B.SourcePriority ? A.SourcePriority > B.SourcePriority : A.RulePriority > B.RulePriority;
			});
		}
		return BySlot;
	}
}

TMap<FGameplayTag, UGSTN_AnimSetDefinition*> UGSTN_AnimSetUtils::ResolveWinners(TConstArrayView<FGSTN_AnimSetSource> Sources, const FGameplayTagContainer& CosmeticTags)
{
	TMap<FGameplayTag, UGSTN_AnimSetDefinition*> Winners;
	for (const TPair<FGameplayTag, TArray<GSTNAnimSetPrivate::FCandidate>>& Slot : GSTNAnimSetPrivate::RankCandidates(Sources, CosmeticTags))
	{
		Winners.Add(Slot.Key, Slot.Value[0].Set);
	}
	return Winners;
}

TMap<FGameplayTag, TSoftClassPtr<UAnimInstance>> UGSTN_AnimSetUtils::ResolveLayerClasses(TConstArrayView<FGSTN_AnimSetSource> Sources, const FGameplayTagContainer& CosmeticTags)
{
	TMap<FGameplayTag, TSoftClassPtr<UAnimInstance>> Layers;
	for (const TPair<FGameplayTag, TArray<GSTNAnimSetPrivate::FCandidate>>& Slot : GSTNAnimSetPrivate::RankCandidates(Sources, CosmeticTags))
	{
		for (const GSTNAnimSetPrivate::FCandidate& Candidate : Slot.Value)
		{
			if (!Candidate.Set->LayerClass.IsNull())
			{
				Layers.Add(Slot.Key, Candidate.Set->LayerClass);
				break;
			}
		}
	}
	return Layers;
}

bool UGSTN_AnimSetUtils::ValidateMontageOverride(const UAnimMontage& Reference, const UAnimMontage& Visual, TArray<FText>& OutProblems)
{
	const int32 ProblemsBefore = OutProblems.Num();

	const int32 NumSections = Reference.CompositeSections.Num();
	if (Visual.CompositeSections.Num() != NumSections)
	{
		OutProblems.Add(FText::Format(LOCTEXT("SectionCount", "{0} has {1} sections, its reference {2} has {3}"),
			FText::FromString(Visual.GetName()), Visual.CompositeSections.Num(), FText::FromString(Reference.GetName()), NumSections));
	}
	else
	{
		for (int32 Index = 0; Index < NumSections; ++Index)
		{
			const FName RefName = Reference.CompositeSections[Index].SectionName;
			const FName VisName = Visual.CompositeSections[Index].SectionName;
			if (RefName != VisName)
			{
				OutProblems.Add(FText::Format(LOCTEXT("SectionName", "{0}: section {1} is '{2}', the reference has '{3}'"),
					FText::FromString(Visual.GetName()), Index, FText::FromName(VisName), FText::FromName(RefName)));
				continue;
			}

			const float Delta = FMath::Abs(Visual.GetSectionLength(Index) - Reference.GetSectionLength(Index));
			if (Delta > GSTNAnimSet::MaxSectionLengthDelta)
			{
				OutProblems.Add(FText::Format(LOCTEXT("SectionLength", "{0}: section '{1}' differs from the reference by {2} s (max one frame at 30 fps)"),
					FText::FromString(Visual.GetName()), FText::FromName(VisName), FText::AsNumber(Delta)));
			}
		}
	}

	if (!FMath::IsNearlyEqual(Visual.RateScale, Reference.RateScale))
	{
		OutProblems.Add(FText::Format(LOCTEXT("RateScale", "{0}: rate scale {1}, the reference has {2}"),
			FText::FromString(Visual.GetName()), FText::AsNumber(Visual.RateScale), FText::AsNumber(Reference.RateScale)));
	}

	// Movement comes from the simulation, not from the clip: a variant must not add root motion the reference does not have
	if (Visual.HasRootMotion() && !Reference.HasRootMotion())
	{
		OutProblems.Add(FText::Format(LOCTEXT("RootMotion", "{0} has root motion, its reference {1} has none"),
			FText::FromString(Visual.GetName()), FText::FromString(Reference.GetName())));
	}

	return OutProblems.Num() == ProblemsBefore;
}

#undef LOCTEXT_NAMESPACE

// Ability-system layer of the combat framework.

#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "Animation/AnimSequenceBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FGSTN_ReactionAnimRule::Matches(const FGameplayTagContainer& OwnerTags) const
{
	return !RequiredTags.IsEmpty() && OwnerTags.HasAll(RequiredTags) && !OwnerTags.HasAny(BlockedTags);
}

int32 UGSTN_ReactionAnimSet::SelectRule(const FGameplayTagContainer& OwnerTags) const
{
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		// Strictly greater: of equal priorities the first listed stays
		if (Rules[Index].Matches(OwnerTags) && (Best == INDEX_NONE || Rules[Index].Priority > Rules[Best].Priority))
		{
			Best = Index;
		}
	}
	return Best;
}

FPrimaryAssetId UGSTN_ReactionAnimSet::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GSTN_ReactionAnimSet"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_ReactionAnimSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		const FGSTN_ReactionAnimRule& Rule = Rules[Index];
		if (Rule.RequiredTags.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Reaction rule %d (%s) has no required tag: it would never play."), Index, *Rule.Name.ToString())));
			Result = EDataValidationResult::Invalid;
		}
		if (Rule.Animation.IsNull())
		{
			Context.AddWarning(FText::FromString(FString::Printf(TEXT("Reaction rule %d (%s) has no animation: it shows the character's own pose."), Index, *Rule.Name.ToString())));
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

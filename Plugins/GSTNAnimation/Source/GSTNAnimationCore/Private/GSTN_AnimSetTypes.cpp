// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimSetTypes.h"

#include "Animation/AnimMontage.h"
#include "GSTN_AnimSetUtils.h"
#include "GSTN_AnimTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimSetTypes)

#define LOCTEXT_NAMESPACE "GSTNAnimSet"

namespace GSTNAnimSet
{
	const FPrimaryAssetType AnimSetType(TEXT("GSTN_AnimSet"));
	const FPrimaryAssetType AnimSetCollectionType(TEXT("GSTN_AnimSetCollection"));
}

FPrimaryAssetId UGSTN_AnimSetDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNAnimSet::AnimSetType, GetFName());
}

FPrimaryAssetId UGSTN_AnimSetCollection::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNAnimSet::AnimSetCollectionType, GetFName());
}

#if WITH_EDITOR

EDataValidationResult UGSTN_AnimSetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!SetTag.IsValid() || !SetTag.MatchesTag(GSTNAnimTags::Root_Set) || SetTag == GSTNAnimTags::Root_Set.GetTag())
	{
		Context.AddError(LOCTEXT("SetTag", "SetTag must be a leaf under Anim.Set"));
		Result = EDataValidationResult::Invalid;
	}

	TSet<FSoftObjectPath> SeenReferences;
	for (int32 Index = 0; Index < MontageOverrides.Num(); ++Index)
	{
		const FGSTN_MontageOverride& Override = MontageOverrides[Index];
		if (Override.Reference.IsNull() || Override.Visual.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("OverrideEmpty", "Montage override {0} needs both a reference and a visual montage"), Index));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		bool bAlreadySeen = false;
		SeenReferences.Add(Override.Reference.ToSoftObjectPath(), &bAlreadySeen);
		if (bAlreadySeen)
		{
			Context.AddError(FText::Format(LOCTEXT("OverrideDuplicate", "Montage override {0} replaces a reference montage already replaced above"), Index));
			Result = EDataValidationResult::Invalid;
		}

		// Validation runs in the editor: loading the two montages here is acceptable
		const UAnimMontage* Reference = Override.Reference.LoadSynchronous();
		const UAnimMontage* Visual = Override.Visual.LoadSynchronous();
		TArray<FText> Problems;
		if (Reference && Visual && !UGSTN_AnimSetUtils::ValidateMontageOverride(*Reference, *Visual, Problems))
		{
			for (const FText& Problem : Problems)
			{
				Context.AddError(Problem);
			}
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

EDataValidationResult UGSTN_AnimSetCollection::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		if (Rules[Index].Set.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("RuleEmpty", "Rule {0} names no anim set"), Index));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE

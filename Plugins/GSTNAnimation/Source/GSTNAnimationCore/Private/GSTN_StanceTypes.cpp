// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_StanceTypes.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_StanceTypes)

#define LOCTEXT_NAMESPACE "GSTN_StanceTypes"

UGSTN_StanceDefinition::UGSTN_StanceDefinition()
{
	// UEFN mannequin names. Weight rising along the spine hides the seam with the untouched pelvis; the torso arrives later than the arms
	auto Add = [this](const TCHAR* Bone, float Weight, float TimeScale)
	{
		FGSTN_StanceBoneWeight& Entry = BoneWeights.AddDefaulted_GetRef();
		Entry.Bone = Bone;
		Entry.Weight = Weight;
		Entry.FadeTimeScale = TimeScale;
	};
	Add(TEXT("spine_01"), 0.3f, 1.3f);
	Add(TEXT("spine_02"), 0.5f, 1.3f);
	Add(TEXT("spine_03"), 0.7f, 1.3f);
	Add(TEXT("spine_04"), 0.85f, 1.3f);
	Add(TEXT("spine_05"), 1.f, 1.3f);
	Add(TEXT("neck_01"), 1.f, 1.2f);
	Add(TEXT("clavicle_l"), 1.f, 1.f);
	Add(TEXT("clavicle_r"), 1.f, 1.f);
}

bool UGSTN_StanceDefinition::HasCompatiblePoses(FText* OutReason) const
{
	auto Fail = [OutReason](const FText& Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};
	if (!StancePose || !ReferencePose)
	{
		return Fail(LOCTEXT("Missing", "StancePose and ReferencePose must both be set"));
	}
	if (StancePose->GetSkeleton() != ReferencePose->GetSkeleton() || !StancePose->GetSkeleton())
	{
		return Fail(LOCTEXT("Skeleton", "StancePose and ReferencePose must use the same skeleton"));
	}
	if (StancePose->GetAdditiveAnimType() != AAT_None || ReferencePose->GetAdditiveAnimType() != AAT_None)
	{
		return Fail(LOCTEXT("Additive", "StancePose and ReferencePose must be plain (non-additive) sequences: the offset is computed at runtime"));
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UGSTN_StanceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	FText Reason;
	if (!HasCompatiblePoses(&Reason))
	{
		Context.AddError(Reason);
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE

// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_PartSetDefinition.h"

#include "Data/GSTN_PartsAssetTypes.h"
#include "Data/GSTN_PartsValidation.h"
#include "GSTN_PartsSettings.h"

#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Misc/DataValidation.h"
#endif

UGSTN_PartSetDefinition::UGSTN_PartSetDefinition()
{
	// A starting shape for an override: the default of the spec, 1.0 at no damage falling to 0.6 at the cap
	if (FRichCurve* Curve = WeightCurve.GetRichCurve())
	{
		Curve->AddKey(0.f, 1.f);
		Curve->AddKey(1.f, 0.6f);
	}
	BakedWeightCurve = FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.6f);
}

int32 UGSTN_PartSetDefinition::FindPartByName(FName PartName) const
{
	const TConstArrayView<FGSTN_PartDefinition> View = GetParts();
	for (int32 Index = 0; Index < View.Num(); ++Index)
	{
		if (View[Index].Name == PartName)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UGSTN_PartSetDefinition::FindPartByHurtbox(FName HurtboxName) const
{
	const TConstArrayView<FGSTN_PartDefinition> View = GetParts();
	for (int32 Index = 0; Index < View.Num(); ++Index)
	{
		if (View[Index].Hurtboxes.Contains(HurtboxName))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

uint16 UGSTN_PartSetDefinition::GetBreakableMask() const
{
	uint16 Mask = 0;
	const TConstArrayView<FGSTN_PartDefinition> View = GetParts();
	for (int32 Index = 0; Index < View.Num(); ++Index)
	{
		if (View[Index].IsBreakable())
		{
			Mask |= static_cast<uint16>(1u << Index);
		}
	}
	return Mask;
}

void UGSTN_PartSetDefinition::RebakeWeightCurve()
{
	if (bOverrideWeightCurve)
	{
		if (const FRichCurve* Curve = WeightCurve.GetRichCurveConst())
		{
			BakedWeightCurve = FGSTN_PartsBakedCurve::FromRichCurve(*Curve);
			return;
		}
	}
	BakedWeightCurve = FGSTN_PartsBakedCurve::MakeLinear(1.f, UGSTN_PartsSettings::Get().DefaultWeightScaleAtCap);
}

FPrimaryAssetId UGSTN_PartSetDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNPartsAssetTypes::PartSet(), GetFName());
}

void UGSTN_PartSetDefinition::PostLoad()
{
	Super::PostLoad();
	RebakeWeightCurve();
}

void UGSTN_PartSetDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	RebakeWeightCurve();
}

#if WITH_EDITOR
void UGSTN_PartSetDefinition::PostEditImport()
{
	Super::PostEditImport();
	RebakeWeightCurve();
}

void UGSTN_PartSetDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebakeWeightCurve();
}

EDataValidationResult UGSTN_PartSetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Bone names come from the preview mesh when one is set; without it the check is skipped, not failed
	TSet<FName> Bones;
	bool bHaveBones = false;
#if WITH_EDITORONLY_DATA
	if (const USkeletalMesh* Mesh = PreviewMesh.LoadSynchronous())
	{
		const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
		for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum(); ++BoneIndex)
		{
			Bones.Add(Skeleton.GetBoneName(BoneIndex));
		}
		bHaveBones = true;
	}
#endif

	FGSTN_PartsValidationReport Report;
	FGSTN_PartsValidation::ValidateParts(Parts, GrabGate, BodyReaction, bHaveBones ? &Bones : nullptr,
		GSTN_PartsLimits::MaxParts, Report);
	for (const FGSTN_PartsIssue& Issue : Report.Issues)
	{
		if (Issue.bError)
		{
			Context.AddError(FText::FromString(Issue.Message));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			Context.AddWarning(FText::FromString(Issue.Message));
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

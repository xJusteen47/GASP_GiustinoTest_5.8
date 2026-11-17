// Editor node of FGSTN_AnimNode_CoveredLanding.

#include "GSTN_AnimGraphNode_CoveredLanding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_CoveredLanding)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_CoveredLanding"

FText UGSTN_AnimGraphNode_CoveredLanding::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Covered Landing");
}

FText UGSTN_AnimGraphNode_CoveredLanding::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "After a landing made while a montage drove the pose (a landing lag), holds the montage's last full pose over the base "
		"layer's own landing reaction, then blends back. Passes the pose through otherwise. Reads UGSTN_CharacterAnimInstance; a.GSTN.CoveredLanding 0 turns it off.");
}

FString UGSTN_AnimGraphNode_CoveredLanding::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_CoveredLanding::GetNodeTitleColor() const
{
	return FLinearColor(0.4f, 0.3f, 0.6f);
}

#undef LOCTEXT_NAMESPACE

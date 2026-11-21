// Editor node of FGSTN_AnimNode_StanceWidth.

#include "GSTN_AnimGraphNode_StanceWidth.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_StanceWidth)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_StanceWidth"

FText UGSTN_AnimGraphNode_StanceWidth::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Stance Width");
}

FText UGSTN_AnimGraphNode_StanceWidth::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Moves each foot outward (two-bone leg IK, heights kept) where the pose carries the curves GSTN_StanceWidth_L/_R. "
		"Widths: gstn.Loco.StanceWidth (cm per foot) and gstn.Loco.StanceMinHalf (minimum distance from the midline); gstn.Loco.StanceWidth.Force applies it to every clip. Put it at the end of the AnimGraph.");
}

FString UGSTN_AnimGraphNode_StanceWidth::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_StanceWidth::GetNodeTitleColor() const
{
	return FLinearColor(0.3f, 0.5f, 0.4f);
}

#undef LOCTEXT_NAMESPACE

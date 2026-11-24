// Editor node of FGSTN_AnimNode_GreyboxShape.

#include "GSTN_AnimGraphNode_GreyboxShape.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_GreyboxShape)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_GreyboxShape"

FText UGSTN_AnimGraphNode_GreyboxShape::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Greybox Shape");
}

FText UGSTN_AnimGraphNode_GreyboxShape::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Greybox capsule: squash/stretch and lean of the mesh root from the anim instance (GSTN Greybox Shape Source). Presentation only.");
}

FString UGSTN_AnimGraphNode_GreyboxShape::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_GreyboxShape::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.35f, 0.45f);
}

#undef LOCTEXT_NAMESPACE

// Editor node of FGSTN_AnimNode_GroundContactFix.

#include "GSTN_AnimGraphNode_GroundContactFix.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_GroundContactFix)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_GroundContactFix"

FText UGSTN_AnimGraphNode_GroundContactFix::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Ground Contact Fix");
}

FText UGSTN_AnimGraphNode_GroundContactFix::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Post-process on the final pose: in the states of its settings asset (tags, montages, curve) keeps body, hands and "
		"feet above the floor by lifting the pelvis and two-bone IK on the limbs, only by the depth they sink. Passes the pose through otherwise. "
		"a.GSTN.GroundContactFix 0 turns it off, gstn.Anim.Debug.GroundContactFix 1 draws it.");
}

FString UGSTN_AnimGraphNode_GroundContactFix::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_GroundContactFix::GetNodeTitleColor() const
{
	return FLinearColor(0.45f, 0.35f, 0.1f);
}

#undef LOCTEXT_NAMESPACE

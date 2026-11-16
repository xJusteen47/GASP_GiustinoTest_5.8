// Editor node of FGSTN_AnimNode_DirectionalHitAdditive.

#include "GSTN_AnimGraphNode_DirectionalHitAdditive.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_DirectionalHitAdditive)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_DirectionalHitAdditive"

FText UGSTN_AnimGraphNode_DirectionalHitAdditive::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Directional Hit Additive");
}

FText UGSTN_AnimGraphNode_DirectionalHitAdditive::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Adds the directional hit additive (front/back/left/right) chosen by the reaction component on the torso, weighted by the hit "
		"intensity. Without a clip it draws a PLACEHOLDER spine bend. Passes the pose through otherwise; a.GSTN.HitAdditive 0 turns it off.");
}

FString UGSTN_AnimGraphNode_DirectionalHitAdditive::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_DirectionalHitAdditive::GetNodeTitleColor() const
{
	return FLinearColor(0.6f, 0.25f, 0.1f);
}

#undef LOCTEXT_NAMESPACE

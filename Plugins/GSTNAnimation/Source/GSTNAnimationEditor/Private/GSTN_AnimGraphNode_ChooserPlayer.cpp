// Editor node of FGSTN_AnimNode_ChooserPlayer.

#include "GSTN_AnimGraphNode_ChooserPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_ChooserPlayer)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_ChooserPlayer"

FText UGSTN_AnimGraphNode_ChooserPlayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Chooser Player");
}

FText UGSTN_AnimGraphNode_ChooserPlayer::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Plays the clip a Chooser Table picks for this anim instance (parameter 0 = the instance, 1 = GSTN Chooser Player Output) each time the node becomes relevant. No root motion.");
}

FString UGSTN_AnimGraphNode_ChooserPlayer::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_ChooserPlayer::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.35f, 0.45f);
}

#undef LOCTEXT_NAMESPACE

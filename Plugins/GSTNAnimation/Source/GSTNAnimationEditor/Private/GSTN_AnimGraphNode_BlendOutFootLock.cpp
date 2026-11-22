// Editor node of FGSTN_AnimNode_BlendOutFootLock.

#include "GSTN_AnimGraphNode_BlendOutFootLock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_BlendOutFootLock)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_BlendOutFootLock"

FText UGSTN_AnimGraphNode_BlendOutFootLock::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Blend-Out Foot Lock");
}

FText UGSTN_AnimGraphNode_BlendOutFootLock::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Keeps the feet planted while a montage blends back to the game pose, then steps each foot to where the game pose "
		"wants it. Passes the pose through untouched otherwise. Request from UGSTN_CharacterAnimInstance; a.GSTN.BlendOutFootLock 0 turns it off.");
}

FString UGSTN_AnimGraphNode_BlendOutFootLock::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_BlendOutFootLock::GetNodeTitleColor() const
{
	return FLinearColor(0.1f, 0.5f, 0.4f);
}

#undef LOCTEXT_NAMESPACE

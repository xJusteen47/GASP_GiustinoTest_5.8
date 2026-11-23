// Editor node of FGSTN_AnimNode_ClipAtTime.

#include "GSTN_AnimGraphNode_ClipAtTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_ClipAtTime)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_ClipAtTime"

FText UGSTN_AnimGraphNode_ClipAtTime::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Clip At Time");
}

FText UGSTN_AnimGraphNode_ClipAtTime::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "The clip of this anim instance at the time it chooses (GSTN Clip Time Source, e.g. the run skid layer). No root motion.");
}

FString UGSTN_AnimGraphNode_ClipAtTime::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_ClipAtTime::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.35f, 0.45f);
}

#undef LOCTEXT_NAMESPACE

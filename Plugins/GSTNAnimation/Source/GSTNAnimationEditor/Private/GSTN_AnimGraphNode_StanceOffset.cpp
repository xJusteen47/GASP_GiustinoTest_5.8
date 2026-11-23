// Editor node of FGSTN_AnimNode_StanceOffset.

#include "GSTN_AnimGraphNode_StanceOffset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphNode_StanceOffset)

#define LOCTEXT_NAMESPACE "GSTN_AnimGraphNode_StanceOffset"

FText UGSTN_AnimGraphNode_StanceOffset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "GSTN Stance Offset");
}

FText UGSTN_AnimGraphNode_StanceOffset::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Adds the chosen upper-body stance (torso, neck, arms) on top of the game idle, only at rest: off during montages and while "
		"the capsule moves, back after a move per the stance asset. Passes the pose through otherwise. Reads UGSTN_CharacterAnimInstance; a.GSTN.StanceOffset 0 turns it off.");
}

FString UGSTN_AnimGraphNode_StanceOffset::GetNodeCategory() const
{
	return TEXT("GSTN");
}

FLinearColor UGSTN_AnimGraphNode_StanceOffset::GetNodeTitleColor() const
{
	return FLinearColor(0.4f, 0.3f, 0.6f);
}

#undef LOCTEXT_NAMESPACE

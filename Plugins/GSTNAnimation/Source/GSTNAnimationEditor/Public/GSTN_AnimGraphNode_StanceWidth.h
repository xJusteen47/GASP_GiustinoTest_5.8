// Editor node of FGSTN_AnimNode_StanceWidth (the node reads its widths from cvars and its weights from the pose curves).

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "GSTN_AnimNode_StanceWidth.h"
#include "GSTN_AnimGraphNode_StanceWidth.generated.h"

UCLASS()
class GSTNANIMATIONEDITOR_API UGSTN_AnimGraphNode_StanceWidth : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_AnimNode_StanceWidth Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

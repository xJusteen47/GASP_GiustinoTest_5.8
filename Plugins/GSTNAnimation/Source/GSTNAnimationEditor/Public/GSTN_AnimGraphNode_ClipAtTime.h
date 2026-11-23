// Editor node of FGSTN_AnimNode_ClipAtTime.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "GSTN_AnimNode_ClipAtTime.h"
#include "GSTN_AnimGraphNode_ClipAtTime.generated.h"

UCLASS()
class GSTNANIMATIONEDITOR_API UGSTN_AnimGraphNode_ClipAtTime : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_AnimNode_ClipAtTime Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

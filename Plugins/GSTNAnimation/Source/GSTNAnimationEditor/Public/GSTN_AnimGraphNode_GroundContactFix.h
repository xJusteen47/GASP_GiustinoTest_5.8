// Editor node of FGSTN_AnimNode_GroundContactFix (no feature knowledge: states and tuning come from its settings asset).

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "GSTN_AnimNode_GroundContactFix.h"
#include "GSTN_AnimGraphNode_GroundContactFix.generated.h"

UCLASS()
class GSTNANIMATIONEDITOR_API UGSTN_AnimGraphNode_GroundContactFix : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_AnimNode_GroundContactFix Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

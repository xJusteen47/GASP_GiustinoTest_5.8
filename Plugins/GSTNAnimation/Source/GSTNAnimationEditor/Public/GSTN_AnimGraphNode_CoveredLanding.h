// Editor node of FGSTN_AnimNode_CoveredLanding (no feature knowledge: the node reads its hold from UGSTN_CharacterAnimInstance).

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "GSTN_AnimNode_CoveredLanding.h"
#include "GSTN_AnimGraphNode_CoveredLanding.generated.h"

UCLASS()
class GSTNANIMATIONEDITOR_API UGSTN_AnimGraphNode_CoveredLanding : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_AnimNode_CoveredLanding Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

// Editor node of FGSTN_AnimNode_BlendOutFootLock (no feature knowledge: the node reads its request from UGSTN_CharacterAnimInstance).

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "GSTN_AnimNode_BlendOutFootLock.h"
#include "GSTN_AnimGraphNode_BlendOutFootLock.generated.h"

UCLASS()
class GSTNANIMATIONEDITOR_API UGSTN_AnimGraphNode_BlendOutFootLock : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_AnimNode_BlendOutFootLock Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

// Editor node of FGSTN_AnimNode_DirectionalHitAdditive (no combat knowledge: the node reads an IGSTN_AdditiveReactionSource).

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "GSTN_AnimNode_DirectionalHitAdditive.h"
#include "GSTN_AnimGraphNode_DirectionalHitAdditive.generated.h"

UCLASS()
class GSTNANIMATIONEDITOR_API UGSTN_AnimGraphNode_DirectionalHitAdditive : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_AnimNode_DirectionalHitAdditive Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

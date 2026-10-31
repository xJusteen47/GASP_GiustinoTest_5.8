// Interaction runtime: rules per kit or mode (interaction.md §9).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_InteractRulesAsset.generated.h"

/** Rules of the focus for one kit or mode. An interactor names one; none = the project settings' defaults */
UCLASS(BlueprintType)
class GSTNINTERACTRUNTIME_API UGSTN_InteractRulesAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
	FGSTN_InteractRules Rules;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

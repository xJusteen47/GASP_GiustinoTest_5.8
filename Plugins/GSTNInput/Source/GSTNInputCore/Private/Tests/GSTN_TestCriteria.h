// Priority criteria used only by the Core automation tests.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_PriorityCriteria.h"
#include "GSTN_TestCriteria.generated.h"

/** Scores 1 for the slots of one command and 0 for the others: a "this command first" rule written the way a game would. */
USTRUCT(meta = (Hidden))
struct FGSTN_PriorityCriterion_TestFavorite : public FGSTN_PriorityCriterion
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Favorite;

	virtual int32 Score(const FGSTN_PriorityContext& Context) const override
	{
		return Context.Slot.Command == Favorite ? 1 : 0;
	}
};

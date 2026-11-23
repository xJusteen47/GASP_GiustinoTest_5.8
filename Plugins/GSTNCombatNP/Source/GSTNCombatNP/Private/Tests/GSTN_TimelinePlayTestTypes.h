// Ability classes used only by the timeline node automation tests (Play Combat Timeline And Wait, Start Combat Move).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_TimelinePlayTestTypes.generated.h"

/** A host: plays nothing on activation; the test fills PlayableTimelines on the class default and restores it */
UCLASS(Hidden)
class UGSTN_TestTimelineHost : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_TestTimelineHost(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		bPlayTimelineOnActivation = false;
	}
};

/** A standard move (plays its own timeline on activation) */
UCLASS(Hidden)
class UGSTN_TestTimelineMove : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()
};

// A move action used only by the move action automation tests (CP2 of the Blueprint seams).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_MoveActions.h"
#include "GSTN_MoveActionTestTypes.generated.h"

/** Logs its Id with the key it ran for ("Id:KeyTagLeaf@TimelineFrame"); with bEndsMove it also says the move stopped (an event that replaced it) */
USTRUCT(meta = (Hidden))
struct FGSTN_TestMoveAction : public FGSTN_MoveAction
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	bool bEndsMove = false;

	/** What the actions ran, in order; the test clears it */
	static TArray<FString>& Log()
	{
		static TArray<FString> Entries;
		return Entries;
	}

	/** Whether the move still runs: an action with bEndsMove clears it */
	static bool& MoveRunning()
	{
		static bool bRunning = true;
		return bRunning;
	}

	virtual bool Execute(const FGSTN_MoveActionContext& Context) const override
	{
		Log().Add(FString::Printf(TEXT("%d@%d"), Id, Context.TimelineFrame));
		if (bEndsMove)
		{
			MoveRunning() = false;
		}
		return true;
	}
};

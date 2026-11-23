#include "GameplayDebugger/GSTN_GameplayDebuggerCategory_Combat.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU

#include "GameFramework/Actor.h"
#include "GameplayDebugger/GSTN_CombatDebugHitRecorder.h"
#include "GameplayDebugger/GSTN_CombatDebugText.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"

const FName FGSTN_GameplayDebuggerCategory_Combat::CategoryName(TEXT("GSTNCombat"));

FGSTN_GameplayDebuggerCategory_Combat::FGSTN_GameplayDebuggerCategory_Combat()
{
	bShowOnlyWithDebugActor = true;
}

TSharedRef<FGameplayDebuggerCategory> FGSTN_GameplayDebuggerCategory_Combat::MakeInstance()
{
	return MakeShared<FGSTN_GameplayDebuggerCategory_Combat>();
}

void FGSTN_GameplayDebuggerCategory_Combat::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UGSTN_CombatSimComponent* Combat = DebugActor ? DebugActor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	if (!Combat)
	{
		AddTextLine(TEXT("{grey}no combat component on the selected actor"));
		return;
	}
	const FGSTN_CombatSyncState* Sync = Combat->GetSyncState();
	if (!Sync)
	{
		AddTextLine(TEXT("{grey}no combat state yet"));
		return;
	}

	TArray<FString> Lines;
	GSTNCombatDebugText::DescribeCore(Sync->Core, Lines);
	if (Combat->GetGateClosedFrames() > 0)
	{
		Lines.Add(FString::Printf(TEXT("{red}gate closed %d frames (missing table set)"), Combat->GetGateClosedFrames()));
	}
	for (const FString& Line : Lines)
	{
		AddTextLine(Line);
	}

	// Input: the history in notation, held buttons and the last request (the component's own debug text)
	AddTextLine(TEXT("{green}input"));
	TArray<FString> InputLines;
	Combat->GetDebugString(8).ParseIntoArrayLines(InputLines);
	for (const FString& Line : InputLines)
	{
		AddTextLine(Line);
	}

	AddTextLine(TEXT("{green}last hits"));
	const UGSTN_CombatDebugHitRecorder* Recorder = UGSTN_CombatDebugHitRecorder::Get(DebugActor);
	const FGSTN_CombatHitRing* Ring = Recorder ? Recorder->FindRing(DebugActor) : nullptr;
	if (!Ring || Ring->Num() == 0)
	{
		AddTextLine(TEXT("{grey}none"));
	}
	else
	{
		for (int32 Index = 0; Index < Ring->Num(); ++Index)
		{
			const FGSTN_CombatHitRecord& Record = (*Ring)[Index];
			AddTextLine(FString::Printf(TEXT("%s%s"), Record.bDealt ? TEXT("{white}") : TEXT("{orange}"), *Record.ToLine()));
		}
	}

	// World: facing arrow at the hips and the timeline frame above the head
	const FVector Location = DebugActor->GetActorLocation();
	const FRotator Facing(0.0, Combat->GetFacingYaw(), 0.0);
	AddShape(FGameplayDebuggerShape::MakeArrow(Location, Location + Facing.Vector() * 100.0, 15.f, 2.f, FColor::Cyan));
	const FGSTN_CombatSyncCore& Core = Sync->Core;
	const int32 Frame = Core.Input.History.GetLatestFrame();
	const FString HeadText = Core.ActiveTimeline != INDEX_NONE && Core.TimelineStartFrame != INDEX_NONE
		? FString::Printf(TEXT("T%d f%d"), Core.ActiveTimeline, Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames)
		: GSTNCombatDebugText::ActionStateName(static_cast<uint8>(Core.ActionState));
	AddShape(FGameplayDebuggerShape::MakePoint(Location + FVector(0.0, 0.0, 120.0), 4.f, FColor::Yellow, HeadText));
}

#endif // WITH_GAMEPLAY_DEBUGGER_MENU

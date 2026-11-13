// E15: the combat Gameplay Debugger category's text, hit ring and recorder (combat-m5-editor.md 10.2).
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Events/GSTN_SimEvent.h"
#include "GameFramework/Actor.h"
#include "GameplayDebugger/GSTN_CombatDebugHitRecorder.h"
#include "GameplayDebugger/GSTN_CombatDebugText.h"
#include "GameplayDebugger/GSTN_GameplayDebuggerCategory_Combat.h"
#include "GSTN_CombatTags.h"
#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatSimTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_CombatHitRecord MakeRecord(int32 Frame)
	{
		FGSTN_CombatHitRecord Record;
		Record.Frame = Frame;
		return Record;
	}

	bool AnyContains(const TArray<FString>& Lines, const TCHAR* Text)
	{
		return Lines.ContainsByPredicate([Text](const FString& Line) { return Line.Contains(Text); });
	}

	/** A bare game world with its subsystems, destroyed by the destructor */
	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNCombatDebugTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}
		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugHitRingTest, "GSTNCombat.Debug.GameplayDebugger.HitRingKeepsLastEightNewestFirst",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugHitRingTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatHitRing Ring;
	for (int32 Frame = 1; Frame <= 3; ++Frame)
	{
		Ring.Add(MakeRecord(Frame));
	}
	TestEqual(TEXT("three hits kept"), Ring.Num(), 3);
	TestEqual(TEXT("newest first"), Ring[0].Frame, 3);
	TestEqual(TEXT("oldest last"), Ring[2].Frame, 1);
	for (int32 Frame = 4; Frame <= 11; ++Frame)
	{
		Ring.Add(MakeRecord(Frame));
	}
	TestEqual(TEXT("capped at the capacity"), Ring.Num(), FGSTN_CombatHitRing::Capacity);
	TestEqual(TEXT("newest after wrapping"), Ring[0].Frame, 11);
	TestEqual(TEXT("oldest kept is the eighth from the end"), Ring[FGSTN_CombatHitRing::Capacity - 1].Frame, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugDescribeCoreTest, "GSTNCombat.Debug.GameplayDebugger.DescribeCoreLines",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugDescribeCoreTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core;
	TArray<FString> Lines;
	GSTNCombatDebugText::DescribeCore(Core, Lines);
	TestTrue(TEXT("default state is actionable"), AnyContains(Lines, TEXT("Actionable")));
	TestTrue(TEXT("no timeline"), AnyContains(Lines, TEXT("timeline -")));
	TestTrue(TEXT("no hitstun"), AnyContains(Lines, TEXT("hitstun -")));

	const int32 Frame = Core.Input.History.GetLatestFrame();
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.ActiveTimeline = 2;
	Core.TimelineStartFrame = Frame - 10;
	Core.TimelinePausedFrames = 3;
	Core.Hit.HitstunEndFrame = Frame + 5;
	Lines.Reset();
	GSTNCombatDebugText::DescribeCore(Core, Lines);
	TestTrue(TEXT("action state named"), AnyContains(Lines, TEXT("InTimeline")));
	TestTrue(TEXT("timeline frame excludes paused frames"), AnyContains(Lines, TEXT("timeline 2  frame 7 (paused 3)")));
	TestTrue(TEXT("hitstun frames left"), AnyContains(Lines, TEXT("hitstun 5")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugHitRecorderTest, "GSTNCombat.Debug.GameplayDebugger.RecorderFilesHitsBySide",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugHitRecorderTest::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UGSTN_CombatDebugHitRecorder* Recorder = Test.World->GetSubsystem<UGSTN_CombatDebugHitRecorder>();
	if (!TestNotNull(TEXT("the recorder exists in a game world"), Recorder))
	{
		return false;
	}
	AActor* Attacker = Test.World->SpawnActor<AActor>();
	AActor* Victim = Test.World->SpawnActor<AActor>();

	FGSTN_SimEvent Event;
	Event.Frame = 42;
	Event.Instigator = Attacker;
	Event.Target = Victim;
	Event.Magnitude = 12.f;
	Event.EventType = GSTNCombatTags::Event_HitLanded;
	Recorder->Record(Event);
	Event.EventType = GSTNCombatTags::Event_HitReceived;
	Recorder->Record(Event);
	Event.EventType = GSTNCombatTags::Event_KO;
	Recorder->Record(Event);

	const FGSTN_CombatHitRing* Dealt = Recorder->FindRing(Attacker);
	const FGSTN_CombatHitRing* Taken = Recorder->FindRing(Victim);
	if (TestNotNull(TEXT("the attacker has a ring"), Dealt) && TestEqual(TEXT("one hit dealt, the KO ignored"), Dealt->Num(), 1))
	{
		TestTrue(TEXT("dealt"), (*Dealt)[0].bDealt);
		TestEqual(TEXT("names the victim"), (*Dealt)[0].Other, Victim->GetName());
		TestEqual(TEXT("frame"), (*Dealt)[0].Frame, 42);
		TestTrue(TEXT("line has the damage"), (*Dealt)[0].ToLine().Contains(TEXT("dmg 12.0")));
	}
	if (TestNotNull(TEXT("the victim has a ring"), Taken) && TestEqual(TEXT("one hit taken"), Taken->Num(), 1))
	{
		TestFalse(TEXT("taken"), (*Taken)[0].bDealt);
		TestEqual(TEXT("names the attacker"), (*Taken)[0].Other, Attacker->GetName());
	}
	return true;
}

#if WITH_GAMEPLAY_DEBUGGER_MENU
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugCategoryNoCombatTest, "GSTNCombat.Debug.GameplayDebugger.CategoryWithoutCombatComponent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugCategoryNoCombatTest::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	AActor* Actor = Test.World->SpawnActor<AActor>();
	const TSharedRef<FGameplayDebuggerCategory> Category = FGSTN_GameplayDebuggerCategory_Combat::MakeInstance();
	Category->CollectData(nullptr, Actor);
	TestTrue(TEXT("says the actor has no combat component"), AnyContains(Category->GetReplicatedLinesCopy(), TEXT("no combat component")));
	return true;
}
#endif

#endif // WITH_DEV_AUTOMATION_TESTS

// Automation tests of the cosmetic hooks (CP4 of the Blueprint seams): the window tracker of UGSTN_MoveCosmeticsComponent, stepped the way the
// component steps it from the presented combat state (through hitstop, a cancel, a correction, a slow screen), and the hit cue's parameters.

#include "GSTN_HitEffectContext.h"
#include "Misc/AutomationTest.h"
#include "Presentation/GSTN_GCN_CombatHit.h"
#include "Presentation/GSTN_MoveCosmetics.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MOVE_COSMETICS_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNMoveCosmeticsTest
{
	FGSTN_MoveCosmeticWindow MakeWindow(int32 Start, int32 End, int32 Tail = 0)
	{
		FGSTN_MoveCosmeticWindow Window;
		Window.StartFrame = Start;
		Window.EndFrame = End;
		Window.TailFrames = Tail;
		return Window;
	}

	/** Steps the tracker and logs "+w" for a began window, "-w" for a natural end, "!w" for an interrupted one, in delta order */
	struct FStepper
	{
		FGSTN_MoveCosmeticTracker Tracker;
		double Now = 0.0;

		FString Step(int32 Entry, int32 RunStart, int32 Frame, TConstArrayView<FGSTN_MoveCosmeticWindow> Windows, double Dt = 0.016)
		{
			Now += Dt;
			FGSTN_MoveCosmeticTracker::FDelta Delta;
			Tracker.Update(Entry, RunStart, Frame, Windows, Now, FGSTN_MoveCosmeticTracker::DefaultMemorySeconds, Delta);
			FString Log;
			for (const FGSTN_MoveCosmeticKey& Key : Delta.Began)
			{
				Log += FString::Printf(TEXT("+%d"), Key.Window);
			}
			for (const FGSTN_MoveCosmeticTracker::FEnded& Ended : Delta.Ended)
			{
				Log += FString::Printf(TEXT("%s%d"), Ended.bInterrupted ? TEXT("!") : TEXT("-"), Ended.Key.Window);
			}
			return Log;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveCosmeticsWindowTest, "GSTNCombat.NP.MoveCosmetics.WindowsAndTail", GSTN_MOVE_COSMETICS_TEST_FLAGS)
bool FGSTN_MoveCosmeticsWindowTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveCosmeticsTest;
	// 0: trail on the active frames 8-10 with a 2-frame tail; 1: whoosh one-shot on 5; 2: glow until the move ends
	const TArray<FGSTN_MoveCosmeticWindow> Windows = { MakeWindow(8, 10, 2), MakeWindow(5, 5), MakeWindow(3, -1) };
	TestTrue(TEXT("Tail is live"), Windows[0].IsLiveAt(12));
	TestFalse(TEXT("After the tail it is not"), Windows[0].IsLiveAt(13));

	FStepper S;
	FString Log;
	for (int32 Frame = 0; Frame <= 14; ++Frame)
	{
		Log += FString::Printf(TEXT("%d:%s "), Frame, *S.Step(4, 100, Frame, Windows));
	}
	TestEqual(TEXT("Glow at 3, whoosh 5 ends at 6, trail 8 ends after its tail at 13"), Log,
		FString(TEXT("0: 1: 2: 3:+2 4: 5:+1 6:-1 7: 8:+0 9: 10: 11: 12: 13:-0 14: ")));
	// The move ends: the glow lasted until the end, which is its natural end
	TestEqual(TEXT("Move over: the glow ends naturally"), S.Step(INDEX_NONE, INDEX_NONE, INDEX_NONE, {}), FString(TEXT("-2")));
	TestEqual(TEXT("Nothing left"), S.Tracker.NumActive(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveCosmeticsHitstopTest, "GSTNCombat.NP.MoveCosmetics.HitstopHoldsTheFrame", GSTN_MOVE_COSMETICS_TEST_FLAGS)
bool FGSTN_MoveCosmeticsHitstopTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveCosmeticsTest;
	const TArray<FGSTN_MoveCosmeticWindow> Windows = { MakeWindow(8, 9) };
	FStepper S;
	TestEqual(TEXT("Begins on 8"), S.Step(1, 50, 8, Windows), FString(TEXT("+0")));
	// Hitstop: the timeline frame stands still for many drawn frames; the window neither ends nor begins again
	for (int32 Index = 0; Index < 10; ++Index)
	{
		TestEqual(TEXT("Frozen on 9"), S.Step(1, 50, 9, Windows), FString());
	}
	TestEqual(TEXT("Ends after the hitstop"), S.Step(1, 50, 10, Windows), FString(TEXT("-0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveCosmeticsInterruptTest, "GSTNCombat.NP.MoveCosmetics.CancelAndCorrection", GSTN_MOVE_COSMETICS_TEST_FLAGS)
bool FGSTN_MoveCosmeticsInterruptTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveCosmeticsTest;
	const TArray<FGSTN_MoveCosmeticWindow> Jab1 = { MakeWindow(4, 6, 2) };
	const TArray<FGSTN_MoveCosmeticWindow> Jab2 = { MakeWindow(3, 5) };

	// Cancelled during the active frames into jab 2: the trail is interrupted (killed), jab 2's own windows run
	FStepper A;
	A.Step(1, 10, 4, Jab1);
	TestEqual(TEXT("Cut in its frames: interrupted"), A.Step(2, 15, 0, Jab2), FString(TEXT("!0")));
	TestEqual(TEXT("Jab 2's window begins"), A.Step(2, 15, 3, Jab2), FString(TEXT("+0")));

	// Cancelled during the tail only: the active part was done, the trail fades out naturally
	FStepper B;
	B.Step(1, 10, 4, Jab1);
	B.Step(1, 10, 7, Jab1);
	TestEqual(TEXT("Cut in its tail: natural"), B.Step(2, 18, 0, Jab2), FString(TEXT("-0")));

	// A correction drops the run for one drawn frame and brings it back (an NP resimulation): the window is not played twice
	FStepper C;
	TestEqual(TEXT("Begins"), C.Step(1, 10, 4, Jab1), FString(TEXT("+0")));
	TestEqual(TEXT("Dropped by the correction"), C.Step(INDEX_NONE, INDEX_NONE, INDEX_NONE, {}), FString(TEXT("!0")));
	TestEqual(TEXT("Back: no replay"), C.Step(1, 10, 5, Jab1), FString());
	TestEqual(TEXT("And no end for a window that is not playing"), C.Step(1, 10, 9, Jab1), FString());

	// A correction that moved the start of the move (another run): its windows are new
	TestEqual(TEXT("Another run of the same move plays again"), C.Step(1, 12, 4, Jab1), FString(TEXT("+0")));

	// Memory: long after, the same run key could begin again (the run is long over in practice)
	FStepper D;
	D.Step(1, 10, 4, Jab1);
	D.Step(INDEX_NONE, INDEX_NONE, INDEX_NONE, {});
	D.Step(INDEX_NONE, INDEX_NONE, INDEX_NONE, {}, 5.0);
	TestEqual(TEXT("Forgotten after the memory time"), D.Tracker.NumRemembered(), 0);

	// The frames went back inside the same run (a correction before the window): interrupted
	FStepper E;
	E.Step(1, 10, 4, Jab1);
	TestEqual(TEXT("Frame back before the window: interrupted"), E.Step(1, 10, 2, Jab1), FString(TEXT("!0")));

	// End all (the component stops)
	FStepper F;
	F.Step(1, 10, 5, Jab1);
	FGSTN_MoveCosmeticTracker::FDelta Delta;
	F.Tracker.EndAll(Delta);
	TestEqual(TEXT("End all: one interrupted"), Delta.Ended.Num(), 1);
	TestTrue(TEXT("End all: interrupted"), Delta.Ended.Num() == 1 && Delta.Ended[0].bInterrupted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveCosmeticsSlowScreenTest, "GSTNCombat.NP.MoveCosmetics.SlowScreenCrossing", GSTN_MOVE_COSMETICS_TEST_FLAGS)
bool FGSTN_MoveCosmeticsSlowScreenTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveCosmeticsTest;
	// A one-shot on 5 and a trail on 6-6: the game draws every third simulation frame (a slow machine), 3 -> 6 -> 9
	const TArray<FGSTN_MoveCosmeticWindow> Windows = { MakeWindow(5, 5), MakeWindow(6, 6) };
	FStepper S;
	TestEqual(TEXT("Frame 3: nothing yet"), S.Step(1, 10, 3, Windows), FString());
	TestEqual(TEXT("Frame 6: the skipped one-shot plays and ends, the trail begins"), S.Step(1, 10, 6, Windows), FString(TEXT("+1+0-0")));
	TestEqual(TEXT("Frame 9: the trail ends"), S.Step(1, 10, 9, Windows), FString(TEXT("-1")));
	TestEqual(TEXT("Frame 12: nothing replays"), S.Step(1, 10, 12, Windows), FString());

	// The first drawn frame of a run is already past a window: no crossing without a previous frame of the same run (it would play late)
	FStepper T;
	TestEqual(TEXT("First seen at 9: window 5 is not replayed late"), T.Step(1, 20, 9, Windows), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CombatHitCueParamsTest, "GSTNCombat.NP.MoveCosmetics.HitCueParameters", GSTN_MOVE_COSMETICS_TEST_FLAGS)
bool FGSTN_CombatHitCueParamsTest::RunTest(const FString& Parameters)
{
	FGameplayCueParameters Plain;
	FGSTN_CombatHitCueInfo Info;
	TestFalse(TEXT("A cue without a hit context is not a combat hit"), UGSTN_CombatCueLibrary::GetCombatHitInfo(nullptr, Plain, Info));
	const FGameplayCueParameters Same = UGSTN_GCN_CombatHit::MakeHitParameters(nullptr, Plain, true, true);
	TestTrue(TEXT("Without a context the parameters stay as they are"), FVector(Same.Location).IsZero());

	// The simulation's cue: only the context, as UAbilitySystemGlobals::InitGameplayCueParameters makes it
	FGameplayCueParameters FromSim;
	FromSim.EffectContext = FGSTN_HitEffectContext::Make(nullptr, nullptr, nullptr);
	FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandleMutable(FromSim.EffectContext);
	TestNotNull(TEXT("Hit context"), Context);
	if (!Context)
	{
		return false;
	}
	Context->HitLocation = FVector_NetQuantize10(FVector(120.f, -40.f, 90.f));
	Context->Reaction = EGSTN_HitReaction::Launch;
	Context->bStaggered = true;
	Context->HurtboxIndex = 2;

	const FGameplayCueParameters Placed = UGSTN_GCN_CombatHit::MakeHitParameters(nullptr, FromSim, true, true);
	TestTrue(TEXT("The Burst is placed at the hit"), FVector(Placed.Location).Equals(FVector(120.f, -40.f, 90.f), 0.2f));
	const FGameplayCueParameters NotPlaced = UGSTN_GCN_CombatHit::MakeHitParameters(nullptr, FromSim, false, true);
	TestTrue(TEXT("Place At Hit off: like a plain Burst"), FVector(NotPlaced.Location).IsZero());

	// The attacker's prediction already set its own location: it is kept
	FGameplayCueParameters Predicted = FromSim;
	Predicted.Location = FVector(1.f, 2.f, 3.f);
	TestTrue(TEXT("A location already set wins"), FVector(UGSTN_GCN_CombatHit::MakeHitParameters(nullptr, Predicted, true, true).Location).Equals(FVector(1.f, 2.f, 3.f), 0.2f));

	TestTrue(TEXT("Hit info"), UGSTN_CombatCueLibrary::GetCombatHitInfo(nullptr, FromSim, Info));
	TestTrue(TEXT("Reaction"), Info.Reaction == EGSTN_HitReaction::Launch);
	TestTrue(TEXT("Staggered"), Info.bStaggered);
	TestEqual(TEXT("Hurtbox"), Info.HurtboxIndex, 2);
	TestTrue(TEXT("Location"), Info.Location.Equals(FVector(120.f, -40.f, 90.f), 0.2f));
	return true;
}

#endif

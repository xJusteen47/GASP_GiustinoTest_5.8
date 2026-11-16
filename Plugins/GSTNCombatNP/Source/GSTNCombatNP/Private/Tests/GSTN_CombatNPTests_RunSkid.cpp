// Automation tests of the run skid move (docs/notes/corsa-mario-piano.md, P2): its phase and tags come from its own serialized fields (so a
// rollback restores them), and it survives the network. The full loop (stick reversal, brake, ledge, exit turn, two clients) is the PIE playtest.

#include "GSTN_CombatNPTags.h"
#include "Misc/AutomationTest.h"
#include "Movement/GSTN_LayeredMove_RunSkid.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	FGSTN_LayeredMove_RunSkid MakeRunningSkid(int32 LiveFrame)
	{
		FGSTN_LayeredMove_RunSkid Skid;
		Skid.Life = static_cast<uint8>(EGSTN_RunSkidLife::Running);
		Skid.Direction = FVector(1.0, 0.0, 0.0);
		Skid.TurnDirection = FVector(-1.0, 0.0, 0.0);
		Skid.StartSpeed = 600.f;
		Skid.Brake = 2500.f;
		Skid.EnterFrames = 4;
		Skid.ExitFrames = 8;
		Skid.ExitSpeed = 250.f;
		Skid.JumpWindowStart = 3;
		Skid.JumpWindowEnd = 6;
		Skid.RestFrame = 15;
		Skid.CurrentLiveFrame = LiveFrame;
		Skid.LiveFrames = LiveFrame + 1;
		Skid.LastStepTimeMs = 1234.0;
		return Skid;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunSkidMoveTagsTest, "GSTNCombat.NP.RunSkid.Tags", GSTN_NP_TEST_FLAGS)
bool FGSTN_RunSkidMoveTagsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Skid = GSTNCombatNPTags::Movement_State_Skid;
	const FGameplayTag Window = GSTNCombatNPTags::Movement_State_Skid_JumpWindow;

	const FGSTN_LayeredMove_RunSkid Early = MakeRunningSkid(0);
	TestEqual(TEXT("frame 0 is the enter"), Early.GetPhase(), EGSTN_RunSkidPhase::Enter);
	TestTrue(TEXT("skid tag"), Early.HasGameplayTag(Skid, true));
	TestFalse(TEXT("no window yet"), Early.HasGameplayTag(Window, true));

	const FGSTN_LayeredMove_RunSkid InWindow = MakeRunningSkid(2);
	TestTrue(TEXT("window tag (shifted one frame for the abilities)"), InWindow.HasGameplayTag(Window, true));
	TestTrue(TEXT("the window implies the skid (parent match)"), InWindow.HasGameplayTag(Skid, false));

	const FGSTN_LayeredMove_RunSkid Exit = MakeRunningSkid(16);
	TestEqual(TEXT("at rest: exit"), Exit.GetPhase(), EGSTN_RunSkidPhase::Exit);
	TestEqual(TEXT("no slide speed in the exit"), Exit.GetSlideSpeed(0.016f), 0.f);

	FGSTN_LayeredMove_RunSkid Done = MakeRunningSkid(5);
	Done.Life = static_cast<uint8>(EGSTN_RunSkidLife::Done);
	TestFalse(TEXT("a finished skid has no tag"), Done.HasGameplayTag(Skid, false));
	TestEqual(TEXT("a finished skid has no phase"), Done.GetPhase(), EGSTN_RunSkidPhase::None);

	FGSTN_LayeredMove_RunSkid Frozen = MakeRunningSkid(5);
	Frozen.CurrentLiveFrame = INDEX_NONE;
	TestEqual(TEXT("a frozen step keeps the last live frame's phase"), Frozen.GetPhase(), EGSTN_RunSkidPhase::Active);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunSkidMoveNetTest, "GSTNCombat.NP.RunSkid.NetSerialize", GSTN_NP_TEST_FLAGS)
bool FGSTN_RunSkidMoveNetTest::RunTest(const FString& Parameters)
{
	FGSTN_LayeredMove_RunSkid Sent = MakeRunningSkid(7);
	Sent.bStopAtLedge = false;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Sent.NetSerialize(Writer);

	FGSTN_LayeredMove_RunSkid Got;
	FMemoryReader Reader(Bytes);
	Got.NetSerialize(Reader);
	TestEqual(TEXT("life"), Got.Life, Sent.Life);
	TestFalse(TEXT("ledge rule"), Got.bStopAtLedge);
	TestTrue(TEXT("directions"), Got.Direction.Equals(Sent.Direction) && Got.TurnDirection.Equals(Sent.TurnDirection));
	TestEqual(TEXT("start speed"), Got.StartSpeed, Sent.StartSpeed);
	TestEqual(TEXT("brake"), Got.Brake, Sent.Brake);
	TestEqual(TEXT("rest frame"), Got.RestFrame, Sent.RestFrame);
	TestEqual(TEXT("live frame"), Got.CurrentLiveFrame, Sent.CurrentLiveFrame);
	TestEqual(TEXT("window end"), Got.JumpWindowEnd, Sent.JumpWindowEnd);
	TestEqual(TEXT("same phase"), Got.GetPhase(), Sent.GetPhase());
	TestEqual(TEXT("same speed"), Got.GetSlideSpeed(0.016f), Sent.GetSlideSpeed(0.016f));
	return true;
}

#endif

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Timeline/GSTN_TimelinePhases.h"

#if WITH_DEV_AUTOMATION_TESTS

// Derived frame data (E3 of the M5 plan). The tracks are the real FGSTN_TimelineTrack_Hitbox with real hitbox specs, not a stand-in.

namespace GSTNPhasesTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	/** A hitbox track over [Start, End) with one real hitbox spec */
	FTrackInstance MakeHitbox(int32 Start, int32 End, bool bWithSpec = true)
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		if (bWithSpec)
		{
			FGSTN_HitboxSpec Spec;
			Spec.Shape.Type = EGSTN_ShapeType::Sphere;
			FGSTN_HitboxKey Key;
			Key.Frame = 0;
			Spec.Keys.Add(Key);
			Track.Hitboxes.Add(Spec);
		}
		return FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Track);
	}

	FGSTN_CombatTimelineData MakeData(int32 Total, int32 Faf)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = Total;
		Data.FirstActionableFrame = Faf;
		return Data;
	}

	void IgnoreAll(FGSTN_HitboxSpec& Spec)
	{
		const FGameplayTag Relationships[] = {
			GSTNCombatHitTags::Relationship_Self.GetTag(), GSTNCombatHitTags::Relationship_Ally.GetTag(),
			GSTNCombatHitTags::Relationship_Enemy.GetTag(), GSTNCombatHitTags::Relationship_Neutral.GetTag() };
		for (const FGameplayTag& Relationship : Relationships)
		{
			FGSTN_HitVariant Variant;
			Variant.bIgnore = true;
			Spec.RelationshipVariants.Add(Relationship, Variant);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesNoHitboxTest, "GSTNCombat.Core.TimelinePhases.NoHitbox", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesNoHitboxTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(20, 14);
	// Other kinds of track never make a phase
	Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Movement>());
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestFalse(TEXT("no active"), Phases.bHasActive);
	TestEqual(TEXT("no startup"), Phases.GetStartupFrames(), 0);
	TestEqual(TEXT("no active frames"), Phases.GetActiveFrames(), 0);
	TestEqual(TEXT("no recovery"), Phases.GetRecoveryFrames(), 0);
	TestEqual(TEXT("no first hit frame"), Phases.FirstHitFrame1Based, 0);
	TestTrue(TEXT("no ranges"), Phases.ActiveRanges.IsEmpty());
	TestEqual(TEXT("the FAF is still the end of the busy stretch"), Phases.RecoveryEnd, 14);
	TestEqual(TEXT("summary"), Phases.FormatSummary(62.5f).ToString(), FString(TEXT("No hitbox | FAF 14 | Total 20 (320 ms)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesOneWindowTest, "GSTNCombat.Core.TimelinePhases.OneWindow", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesOneWindowTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(30, 24);
	Data.Tracks.Add(MakeHitbox(7, 10));
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestTrue(TEXT("active"), Phases.bHasActive);
	TestEqual(TEXT("startup end"), Phases.StartupEnd, 7);
	TestEqual(TEXT("active start"), Phases.ActiveStart, 7);
	TestEqual(TEXT("active end is exclusive"), Phases.ActiveEnd, 10);
	TestEqual(TEXT("startup frames"), Phases.GetStartupFrames(), 7);
	TestEqual(TEXT("active frames"), Phases.GetActiveFrames(), 3);
	TestEqual(TEXT("recovery frames"), Phases.GetRecoveryFrames(), 14);
	TestEqual(TEXT("recovery ends at the FAF"), Phases.RecoveryEnd, 24);
	TestEqual(TEXT("the first hit frame is 1-based"), Phases.FirstHitFrame1Based, 8);
	TestEqual(TEXT("total"), Phases.Total, 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesTwoDisjointTest, "GSTNCombat.Core.TimelinePhases.TwoDisjoint", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesTwoDisjointTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(40, 30);
	// Added out of order on purpose
	Data.Tracks.Add(MakeHitbox(20, 22));
	Data.Tracks.Add(MakeHitbox(6, 9));
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("two ranges"), Phases.ActiveRanges.Num(), 2);
	if (Phases.ActiveRanges.Num() == 2)
	{
		TestTrue(TEXT("[6,9) first"), Phases.ActiveRanges[0] == FIntPoint(6, 9));
		TestTrue(TEXT("[20,22) second"), Phases.ActiveRanges[1] == FIntPoint(20, 22));
	}
	TestEqual(TEXT("active spans the gap"), Phases.GetActiveFrames(), 16);
	TestEqual(TEXT("recovery after the last range"), Phases.GetRecoveryFrames(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesOverlappingMergedTest, "GSTNCombat.Core.TimelinePhases.OverlappingMerged", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesOverlappingMergedTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(40, 30);
	Data.Tracks.Add(MakeHitbox(6, 10));
	Data.Tracks.Add(MakeHitbox(8, 11));  // overlaps
	Data.Tracks.Add(MakeHitbox(11, 12)); // touches
	Data.Tracks.Add(MakeHitbox(7, 8));   // inside
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("one range"), Phases.ActiveRanges.Num(), 1);
	if (Phases.ActiveRanges.Num() == 1)
	{
		TestTrue(TEXT("[6,12)"), Phases.ActiveRanges[0] == FIntPoint(6, 12));
	}

	// An empty window (End <= Start) is not a window
	FGSTN_CombatTimelineData Empty = MakeData(40, 30);
	Empty.Tracks.Add(MakeHitbox(9, 9));
	TestFalse(TEXT("a zero length window is ignored"), FGSTN_TimelinePhases::Compute(Empty).bHasActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesHitboxAtFrameZeroTest, "GSTNCombat.Core.TimelinePhases.HitboxAtFrameZero", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesHitboxAtFrameZeroTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(20, 10);
	Data.Tracks.Add(MakeHitbox(0, 4));
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("no startup"), Phases.GetStartupFrames(), 0);
	TestEqual(TEXT("the first hit frame is frame 1"), Phases.FirstHitFrame1Based, 1);
	TestEqual(TEXT("active"), Phases.GetActiveFrames(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesFafEqualsTotalTest, "GSTNCombat.Core.TimelinePhases.FafEqualsTotal", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesFafEqualsTotalTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(30, 30);
	Data.Tracks.Add(MakeHitbox(5, 8));
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("recovery ends at the total"), Phases.RecoveryEnd, Phases.Total);
	TestEqual(TEXT("recovery frames"), Phases.GetRecoveryFrames(), 22);

	// A hitbox that runs past the FAF leaves no recovery, and never a negative one
	FGSTN_CombatTimelineData Late = MakeData(30, 6);
	Late.Tracks.Add(MakeHitbox(5, 8));
	TestEqual(TEXT("no negative recovery"), FGSTN_TimelinePhases::Compute(Late).GetRecoveryFrames(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesTailStartIsNotRecoveryTest, "GSTNCombat.Core.TimelinePhases.TailStartIsNotRecovery", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesTailStartIsNotRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(40, 20);
	Data.Tracks.Add(MakeHitbox(5, 8));
	const FGSTN_TimelinePhases Without = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("no tail"), Without.TailStart, static_cast<int32>(INDEX_NONE));

	Data.Tail.bHasTail = true;
	Data.Tail.StartFrame = 26;
	Data.Tail.BlendOutFrames = 6;
	const FGSTN_TimelinePhases With = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("tail start as authored"), With.TailStart, 26);
	TestEqual(TEXT("blend out"), With.TailBlendOutFrames, 6);
	TestEqual(TEXT("the tail does not change recovery"), With.GetRecoveryFrames(), Without.GetRecoveryFrames());
	TestEqual(TEXT("the tail does not move the FAF"), With.RecoveryEnd, 20);

	Data.Tail.StartFrame = INDEX_NONE;
	TestEqual(TEXT("a negative start is the FAF"), FGSTN_TimelinePhases::Compute(Data).TailStart, 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesMutedVariantsTest, "GSTNCombat.Core.TimelinePhases.MutedByVariantIgnored", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesMutedVariantsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;

	FGSTN_TimelineTrack_Hitbox Muted = *MakeHitbox(10, 14).GetPtr<FGSTN_TimelineTrack_Hitbox>();
	IgnoreAll(Muted.Hitboxes[0]);

	FGSTN_CombatTimelineData Data = MakeData(30, 20);
	Data.Tracks.Add(MakeHitbox(4, 6));
	Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Muted));
	FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("the window that hits nobody is not active"), Phases.ActiveRanges.Num(), 1);
	TestEqual(TEXT("active frames are the other window"), Phases.GetActiveFrames(), 2);

	// Ignoring three of the four relationships leaves it active: it still hits neutrals
	Muted.Hitboxes[0].RelationshipVariants.Remove(GSTNCombatHitTags::Relationship_Neutral.GetTag());
	Data.Tracks[1] = FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Muted);
	TestEqual(TEXT("a partial mute still counts"), FGSTN_TimelinePhases::Compute(Data).ActiveRanges.Num(), 2);

	// A second, unmuted spec keeps a fully muted one's track active
	FGSTN_TimelineTrack_Hitbox Mixed = *MakeHitbox(10, 14).GetPtr<FGSTN_TimelineTrack_Hitbox>();
	IgnoreAll(Mixed.Hitboxes[0]);
	Mixed.Hitboxes.Add(FGSTN_HitboxSpec());
	Data.Tracks[1] = FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Mixed);
	TestEqual(TEXT("one live spec is enough"), FGSTN_TimelinePhases::Compute(Data).ActiveRanges.Num(), 2);

	// A track with no spec at all is a shell being authored: it counts (the validator reports it)
	FGSTN_CombatTimelineData Shell = MakeData(30, 20);
	Shell.Tracks.Add(MakeHitbox(4, 6, false));
	TestTrue(TEXT("a shell counts"), FGSTN_TimelinePhases::Compute(Shell).bHasActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesSummaryStringTest, "GSTNCombat.Core.TimelinePhases.SummaryString", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesSummaryStringTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	FGSTN_CombatTimelineData Data = MakeData(30, 24);
	Data.Tracks.Add(MakeHitbox(7, 10));
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("62.5 Hz"), Phases.FormatSummary(62.5f).ToString(), FString(TEXT("Startup 7 | Active 3 | Recovery 14 | FAF 24 | Total 30 (480 ms)")));
	TestEqual(TEXT("60 Hz"), Phases.FormatSummary(60.f).ToString(), FString(TEXT("Startup 7 | Active 3 | Recovery 14 | FAF 24 | Total 30 (500 ms)")));
	TestTrue(TEXT("an unknown tick rate shows 0 ms, no division"), Phases.FormatSummary(0.f).ToString().Contains(TEXT("(0 ms)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PhasesNoAllocationTest, "GSTNCombat.Core.TimelinePhases.NoAllocationBeyondOutput", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PhasesNoAllocationTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPhasesTest;
	// Nothing to report: the output array is never allocated
	FGSTN_CombatTimelineData Bare = MakeData(30, 20);
	TestEqual(TEXT("no hitbox, no allocation"), FGSTN_TimelinePhases::Compute(Bare).ActiveRanges.GetSlack(), 0);

	// Four windows merging into one: the array is sized once for the windows, not grown while merging
	FGSTN_CombatTimelineData Data = MakeData(30, 20);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Data.Tracks.Add(MakeHitbox(Index * 2, Index * 2 + 3));
	}
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);
	TestEqual(TEXT("merged to one"), Phases.ActiveRanges.Num(), 1);
	TestTrue(TEXT("the reservation is at most the window count"), Phases.ActiveRanges.GetSlack() + Phases.ActiveRanges.Num() <= 8);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

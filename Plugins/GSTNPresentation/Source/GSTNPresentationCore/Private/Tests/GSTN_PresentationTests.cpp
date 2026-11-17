// Tests of the presentation events (Plugins/GSTNPresentation/README.md): GSTNPresentation.Core.*
//
// Pure: transient tables and components without a world, snapshots built by hand. The resimulation test replays the same frames twice,
// as the presented state does after a correction, and counts what played.

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_PresentationComponent.h"
#include "GSTN_PresentationLogic.h"
#include "GSTN_PresentationTags.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace GSTNPresentationTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static FGSTN_PresentationCue MakeCue(const FGameplayTag& Tag, float MinMagnitude = 0.f)
	{
		FGSTN_PresentationCue Cue;
		Cue.EventTag = Tag;
		Cue.MinMagnitude = MinMagnitude;
		return Cue;
	}

	static UGSTN_PresentationTable* MakeTable(TArray<FGSTN_PresentationCue> Cues, UGSTN_PresentationTable* Parent = nullptr)
	{
		UGSTN_PresentationTable* Table = NewObject<UGSTN_PresentationTable>(GetTransientPackage());
		Table->Cues = MoveTemp(Cues);
		Table->Parent = Parent;
		return Table;
	}

	static FGSTN_PresentationSnapshot Snap(int32 Frame, bool bGrounded, const FVector& Velocity = FVector::ZeroVector)
	{
		FGSTN_PresentationSnapshot S;
		S.bValid = true;
		S.Frame = Frame;
		S.bGrounded = bGrounded;
		S.Velocity = Velocity;
		return S;
	}

	static int32 Count(const TArray<FGSTN_PresentationEvent>& Events, const FGameplayTag& Tag)
	{
		return Events.FilterByPredicate([&Tag](const FGSTN_PresentationEvent& E) { return E.EventTag == Tag; }).Num();
	}

	/** Steps the deriver through the snapshots and returns every event */
	static TArray<FGSTN_PresentationEvent> Run(const TArray<FGSTN_PresentationSnapshot>& Frames, const FGSTN_PresentationDeriveRules& Rules = FGSTN_PresentationDeriveRules())
	{
		FGSTN_PresentationDeriver Deriver;
		TArray<FGSTN_PresentationEvent> Out;
		for (const FGSTN_PresentationSnapshot& S : Frames)
		{
			Deriver.Step(S, Rules, Out);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationScaleTest, "GSTNPresentation.Core.Scale", GSTNPresentationTest::Flags)
bool FGSTN_PresentationScaleTest::RunTest(const FString& Parameters)
{
	FGSTN_PresentationScale Scale;
	TestEqual(TEXT("default plays every event at 1"), Scale.Evaluate(1234.f), 1.f);

	Scale.InputRange = FVector2D(200.0, 1200.0);
	Scale.OutputRange = FVector2D(0.5, 2.0);
	TestEqual(TEXT("bottom"), Scale.Evaluate(200.f), 0.5f);
	TestEqual(TEXT("top"), Scale.Evaluate(1200.f), 2.f);
	TestEqual(TEXT("middle, linear"), Scale.Evaluate(700.f), 1.25f);
	TestEqual(TEXT("clamped below"), Scale.Evaluate(0.f), 0.5f);
	TestEqual(TEXT("clamped above"), Scale.Evaluate(5000.f), 2.f);

	Scale.Exponent = 2.f;
	TestEqual(TEXT("exponent 2 at the middle"), Scale.Evaluate(700.f), 0.875f);

	Scale.Exponent = 1.f;
	Scale.bClampInput = false;
	TestEqual(TEXT("extrapolates"), Scale.Evaluate(2200.f), 3.5f);

	Scale.InputRange = FVector2D(10.0, 10.0);
	TestEqual(TEXT("empty range: a step"), Scale.Evaluate(9.f), 0.5f);
	TestEqual(TEXT("empty range: a step (top)"), Scale.Evaluate(10.f), 2.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationLookupTest, "GSTNPresentation.Core.Lookup", GSTNPresentationTest::Flags)
bool FGSTN_PresentationLookupTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTest;
	using namespace GSTNPresentationTags;

	UGSTN_PresentationTable* Base = MakeTable({MakeCue(Jump), MakeCue(Land), MakeCue(Skid)});
	UGSTN_PresentationTable* Kit = MakeTable({MakeCue(Jump_Air), MakeCue(Hit)}, Base);

	TestTrue(TEXT("exact row"), Kit->FindCue(Jump_Air) && Kit->FindCue(Jump_Air)->EventTag == Jump_Air);
	TestTrue(TEXT("ancestor row in the parent"), Kit->FindCue(Jump_Ground) && Kit->FindCue(Jump_Ground)->EventTag == Jump);
	TestTrue(TEXT("parent fallback"), Kit->FindCue(Land) && Kit->FindCue(Land)->EventTag == Land);
	TestTrue(TEXT("Skid.Enter -> Skid"), Kit->FindCue(Skid_Enter) && Kit->FindCue(Skid_Enter)->EventTag == Skid);
	TestNull(TEXT("no row anywhere"), Kit->FindCue(Turn180));

	UGSTN_PresentationTable* Specific = MakeTable({MakeCue(Event), MakeCue(Jump), MakeCue(Jump_Ground)});
	TestTrue(TEXT("most specific ancestor wins"), Specific->FindOwnCue(Jump_Ground)->EventTag == Jump_Ground);
	TestTrue(TEXT("Jump.Air -> Jump, not Event"), Specific->FindOwnCue(Jump_Air)->EventTag == Jump);

	UGSTN_PresentationTable* Loop = MakeTable({});
	Loop->Parent = Loop;
	TestNull(TEXT("a parent loop ends"), Loop->FindCue(Jump));

	// Component stack: skin (priority 10) over kit (0) over the default table
	UGSTN_PresentationComponent* Component = NewObject<UGSTN_PresentationComponent>(GetTransientPackage());
	UGSTN_PresentationTable* Skin = MakeTable({MakeCue(Jump)});
	Component->DefaultTable = MakeTable({MakeCue(Turn180)});
	Component->SetTableLayer(TEXT("Kit"), Kit, 0);
	Component->SetTableLayer(TEXT("Skin"), Skin, 10);
	TestTrue(TEXT("skin answers first"), Component->ResolveCue(Jump_Air) == &Skin->Cues[0]);
	TestTrue(TEXT("kit answers what the skin lacks"), Component->ResolveCue(Hit) == &Kit->Cues[1]);
	TestTrue(TEXT("default table last"), Component->ResolveCue(Turn180) && Component->ResolveCue(Turn180)->EventTag == Turn180);
	Component->SetTableLayer(TEXT("Skin"), nullptr);
	TestTrue(TEXT("skin removed"), Component->ResolveCue(Jump_Air) == &Kit->Cues[0]);

	// Threshold: below MinMagnitude the cue does not play
	FGSTN_PresentationCue Dust = MakeCue(Land, 300.f);
	FGSTN_PresentationEvent Soft;
	Soft.EventTag = Land;
	Soft.Magnitude = 200.f;
	TestTrue(TEXT("soft landing: no dust"), UGSTN_PresentationComponent::CueIntensity(&Dust, Soft) < 0.f);
	Soft.Magnitude = 400.f;
	TestEqual(TEXT("hard landing: intensity 1"), UGSTN_PresentationComponent::CueIntensity(&Dust, Soft), 1.f);
	TestEqual(TEXT("no cue: intensity 1"), UGSTN_PresentationComponent::CueIntensity(nullptr, Soft), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationDedupeTest, "GSTNPresentation.Core.Dedupe", GSTNPresentationTest::Flags)
bool FGSTN_PresentationDedupeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTags;
	FGSTN_PresentationDedupe Dedupe;
	Dedupe.FrameWindow = 2;
	Dedupe.MemoryFrames = 60;

	TestTrue(TEXT("first"), Dedupe.Accept(Land, 100));
	TestFalse(TEXT("same event, same frame (resimulation)"), Dedupe.Accept(Land, 100));
	TestFalse(TEXT("same event, moved one frame by the correction"), Dedupe.Accept(Land, 101));
	TestFalse(TEXT("same event, two frames before"), Dedupe.Accept(Land, 98));
	TestTrue(TEXT("other event, same frame"), Dedupe.Accept(Jump_Ground, 100));
	TestTrue(TEXT("same event three frames later: a new one"), Dedupe.Accept(Land, 103));

	TestTrue(TEXT("hit stamped by the simulation"), Dedupe.Accept(Hit, 200, true));
	TestTrue(TEXT("a second hit one frame later (multi-hit)"), Dedupe.Accept(Hit, 201, true));
	TestFalse(TEXT("the same hit again"), Dedupe.Accept(Hit, 200, true));

	TestTrue(TEXT("no frame: always"), Dedupe.Accept(Land, INDEX_NONE));
	TestTrue(TEXT("no frame: always (again)"), Dedupe.Accept(Land, INDEX_NONE));

	Dedupe.Accept(Jump_Air, 400);
	TestTrue(TEXT("old entries forgotten"), Dedupe.Num() <= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationDeriveAirTest, "GSTNPresentation.Core.Derive.Air", GSTNPresentationTest::Flags)
bool FGSTN_PresentationDeriveAirTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTest;
	using namespace GSTNPresentationTags;

	// Jump, air jump, land
	TArray<FGSTN_PresentationSnapshot> Frames = {Snap(1, true), Snap(2, false, FVector(0, 0, 900)), Snap(3, false, FVector(0, 0, 500))};
	FGSTN_PresentationSnapshot AirJump = Snap(4, false, FVector(0, 0, 800));
	AirJump.AirJumpsUsed = 1;
	Frames.Add(AirJump);
	FGSTN_PresentationSnapshot Falling = Snap(5, false, FVector(0, 0, -700));
	Falling.AirJumpsUsed = 1;
	Frames.Add(Falling);
	Frames.Add(Snap(6, true));
	Frames[0].Location = FVector(100, 0, 0);
	Frames[1].Location = FVector(100, 0, 15);  // first airborne frame: already above the floor
	TArray<FGSTN_PresentationEvent> Events = Run(Frames);
	TestEqual(TEXT("one ground jump"), Count(Events, Jump_Ground), 1);
	const FGSTN_PresentationEvent* Takeoff = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::Jump_Ground; });
	TestTrue(TEXT("jump at the last grounded location"), Takeoff && Takeoff->Location.Equals(Frames[0].Location));
	TestEqual(TEXT("one air jump"), Count(Events, Jump_Air), 1);
	TestEqual(TEXT("one landing"), Count(Events, Land), 1);
	const FGSTN_PresentationEvent* Landing = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::Land; });
	TestTrue(TEXT("landing magnitude = fall speed"), Landing && FMath::IsNearlyEqual(Landing->Magnitude, 700.f) && Landing->Frame == 6);

	// Walking off a ledge and a step down: nothing
	Events = Run({Snap(1, true), Snap(2, false, FVector(300, 0, -10)), Snap(3, true, FVector(300, 0, 0))});
	TestEqual(TEXT("ledge: no jump"), Count(Events, Jump_Ground), 0);
	TestEqual(TEXT("step: no landing"), Count(Events, Land), 0);

	// A quiet state (downed): its small bounce makes no jump and no landing
	FGSTN_PresentationDeriveRules Quiet;
	Quiet.QuietAirTags.AddTag(GSTNPresentationTags::Shield);
	FGSTN_PresentationSnapshot Q1 = Snap(1, true);
	Q1.StateTags.AddTag(GSTNPresentationTags::Shield);
	FGSTN_PresentationSnapshot Q2 = Snap(2, false, FVector(0, 0, 500));
	Q2.StateTags = Q1.StateTags;
	FGSTN_PresentationSnapshot Q3 = Snap(3, true);
	Q3.StateTags = Q1.StateTags;
	Q3.Velocity = FVector::ZeroVector;
	FGSTN_PresentationSnapshot Q2b = Q2;
	Q2b.Velocity = FVector(0, 0, -500);
	Q2b.Frame = 3;
	Q3.Frame = 4;
	Events = Run({Q1, Q2, Q2b, Q3}, Quiet);
	TestEqual(TEXT("quiet: no jump"), Count(Events, Jump_Ground), 0);
	TestEqual(TEXT("quiet: no landing"), Count(Events, Land), 0);

	// The first snapshot is only the reference
	Events = Run({Snap(1, false, FVector(0, 0, -900))});
	TestEqual(TEXT("first frame: nothing"), Events.Num(), 0);

	// A tumbling victim hitting the ground bounces, it does not land; a reflected horizontal speed is a wall bounce
	FGSTN_PresentationSnapshot A = Snap(10, false, FVector(0, 0, -900));
	A.bReacting = true;
	FGSTN_PresentationSnapshot B = Snap(11, false, FVector(0, 0, 400));
	B.bReacting = true;
	FGSTN_PresentationSnapshot C = Snap(12, false, FVector(800, 300, 100));
	C.bReacting = true;
	FGSTN_PresentationSnapshot D = Snap(13, false, FVector(-500, 300, 80));
	D.bReacting = true;
	Events = Run({A, B, C, D});
	TestEqual(TEXT("ground bounce"), Count(Events, GroundBounce), 1);
	TestEqual(TEXT("no landing in a reaction"), Count(Events, Land), 0);
	TestEqual(TEXT("wall bounce"), Count(Events, WallBounce), 1);
	const FGSTN_PresentationEvent* Wall = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::WallBounce; });
	TestTrue(TEXT("wall normal = -X, magnitude = speed into it"), Wall && Wall->Direction.Equals(FVector(-1, 0, 0), 0.01) && FMath::IsNearlyEqual(Wall->Magnitude, 800.f));

	// A new hit that turns the velocity around is a hit, never a bounce
	FGSTN_PresentationSnapshot E = D;
	E.Frame = 14;
	E.Velocity = FVector(900, 300, 80);
	E.LastHitFrame = 14;
	E.LastHitStrength = 40.f;
	Events = Run({C, D, E});
	TestEqual(TEXT("re-hit: a hit"), Count(Events, Hit), 1);
	TestEqual(TEXT("re-hit: no extra bounce"), Count(Events, WallBounce), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationDeriveCountedBounceTest, "GSTNPresentation.Core.Derive.CountedBounce", GSTNPresentationTest::Flags)
bool FGSTN_PresentationDeriveCountedBounceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTest;
	using namespace GSTNPresentationTags;

	auto Reacting = [](int32 Frame, const FVector& Velocity, int32 Ground, int32 Wall)
	{
		FGSTN_PresentationSnapshot S = Snap(Frame, false, Velocity);
		S.bReacting = true;
		S.GroundBounceCount = Ground;
		S.WallBounceCount = Wall;
		return S;
	};

	// A spike off the ground (polish M2 r2): the victim is grounded and still, the next presented frame it already rises; the downward
	// speed never reached a snapshot. The simulation's count says it bounced
	FGSTN_PresentationSnapshot Still = Reacting(10, FVector::ZeroVector, 0, 0);
	Still.bGrounded = true;
	TArray<FGSTN_PresentationEvent> Events = Run({Still, Reacting(11, FVector(900, 0, 1600), 1, 0), Reacting(12, FVector(880, 0, 1580), 1, 0)});
	TestEqual(TEXT("spike: one ground bounce from the count"), Count(Events, GroundBounce), 1);
	const FGSTN_PresentationEvent* Spike = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::GroundBounce; });
	TestTrue(TEXT("spike: frame of the rise, magnitude = speed off the floor"), Spike && Spike->Frame == 11 && FMath::IsNearlyEqual(Spike->Magnitude, 1600.f));
	TestEqual(TEXT("spike: no landing, no jump"), Count(Events, Land) + Count(Events, Jump_Ground), 0);

	// Without a count (another source) the same frames show nothing: the velocity reading needs the way in
	FGSTN_PresentationSnapshot NoCountA = Still;
	NoCountA.GroundBounceCount = NoCountA.WallBounceCount = INDEX_NONE;
	FGSTN_PresentationSnapshot NoCountB = Reacting(11, FVector(900, 0, 1600), INDEX_NONE, INDEX_NONE);
	TestEqual(TEXT("no count: no bounce"), Count(Run({NoCountA, NoCountB}), GroundBounce), 0);

	// A bounce both readings see is one event (count and reflection on the same frame)
	Events = Run({Reacting(20, FVector(0, 0, -900), 0, 0), Reacting(21, FVector(0, 0, 540), 1, 0)});
	TestEqual(TEXT("both: one ground bounce"), Count(Events, GroundBounce), 1);

	// A wall: the count rises, the normal is where the speed along the floor turned
	Events = Run({Reacting(30, FVector(800, 300, 100), 0, 0), Reacting(31, FVector(-480, 300, 90), 0, 1)});
	TestEqual(TEXT("wall: one wall bounce"), Count(Events, WallBounce), 1);
	const FGSTN_PresentationEvent* Wall = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::WallBounce; });
	TestTrue(TEXT("wall: normal -X, magnitude = speed into it"), Wall && Wall->Direction.Equals(FVector(-1, 0, 0), 0.01) && FMath::IsNearlyEqual(Wall->Magnitude, 800.f));

	// A ceiling shares the wall count: the rise turned into a fall, the normal points down
	Events = Run({Reacting(40, FVector(200, 0, 900), 0, 0), Reacting(41, FVector(200, 0, -540), 0, 1)});
	const FGSTN_PresentationEvent* Ceiling = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::WallBounce_Ceiling; });
	TestTrue(TEXT("ceiling: one ceiling bounce, normal -Up"), Count(Events, WallBounce_Ceiling) == 1 && Ceiling && Ceiling->Direction.Equals(FVector(0, 0, -1), 0.01));
	TestEqual(TEXT("ceiling: not reported as a wall (polish M2 r3)"), Count(Events, WallBounce), 0);

	// The simulation names the face: a ceiling it stamped is a ceiling even when the velocity turned along the floor too
	FGSTN_PresentationSnapshot Stamped = Reacting(43, FVector(-300, 0, -200), 0, 1);
	Stamped.SurfaceImpactFrame = 43;
	Stamped.bSurfaceImpactCeiling = true;
	Events = Run({Reacting(42, FVector(900, 0, 100), 0, 0), Stamped});
	TestEqual(TEXT("stamped ceiling: ceiling tag"), Count(Events, WallBounce_Ceiling), 1);
	TestEqual(TEXT("stamped ceiling: no wall tag"), Count(Events, WallBounce), 0);
	Stamped.bSurfaceImpactCeiling = false;
	Events = Run({Reacting(42, FVector(900, 0, 100), 0, 0), Stamped});
	TestEqual(TEXT("stamped wall: wall tag"), Count(Events, WallBounce), 1);

	// A second bounce of the chain is a second event; a new hit resets the count without one, and its first bounce counts again
	FGSTN_PresentationSnapshot ReHit = Reacting(52, FVector(600, 0, 600), 0, 0);
	ReHit.LastHitFrame = 52;
	ReHit.LastHitStrength = 60.f;
	FGSTN_PresentationSnapshot AfterHit = Reacting(60, FVector(600, 0, 700), 1, 0);
	AfterHit.LastHitFrame = 52;
	AfterHit.LastHitStrength = 60.f;
	FGSTN_PresentationSnapshot Before = Reacting(61, FVector(600, 0, 690), 1, 0);
	Before.LastHitFrame = 52;
	Events = Run({Reacting(50, FVector(0, 0, 400), 1, 0), Reacting(51, FVector(0, 0, 300), 2, 0), ReHit, AfterHit, Before});
	TestEqual(TEXT("chain: second bounce, then the re-hit's first"), Count(Events, GroundBounce), 2);

	// A splat is not a bounce, whatever the count does
	FGSTN_PresentationSnapshot SplatSnap = Reacting(71, FVector::ZeroVector, 0, 1);
	SplatSnap.bSplat = true;
	TestEqual(TEXT("splat: no wall bounce"), Count(Run({Reacting(70, FVector(900, 0, 0), 0, 0), SplatSnap}), WallBounce), 0);

	// The bump's projected state tag is a Bump event (default tag rules), once per bump
	const FGameplayTag BumpWall = FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.State.Bump.Wall")), false);
	if (BumpWall.IsValid())
	{
		FGSTN_PresentationDeriveRules Rules;
		Rules.TagRules = FGSTN_PresentationDeriveRules::MakeDefaultTagRules();
		FGSTN_PresentationSnapshot B1 = Reacting(80, FVector(300, 0, 0), 0, 0);
		FGSTN_PresentationSnapshot B2 = Reacting(81, FVector(-60, 0, 0), 0, 0);
		B2.StateTags.AddTag(BumpWall);
		FGSTN_PresentationSnapshot B3 = B2;
		B3.Frame = 82;
		TestEqual(TEXT("bump: one event"), Count(Run({B1, B2, B3}, Rules), Bump_Wall), 1);

		// The bump's push off the face turns the velocity, but the simulation stamped the contact: no WallBounce with the bump (polish M2 r3)
		B2.SurfaceImpactFrame = 81;
		B3.SurfaceImpactFrame = 81;
		const TArray<FGSTN_PresentationEvent> BumpEvents = Run({B1, B2, B3}, Rules);
		TestEqual(TEXT("bump: still one bump"), Count(BumpEvents, Bump_Wall), 1);
		TestEqual(TEXT("bump: no wall bounce on the same frame"), Count(BumpEvents, WallBounce) + Count(BumpEvents, WallBounce_Ceiling), 0);
	}

	// Grinding along a wall turns the velocity twice a few frames apart (no count, polish M2 r3): one wall bounce, then one more only after
	// BounceRefireFrames
	{
		const TArray<FGSTN_PresentationEvent> Grind = Run({Reacting(90, FVector(800, 0, 0), INDEX_NONE, INDEX_NONE),
			Reacting(91, FVector(-500, 0, 0), INDEX_NONE, INDEX_NONE), Reacting(94, FVector(600, 0, 0), INDEX_NONE, INDEX_NONE),
			Reacting(95, FVector(-400, 0, 0), INDEX_NONE, INDEX_NONE), Reacting(110, FVector(600, 0, 0), INDEX_NONE, INDEX_NONE),
			Reacting(111, FVector(-400, 0, 0), INDEX_NONE, INDEX_NONE)});
		TestEqual(TEXT("grind: the second turn 4 frames later is not a bounce, the one 20 frames later is"), Count(Grind, WallBounce), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationDeriveRunTest, "GSTNPresentation.Core.Derive.Run", GSTNPresentationTest::Flags)
bool FGSTN_PresentationDeriveRunTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTest;
	using namespace GSTNPresentationTags;

	auto Run1 = [](int32 Frame, int32 Stage, float Vx, EGSTN_PresentationSkidPhase SkidPhase = EGSTN_PresentationSkidPhase::None)
	{
		FGSTN_PresentationSnapshot S = Snap(Frame, true, FVector(Vx, 0, 0));
		S.RunStage = Stage;
		S.SkidPhase = SkidPhase;
		return S;
	};
	// Start, two stage-ups, skid (enter, loop, exit), run the other way
	TArray<FGSTN_PresentationEvent> Events = Run({
		Run1(1, 0, 0), Run1(2, 1, 300), Run1(3, 2, 600), Run1(4, 3, 900),
		Run1(5, 0, 800, EGSTN_PresentationSkidPhase::Enter), Run1(6, 0, 500, EGSTN_PresentationSkidPhase::Loop),
		Run1(7, 0, 0, EGSTN_PresentationSkidPhase::Exit), Run1(8, 0, 0), Run1(9, 2, -400)});
	TestEqual(TEXT("run start"), Count(Events, RunStart), 2);  // from rest, then again after the turn (stage 0 -> 2: resumed stage)
	TestEqual(TEXT("two stage-ups"), Count(Events, StageUp), 2);
	TestEqual(TEXT("skid enter"), Count(Events, Skid_Enter), 1);
	TestEqual(TEXT("skid loop"), Count(Events, Skid_Loop), 1);
	TestEqual(TEXT("skid exit"), Count(Events, Skid_Exit), 1);
	TestEqual(TEXT("a skid is not a release stop"), Count(Events, StopRelease), 0);
	TestEqual(TEXT("turn"), Count(Events, Turn180), 1);
	TestEqual(TEXT("stage 3 -> skid is one stage-down (ends sprint lines)"), Count(Events, StageDown), 1);

	// Stick released: soft stop, no skid, no turn
	Events = Run({Run1(1, 0, 0), Run1(2, 1, 400), Run1(3, 0, 200), Run1(4, 0, 0)});
	TestEqual(TEXT("release stop"), Count(Events, StopRelease), 1);
	TestEqual(TEXT("release: no skid"), Count(Events, Skid_Enter), 0);
	TestEqual(TEXT("release: stage-down too"), Count(Events, StageDown), 1);

	// After the turn window a run the other way is not a turn
	FGSTN_PresentationDeriveRules Rules;
	Rules.TurnWindowFrames = 2;
	Events = Run({Run1(1, 2, 800), Run1(2, 0, 600, EGSTN_PresentationSkidPhase::Enter), Run1(3, 0, 0), Run1(4, 0, 0), Run1(5, 0, 0),
		Run1(6, 0, 0), Run1(7, 1, -300)}, Rules);
	TestEqual(TEXT("late: no turn"), Count(Events, Turn180), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationDeriveTagsTest, "GSTNPresentation.Core.Derive.Tags", GSTNPresentationTest::Flags)
bool FGSTN_PresentationDeriveTagsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTest;
	using namespace GSTNPresentationTags;

	// The rules watch presentation tags here (always registered): a state tag, and a move tag
	FGSTN_PresentationDeriveRules Rules;
	FGSTN_PresentationTagRule& State = Rules.TagRules.AddDefaulted_GetRef();
	State.WatchedTag = Shield;
	State.OnAdded = Shield_On;
	State.OnRemoved = Shield_Break;
	FGSTN_PresentationTagRule& Move = Rules.TagRules.AddDefaulted_GetRef();
	Move.WatchedTag = Dodge;
	Move.bMoveTag = true;
	Move.OnAdded = Dodge;

	FGSTN_PresentationSnapshot S1 = Snap(1, true);
	FGSTN_PresentationSnapshot S2 = Snap(2, true);
	S2.StateTags.AddTag(Shield_Hit);  // a child of the watched tag counts
	FGSTN_PresentationSnapshot S3 = Snap(3, true);
	S3.MoveTags.AddTag(Dodge);
	S3.MoveStartFrame = 3;
	FGSTN_PresentationSnapshot S4 = Snap(4, true);
	S4.MoveTags.AddTag(Dodge);
	S4.MoveStartFrame = 3;
	FGSTN_PresentationSnapshot S5 = Snap(5, true);
	S5.MoveTags.AddTag(Dodge);
	S5.MoveStartFrame = 5;  // the dodge again, back to back
	TArray<FGSTN_PresentationEvent> Events = Run({S1, S2, S3, S4, S5}, Rules);
	TestEqual(TEXT("shield on"), Count(Events, Shield_On), 1);
	TestEqual(TEXT("shield gone"), Count(Events, Shield_Break), 1);
	TestEqual(TEXT("two dodges (restart counts)"), Count(Events, Dodge), 2);
	const FGSTN_PresentationEvent* FirstDodge = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::Dodge; });
	TestTrue(TEXT("move event stamped with the move's start frame"), FirstDodge && FirstDodge->Frame == 3 && FirstDodge->bExactFrame);

	// A held rule waits out its hold tag: the state starts frozen (hitstop), the event comes at the first free frame
	FGSTN_PresentationDeriveRules Held;
	FGSTN_PresentationTagRule& Trail = Held.TagRules.AddDefaulted_GetRef();
	Trail.WatchedTag = Tumble;
	Trail.OnAdded = Tumble_Start;
	Trail.OnRemoved = Tumble_End;
	Trail.HeldWhileTag = Shield;
	FGSTN_PresentationSnapshot H2 = Snap(2, false);
	H2.StateTags.AddTag(Tumble);
	H2.StateTags.AddTag(Shield);
	FGSTN_PresentationSnapshot H3 = H2;
	H3.Frame = 3;
	FGSTN_PresentationSnapshot H4 = Snap(4, false);
	H4.StateTags.AddTag(Tumble);
	Events = Run({Snap(1, true), H2, H3, H4}, Held);
	const FGSTN_PresentationEvent* Start = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::Tumble_Start; });
	TestTrue(TEXT("held: starts once, after the hold"), Count(Events, Tumble_Start) == 1 && Start && Start->Frame == 4);
	TestEqual(TEXT("held: no end while frozen"), Count(Events, Tumble_End), 0);

	TestTrue(TEXT("default rules exist"), FGSTN_PresentationDeriveRules::MakeDefaultTagRules().Num() >= 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationResimTest, "GSTNPresentation.Core.Resimulation", GSTNPresentationTest::Flags)
bool FGSTN_PresentationResimTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTest;
	using namespace GSTNPresentationTags;

	UGSTN_PresentationComponent* Component = NewObject<UGSTN_PresentationComponent>(GetTransientPackage());
	Component->bPlayEffects = false;
	Component->DefaultTable = MakeTable({MakeCue(Land, 300.f)});

	// Presented frames: fall, land at 6; a correction rewinds the presented state to "airborne at 5" and lands again at 6, then at 7
	FGSTN_PresentationSnapshot Fall = Snap(5, false, FVector(0, 0, -800));
	FGSTN_PresentationSnapshot Landed = Snap(6, true);
	FGSTN_PresentationSnapshot LandedLate = Snap(7, true);
	TArray<FGSTN_PresentationEvent> Derived;
	FGSTN_PresentationDeriver Deriver;
	for (const FGSTN_PresentationSnapshot& S : {Snap(4, false, FVector(0, 0, -600)), Fall, Landed, Fall, Landed, Fall, LandedLate})
	{
		Deriver.Step(S, Component->DeriveRules, Derived);
	}
	TestEqual(TEXT("the edge shows three times"), Count(Derived, Land), 3);
	int32 PlayedCount = 0;
	for (const FGSTN_PresentationEvent& E : Derived)
	{
		PlayedCount += Component->ReportEvent(E) ? 1 : 0;
	}
	TestEqual(TEXT("it plays once"), PlayedCount, 1);

	// Soft landing below the row's threshold: does not play
	FGSTN_PresentationEvent Soft;
	Soft.EventTag = Land;
	Soft.Frame = 50;
	Soft.Magnitude = 100.f;
	TestFalse(TEXT("below the threshold"), Component->ReportEvent(Soft));
	return true;
}

#endif

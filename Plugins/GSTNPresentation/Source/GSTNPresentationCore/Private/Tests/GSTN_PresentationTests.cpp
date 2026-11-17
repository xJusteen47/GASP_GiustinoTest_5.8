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
	TArray<FGSTN_PresentationEvent> Events = Run(Frames);
	TestEqual(TEXT("one ground jump"), Count(Events, Jump_Ground), 1);
	TestEqual(TEXT("one air jump"), Count(Events, Jump_Air), 1);
	TestEqual(TEXT("one landing"), Count(Events, Land), 1);
	const FGSTN_PresentationEvent* Landing = Events.FindByPredicate([](const FGSTN_PresentationEvent& E) { return E.EventTag == GSTNPresentationTags::Land; });
	TestTrue(TEXT("landing magnitude = fall speed"), Landing && FMath::IsNearlyEqual(Landing->Magnitude, 700.f) && Landing->Frame == 6);

	// Walking off a ledge and a step down: nothing
	Events = Run({Snap(1, true), Snap(2, false, FVector(300, 0, -10)), Snap(3, true, FVector(300, 0, 0))});
	TestEqual(TEXT("ledge: no jump"), Count(Events, Jump_Ground), 0);
	TestEqual(TEXT("step: no landing"), Count(Events, Land), 0);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PresentationDeriveRunTest, "GSTNPresentation.Core.Derive.Run", GSTNPresentationTest::Flags)
bool FGSTN_PresentationDeriveRunTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPresentationTest;
	using namespace GSTNPresentationTags;

	auto Run1 = [](int32 Frame, int32 Stage, float Vx, EGSTN_PresentationSkidPhase Skid = EGSTN_PresentationSkidPhase::None)
	{
		FGSTN_PresentationSnapshot S = Snap(Frame, true, FVector(Vx, 0, 0));
		S.RunStage = Stage;
		S.SkidPhase = Skid;
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

	// Stick released: soft stop, no skid, no turn
	Events = Run({Run1(1, 0, 0), Run1(2, 1, 400), Run1(3, 0, 200), Run1(4, 0, 0)});
	TestEqual(TEXT("release stop"), Count(Events, StopRelease), 1);
	TestEqual(TEXT("release: no skid"), Count(Events, Skid_Enter), 0);

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

	TestTrue(TEXT("default rules exist"), FGSTN_PresentationDeriveRules::MakeDefaultTagRules().Num() >= 4);
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

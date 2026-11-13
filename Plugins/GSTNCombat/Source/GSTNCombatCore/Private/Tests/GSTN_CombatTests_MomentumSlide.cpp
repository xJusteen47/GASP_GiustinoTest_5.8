#include "Tests/GSTN_CombatTestUtils.h"
#include "Movement/GSTN_MomentumSlide.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MomentumSlidePickTest, "GSTNCombat.Core.MomentumSlide.Pick", GSTN_CORE_TEST_FLAGS)
bool FGSTN_MomentumSlidePickTest::RunTest(const FString& Parameters)
{
	FGSTN_MomentumSlide Slide;
	const FVector Run(600.0, 0.0, 0.0);
	const FVector Ahead(1.0, 0.0, 0.0);
	const FVector Behind(-1.0, 0.0, 0.0);
	const FVector Side(0.0, 1.0, 0.0);
	bool bAgainst = true;

	TestNull(TEXT("off by default: nothing slides"), FGSTN_MomentumSlideMath::PickResponse(Slide, Run, Ahead, bAgainst));
	TestFalse(TEXT("a timeline's default data has the slide off"), FGSTN_CombatTimelineData().MomentumSlide.bEnabled);

	Slide.bEnabled = true;
	TestTrue(TEXT("along: the Along response"), FGSTN_MomentumSlideMath::PickResponse(Slide, Run, Ahead, bAgainst) == &Slide.Along && !bAgainst);
	TestTrue(TEXT("sideways is above the default threshold"), FGSTN_MomentumSlideMath::PickResponse(Slide, Run, Side, bAgainst) == &Slide.Along && !bAgainst);
	TestTrue(TEXT("behind: the Against response"), FGSTN_MomentumSlideMath::PickResponse(Slide, Run, Behind, bAgainst) == &Slide.Against && bAgainst);
	TestTrue(TEXT("against brakes harder than along by default"), Slide.Against.Deceleration > Slide.Along.Deceleration);

	TestNull(TEXT("below MinSpeed nothing slides"), FGSTN_MomentumSlideMath::PickResponse(Slide, FVector(Slide.MinSpeed - 1.0, 0.0, 0.0), Ahead, bAgainst));

	Slide.Against.Mode = EGSTN_MomentumSlideMode::Stop;
	TestNull(TEXT("a Stop response drops the speed"), FGSTN_MomentumSlideMath::PickResponse(Slide, Run, Behind, bAgainst));
	TestTrue(TEXT("and still says it was the Against one"), bAgainst);

	Slide.OppositeDotThreshold = 0.5f;
	TestTrue(TEXT("a higher threshold makes sideways count as against"), FGSTN_MomentumSlideMath::PickResponse(Slide, Run, Side, bAgainst) == nullptr && bAgainst);
	Slide.OppositeDotThreshold = -1.f;
	Slide.Against.Mode = EGSTN_MomentumSlideMode::Slide;
	TestTrue(TEXT("-1: never against"), FGSTN_MomentumSlideMath::PickResponse(Slide, Run, Behind, bAgainst) == &Slide.Along && !bAgainst);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MomentumSlideSpeedTest, "GSTNCombat.Core.MomentumSlide.Speed", GSTN_CORE_TEST_FLAGS)
bool FGSTN_MomentumSlideSpeedTest::RunTest(const FString& Parameters)
{
	const float TickRate = 62.5f;
	const float Dt = 1.f / TickRate;

	FGSTN_MomentumSlideResponse Response;
	Response.SpeedScale = 0.5f;
	TestEqual(TEXT("scale"), FGSTN_MomentumSlideMath::StartSpeed(Response, 600.f), 300.f);
	Response.MaxStartSpeed = 200.f;
	TestEqual(TEXT("cap"), FGSTN_MomentumSlideMath::StartSpeed(Response, 600.f), 200.f);

	// Fixed: a straight line, v^2 / 2a in distance (within one frame of travel)
	FGSTN_MomentumSlideResponse Fixed;
	Fixed.Deceleration = 1500.f;
	const FGSTN_MomentumSlideBraking FixedBraking = FGSTN_MomentumSlideMath::ResolveBraking(Fixed, 4000.f, 16.f);
	TestEqual(TEXT("fixed ignores the character"), FixedBraking.Deceleration, 1500.f);
	TestEqual(TEXT("fixed has no speed friction"), FixedBraking.Friction, 0.f);
	TestEqual(TEXT("one step"), FGSTN_MomentumSlideMath::StepSpeed(600.f, FixedBraking, Dt), 600.f - 1500.f * Dt, 1e-3f);
	TestEqual(TEXT("never below zero"), FGSTN_MomentumSlideMath::StepSpeed(10.f, FixedBraking, Dt), 0.f);
	const float Distance = FGSTN_MomentumSlideMath::PredictDistance(Fixed, 600.f, FixedBraking, TickRate);
	TestTrue(*FString::Printf(TEXT("fixed distance about 120 cm (got %.1f)"), Distance), FMath::Abs(Distance - 120.f) < 600.f * Dt);

	// MaxFrames cuts it
	Fixed.MaxFrames = 5;
	TestEqual(TEXT("max frames"), FGSTN_MomentumSlideMath::PredictDistance(Fixed, 600.f, FixedBraking, TickRate),
		Dt * (600.f + (600.f - 1500.f * Dt) + (600.f - 2.f * 1500.f * Dt) + (600.f - 3.f * 1500.f * Dt) + (600.f - 4.f * 1500.f * Dt)), 1e-2f);

	// Character: the character's terms, scaled
	FGSTN_MomentumSlideResponse Character;
	Character.Friction = EGSTN_MomentumSlideFriction::Character;
	Character.FrictionScale = 0.5f;
	const FGSTN_MomentumSlideBraking CharacterBraking = FGSTN_MomentumSlideMath::ResolveBraking(Character, 4000.f, 16.f);
	TestEqual(TEXT("character deceleration scaled"), CharacterBraking.Deceleration, 2000.f);
	TestEqual(TEXT("character friction scaled"), CharacterBraking.Friction, 8.f);
	TestEqual(TEXT("speed friction grows with speed"), FGSTN_MomentumSlideMath::StepSpeed(600.f, CharacterBraking, Dt), 600.f - (2000.f + 8.f * 600.f) * Dt, 1e-2f);

	// Stop slides nowhere
	FGSTN_MomentumSlideResponse Stop;
	Stop.Mode = EGSTN_MomentumSlideMode::Stop;
	TestEqual(TEXT("stop"), FGSTN_MomentumSlideMath::PredictDistance(Stop, 600.f, FixedBraking, TickRate), 0.f);
	return true;
}

#endif

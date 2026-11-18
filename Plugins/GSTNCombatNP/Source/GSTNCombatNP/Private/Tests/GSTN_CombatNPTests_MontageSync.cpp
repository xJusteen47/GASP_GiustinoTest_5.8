// Synced montage timing (combat-sync-1): the montage of a move shows the timeline's frame. Two halves, both in the GAS-NPP fork:
// the simulation holds a montage on the tick that started it (UNetMontageSimulator::SimulationTick), and the presentation lands the anim
// instance on the simulation time after the mesh's own advance (ESyncedMontagePresentation::LandOnSimTime). Before, the pose drawn on the
// first frame of a move was about two frames ahead of the timeline and a hitstop pulled it back.

#include "Abilities/NpAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "Misc/AutomationTest.h"
#include "MontageSimulator/NetMontageSimulator.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNMontageSyncTests
{
	// A montage of the sample content (not in git): the simulator test is skipped where it is missing
	const TCHAR* SampleMontage = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Bench/M_interaction_bench_into_BR_LFoot_Montage.M_interaction_bench_into_BR_LFoot_Montage");
	constexpr float StepMs = 16.f;
	constexpr float Step = StepMs / 1000.f;
	// The rate the Overlord jab timelines use: one 60 fps animation frame per 62.5 Hz simulation tick
	constexpr float CardRate = 62.5f / 60.f;

	/**
	 * The engine's montage instance as the captures show it (FAnimMontageInstance::Advance and its FMontageSubStepper): every update adds its
	 * delta to the stepper's time, and the time is spent only when the play rate is not 0, all of it in that update. So an update at rate 0
	 * keeps its delta and the next moving update moves by rate * (kept + delta). SetPosition does not empty it
	 */
	struct FEngineInstanceModel
	{
		float Position = 0.f;
		float StepperSeconds = 0.f;

		void Update(float PlayRate, float AnimDelta)
		{
			StepperSeconds += AnimDelta;
			if (PlayRate > 0.f)
			{
				Position += PlayRate * StepperSeconds;
				StepperSeconds = 0.f;
			}
		}
	};

	/**
	 * One render frame of a mesh: what FinalizeFrame does (with the simulator's carried stepper estimate, or none when bTrackCarry is false:
	 * the code before the half-rate fix), then the animation update advancing the instance. Returns the position drawn
	 */
	float PresentFrame(FEngineInstanceModel& Instance, float& Carried, float Target, float NominalRate, float AnimDelta, bool bTrackCarry = true)
	{
		const FSyncedMontagePresentationStep PresentStep = UNetMontageSimulator::ComputeLandOnSimTimeStep(Instance.Position, Target, NominalRate,
			AnimDelta, 4.f, bTrackCarry ? Carried : 0.f);
		if (PresentStep.bSetPosition)
		{
			Instance.Position = PresentStep.Position;
		}
		Instance.Update(PresentStep.PlayRate, AnimDelta);
		Carried = PresentStep.CarriedSecondsAfter;
		return Instance.Position;
	}

	/** The same for a fresh instance at InstancePosition with an empty stepper (the cases that look at one frame) */
	float PresentFrame(float& InstancePosition, float Target, float NominalRate, float AnimDelta)
	{
		FEngineInstanceModel Instance;
		Instance.Position = InstancePosition;
		float Carried = 0.f;
		InstancePosition = PresentFrame(Instance, Carried, Target, NominalRate, AnimDelta);
		return InstancePosition;
	}

	FAbilitySystemTimeStep MakeStep(int32 Frame)
	{
		FAbilitySystemTimeStep TimeStep;
		TimeStep.ServerFrame = Frame;
		TimeStep.BaseSimTimeMs = Frame * StepMs;
		TimeStep.StepMs = StepMs;
		return TimeStep;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MontageSyncLandOnSimTimeTest, "GSTNCombat.NP.MontageSync.LandOnSimTime", GSTN_NP_TEST_FLAGS)
bool FGSTN_MontageSyncLandOnSimTimeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMontageSyncTests;
	constexpr float Tolerance = 1.e-4f;
	const float Dt = 1.f / 60.f;

	// Started on the simulation time this frame: drawn there, not one render frame ahead (the old first image was a2.04 for frame 0)
	{
		float Instance = 0.f;
		TestEqual(TEXT("a montage started at 0 is drawn at 0"), PresentFrame(Instance, 0.f, CardRate, Dt), 0.f, Tolerance);
	}

	// A move at 60 fps render and 62.5 Hz simulation, with the smoothed simulation time as the target: every frame is drawn on the target
	{
		FEngineInstanceModel Instance;
		float Carried = 0.f;
		bool bAllOn = true;
		for (int32 RenderFrame = 1; RenderFrame <= 40; ++RenderFrame)
		{
			const float Target = RenderFrame * Dt * CardRate;
			bAllOn &= FMath::IsNearlyEqual(PresentFrame(Instance, Carried, Target, CardRate, Dt), Target, Tolerance);
		}
		TestTrue(TEXT("a running montage is drawn on the simulation time every frame"), bAllOn);
	}

	// The fixed tick without smoothing: the target moves by whole steps, sometimes two in a frame, sometimes none. The repeated ticks are holds
	// (rate 0), whose delta the engine keeps for the next frame
	{
		FEngineInstanceModel Instance;
		float Carried = 0.f;
		const int32 Ticks[] = {0, 1, 2, 3, 3, 4, 6, 7, 8, 8, 9};
		bool bAllOn = true;
		for (const int32 Tick : Ticks)
		{
			const float Target = Tick * Step * CardRate;
			bAllOn &= FMath::IsNearlyEqual(PresentFrame(Instance, Carried, Target, CardRate, Dt), Target, Tolerance);
		}
		TestTrue(TEXT("whole-step targets (0, 1 or 2 ticks a frame) are drawn exactly"), bAllOn);
	}

	// Hitstop: the simulation holds the hit frame. With the mesh running (no hit pause on it) the instance holds too
	{
		float Instance = 9.f * Step * CardRate;
		const float Hold = Instance;
		const FSyncedMontagePresentationStep Held = UNetMontageSimulator::ComputeLandOnSimTimeStep(Instance, Hold, 0.f, Dt);
		TestFalse(TEXT("held: no teleport"), Held.bSetPosition);
		TestEqual(TEXT("held: rate 0"), Held.PlayRate, 0.f);
		TestEqual(TEXT("held: drawn on the hit frame"), PresentFrame(Instance, Hold, 0.f, Dt), Hold, Tolerance);
	}

	// A hit pause on the mesh (GlobalAnimRateScale 0, so no advance): the instance is put on the target with rate 0, also when the pause is
	// lifted later in the same frame (no carry past the target)
	{
		const FSyncedMontagePresentationStep Paused = UNetMontageSimulator::ComputeLandOnSimTimeStep(0.2f, 0.15f, CardRate, 0.f);
		TestTrue(TEXT("mesh paused: teleport to the target"), Paused.bSetPosition);
		TestEqual(TEXT("mesh paused: position"), Paused.Position, 0.15f, Tolerance);
		TestEqual(TEXT("mesh paused: rate 0"), Paused.PlayRate, 0.f);
	}

	// Ahead of the simulation (a correction moved it back): back and hold, never run backwards
	{
		const FSyncedMontagePresentationStep Ahead = UNetMontageSimulator::ComputeLandOnSimTimeStep(0.30f, 0.25f, CardRate, Dt);
		TestTrue(TEXT("ahead: teleport back"), Ahead.bSetPosition);
		TestEqual(TEXT("ahead: on the target"), Ahead.Position, 0.25f, Tolerance);
		TestEqual(TEXT("ahead: rate 0"), Ahead.PlayRate, 0.f);
	}

	// Far behind (a long hitch): teleport to where the nominal rate lands on the target, so the notifies of the last frame still fire
	{
		float Instance = 0.f;
		const float Target = 0.5f;
		const FSyncedMontagePresentationStep Behind = UNetMontageSimulator::ComputeLandOnSimTimeStep(Instance, Target, CardRate, Dt);
		TestTrue(TEXT("far behind: teleport"), Behind.bSetPosition);
		TestEqual(TEXT("far behind: lands on the target"), PresentFrame(Instance, Target, CardRate, Dt), Target, Tolerance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MontageSyncStartTickTest, "GSTNCombat.NP.MontageSync.StartTickHold", GSTN_NP_TEST_FLAGS)
bool FGSTN_MontageSyncStartTickTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMontageSyncTests;
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, SampleMontage);
	if (!Montage)
	{
		AddInfo(TEXT("sample montage missing (sample content is not in git): skipped"));
		return true;
	}
	// The sync state keeps the play rate to 3 decimals (1.041 for 62.5/60)
	constexpr float Tolerance = 1.e-3f;

	// Frame N of the move is the tick that started it plus N; after that tick the montage must be at N steps (the pose of frame N)
	auto RunMove = [&](bool bLegacyStartTick, int32 HitFrame, int32 StillFrames, TArray<float>& OutTimes)
	{
		UNpAbilitySystemComponent* Asc = NewObject<UNpAbilitySystemComponent>(GetTransientPackage());
		Asc->AbilityActorInfo = MakeShared<FGameplayAbilityActorInfo>();
		Asc->bAdvanceSyncedMontageOnStartTick = bLegacyStartTick;
		UNetMontageSimulator* Simulator = NewObject<UNetMontageSimulator>(Asc);
		Simulator->SetAbilitySystemComponent(Asc);

		FMontageSimSyncState In, Out;
		for (int32 Frame = 0; Frame < HitFrame + StillFrames + 4; ++Frame)
		{
			// What the ability tick does before the montage ticks: start on frame 0, pause on the frames the move clock stands still
			if (Frame == 0)
			{
				Simulator->PlayMontage(FAbilityMontagePlayback(Montage, 0.f, CardRate, NAME_None, 1.f));
			}
			const bool bStill = Frame > HitFrame && Frame <= HitFrame + StillFrames;
			if (bStill)
			{
				Simulator->PauseCurrentMontage();
			}
			else if (Frame == HitFrame + StillFrames + 1)
			{
				Simulator->ResumeCurrentMontage();
			}
			Simulator->SimulationTick(MakeStep(Frame), In, Out);
			In = Out;
			float Time = -1.f;
			Simulator->GetPlayingMontage(Time);
			OutTimes.Add(Time);
		}
	};

	const int32 Hit = 6;
	const int32 Still = 3;
	TArray<float> Times;
	RunMove(false, Hit, Still, Times);
	TestEqual(TEXT("frame 0 (the start tick): the montage holds its start time"), Times[0], 0.f, Tolerance);
	TestEqual(TEXT("frame 1: one step"), Times[1], 1.f * Step * CardRate, Tolerance);
	TestEqual(TEXT("hit frame: the pose of the hit frame"), Times[Hit], Hit * Step * CardRate, Tolerance);
	TestEqual(TEXT("last still frame: still the hit frame"), Times[Hit + Still], Hit * Step * CardRate, Tolerance);
	// The timeline frame after the hitstop is the frame index minus the still frames
	const int32 After = Hit + Still + 2;
	TestEqual(TEXT("after the hitstop: the timeline's frame"), Times[After], (After - Still) * Step * CardRate, Tolerance);

	// The fork's original behaviour (setting on): one step ahead from the start tick
	TArray<float> LegacyTimes;
	RunMove(true, Hit, Still, LegacyTimes);
	TestEqual(TEXT("legacy: frame 0 is already one step in"), LegacyTimes[0], 1.f * Step * CardRate, Tolerance);
	return true;
}

// montage-exit (2026-11-15): a stopped synced montage leaves with the montage's own blend-out, never with its blend-in (the instance's current
// blend). The Overlord jab has no blend-in and a 4 frame blend-out: it was cut in one frame and the graph under it picked a crouch clip
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MontageSyncBlendOutTest, "GSTNCombat.NP.MontageSync.BlendOutIsMontages", GSTN_NP_TEST_FLAGS)
bool FGSTN_MontageSyncBlendOutTest::RunTest(const FString& Parameters)
{
	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
	Montage->BlendIn.SetBlendTime(0.f);
	Montage->BlendOut.SetBlendTime(8.f / 60.f);
	Montage->BlendOut.SetBlendOption(EAlphaBlendOption::Cubic);
	Montage->BlendModeOut = EMontageBlendMode::Standard;

	const FMontageBlendSettings Own = UNetMontageSimulator::MakeMontageBlendOut(*Montage);
	TestEqual(TEXT("the montage's blend-out time, not its blend-in"), Own.Blend.BlendTime, 8.f / 60.f, 1.e-6f);
	TestTrue(TEXT("the montage's blend-out curve"), Own.Blend.BlendOption == EAlphaBlendOption::Cubic);
	TestTrue(TEXT("the montage's blend-out mode"), Own.BlendMode == EMontageBlendMode::Standard);

	const FMontageBlendSettings Asked = UNetMontageSimulator::MakeMontageBlendOut(*Montage, 4.f / 60.f);
	TestEqual(TEXT("an explicit time replaces the time"), Asked.Blend.BlendTime, 4.f / 60.f, 1.e-6f);
	TestTrue(TEXT("and keeps the montage's curve"), Asked.Blend.BlendOption == EAlphaBlendOption::Cubic);

	const FMontageBlendSettings Cut = UNetMontageSimulator::MakeMontageBlendOut(*Montage, 0.f);
	TestEqual(TEXT("0 is a cut"), Cut.Blend.BlendTime, 0.f, 1.e-6f);
	return true;
}

// montage start jitter (2026-11-15): captures at 60 fps over the 62.5 Hz simulation showed the first frames of a move at different montage times
// from run to run (0.00 then 0.18 or 1.25 animation frames) and dead frames later. The presentation clock shows the start on the first rendered
// frame whatever the phase of the press, then advances every frame, never ahead of the latest tick and never more than two ticks behind it
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MontageSyncPresentationClockTest, "GSTNCombat.NP.MontageSync.PresentationClock", GSTN_NP_TEST_FLAGS)
bool FGSTN_MontageSyncPresentationClockTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMontageSyncTests;
	constexpr float Tolerance = 1.e-5f;
	const float RenderDt = 1.f / 60.f;
	const float TickMontage = Step * CardRate;     // one animation frame per tick
	const float MaxLag = 2.f * TickMontage;

	bool bFirstAlwaysStart = true;
	bool bNeverAhead = true;
	bool bNeverTooLate = true;
	bool bNoDeadFrame = true;
	bool bNoBigSkip = true;
	// Every phase of the start tick inside a render frame, one millisecond apart
	for (int32 PhaseMs = 0; PhaseMs < 17; ++PhaseMs)
	{
		const double StartTick = 1.0 + PhaseMs / 1000.0;
		FSyncedMontagePresentationClockInput Clock;
		Clock.Rate = CardRate;
		Clock.DeltaSeconds = RenderDt;
		Clock.MaxLagSeconds = MaxLag;
		bool bStarted = false;
		float Previous = 0.f;
		for (int32 Frame = 0; Frame < 120; ++Frame)
		{
			const double RenderTime = 1.0 + Frame * RenderDt;
			if (RenderTime < StartTick)
			{
				continue;
			}
			// Ticks since the start tick that ran by this render frame; the start tick holds the montage at 0
			const int32 Ticks = FMath::FloorToInt32(static_cast<float>((RenderTime - StartTick) / Step + 1.e-6));
			Clock.LatestTime = Ticks * TickMontage;
			Clock.AnchorTime = bStarted ? -1.f : 0.f;
			Clock.bHasPrevious = bStarted;
			Clock.PreviousTime = Previous;
			const float Shown = UNetMontageSimulator::ComputePresentedTime(Clock);
			if (!bStarted)
			{
				bFirstAlwaysStart &= FMath::IsNearlyEqual(Shown, 0.f, Tolerance);
			}
			else
			{
				const float Advance = Shown - Previous;
				bNoDeadFrame &= Advance > 0.9f * TickMontage;
				bNoBigSkip &= Advance < 2.05f * TickMontage;
			}
			bNeverAhead &= Shown <= Clock.LatestTime + Tolerance;
			bNeverTooLate &= Shown >= Clock.LatestTime - MaxLag - Tolerance;
			Previous = Shown;
			bStarted = true;
		}
	}
	TestTrue(TEXT("the first rendered frame of a start shows the start, at every phase"), bFirstAlwaysStart);
	TestTrue(TEXT("never ahead of the latest tick"), bNeverAhead);
	TestTrue(TEXT("never more than two ticks behind"), bNeverTooLate);
	TestTrue(TEXT("no dead frame (every frame advances about one animation frame)"), bNoDeadFrame);
	TestTrue(TEXT("no frame skips more than two animation frames"), bNoBigSkip);

	// Discontinuities follow the simulation at once
	FSyncedMontagePresentationClockInput Jump;
	Jump.Rate = CardRate;
	Jump.DeltaSeconds = RenderDt;
	Jump.MaxLagSeconds = MaxLag;
	Jump.bHasPrevious = true;
	Jump.PreviousTime = 0.40f;
	Jump.LatestTime = 0.27f;
	TestEqual(TEXT("a loop going back shows the latest tick"), UNetMontageSimulator::ComputePresentedTime(Jump), 0.27f, Tolerance);
	Jump.LatestTime = 1.00f;
	TestEqual(TEXT("a jump forward shows the latest tick"), UNetMontageSimulator::ComputePresentedTime(Jump), 1.00f, Tolerance);
	Jump.AnchorTime = 0.95f;
	TestEqual(TEXT("unless it is a restart: then its first frame (the land clip's frame 0)"), UNetMontageSimulator::ComputePresentedTime(Jump), 0.95f, Tolerance);
	Jump.AnchorTime = 1.20f;
	TestEqual(TEXT("an anchor past the latest tick (undone by a rollback) is ignored"), UNetMontageSimulator::ComputePresentedTime(Jump), 1.00f, Tolerance);

	// A paused simulation (hitstop): the clock runs up to the frozen time and stays there
	FSyncedMontagePresentationClockInput Still;
	Still.Rate = CardRate;
	Still.DeltaSeconds = RenderDt;
	Still.MaxLagSeconds = MaxLag;
	Still.bHasPrevious = true;
	Still.LatestTime = 0.30f;
	Still.PreviousTime = 0.30f - TickMontage;
	const float FirstStill = UNetMontageSimulator::ComputePresentedTime(Still);
	TestEqual(TEXT("hitstop: reaches the hit frame's pose"), FirstStill, 0.30f, Tolerance);
	Still.PreviousTime = FirstStill;
	TestEqual(TEXT("and holds it"), UNetMontageSimulator::ComputePresentedTime(Still), 0.30f, Tolerance);
	return true;
}


// The half-rate montage (rule-10 review cp5, 2026-11-15): the captures read the drawn montage position 0 2 2 4 4 6 for the first frames of every
// move and through the whole dropkick hold loop, while the simulation moved one tick a frame. A start is a hold (rate 0), the engine keeps that
// frame's delta in the montage sub-stepper, the next frame spends two deltas (two frames), lands on the following target and holds again.
// The presentation now counts the kept time (CarriedSeconds)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MontageSyncStepperCarryTest, "GSTNCombat.NP.MontageSync.StepperCarry", GSTN_NP_TEST_FLAGS)
bool FGSTN_MontageSyncStepperCarryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMontageSyncTests;
	constexpr float Tolerance = 1.e-4f;
	const float Dt = 1.f / 60.f;
	// One tick a frame from the start (the capture's 60 fps against 62.5 Hz, between its double ticks): targets 0, 1, 2 ... steps
	const auto Run = [&](bool bTrackCarry, TArray<float>& OutDrawn)
	{
		FEngineInstanceModel Instance;
		float Carried = 0.f;
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			OutDrawn.Add(PresentFrame(Instance, Carried, Frame * Step * CardRate, CardRate, Dt, bTrackCarry) / (Step * CardRate));
		}
	};

	TArray<float> Old;
	Run(false, Old);
	// The defect, reproduced by the model: what the captures read (in ticks)
	TestEqual(TEXT("old: frame 1 drawn two ticks in"), Old[1], 2.f, Tolerance);
	TestEqual(TEXT("old: frame 2 repeats it"), Old[2], 2.f, Tolerance);

	TArray<float> Fixed;
	Run(true, Fixed);
	bool bEveryFrameMoves = true;
	for (int32 Frame = 0; Frame < Fixed.Num(); ++Frame)
	{
		bEveryFrameMoves &= FMath::IsNearlyEqual(Fixed[Frame], static_cast<float>(Frame), Tolerance);
	}
	TestTrue(TEXT("fixed: every frame drawn on its tick (0 1 2 3 ...)"), bEveryFrameMoves);

	// A loop wrap (the dropkick hold) teleports back and holds: the kept time is counted, the next frame moves one tick, not three
	{
		FEngineInstanceModel Instance;
		Instance.Position = 25.f * Step * CardRate;
		Instance.StepperSeconds = Dt; // a hold the frame before
		float Carried = Dt;
		PresentFrame(Instance, Carried, 15.f * Step * CardRate, CardRate, Dt);
		TestEqual(TEXT("wrap: back on the loop start"), Instance.Position / (Step * CardRate), 15.f, Tolerance);
		TestEqual(TEXT("wrap: two deltas kept"), Carried, 2.f * Dt, Tolerance);
		PresentFrame(Instance, Carried, 16.f * Step * CardRate, CardRate, Dt);
		TestEqual(TEXT("wrap: next frame one tick on"), Instance.Position / (Step * CardRate), 16.f, Tolerance);
		TestEqual(TEXT("wrap: kept time spent"), Carried, 0.f, Tolerance);
	}

	// A mesh paused (no delta) adds nothing to the kept time
	{
		const FSyncedMontagePresentationStep Paused = UNetMontageSimulator::ComputeLandOnSimTimeStep(0.2f, 0.2f, CardRate, 0.f, 4.f, Dt);
		TestEqual(TEXT("paused mesh: kept time unchanged"), Paused.CarriedSecondsAfter, Dt, Tolerance);
	}
	return true;
}

// Hitstop micro-advance (Sakurai #7, 2026-11-18): the presentation lead rides on the shown time, never reaches the blend out, 0 changes nothing
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MontageSyncPresentationLeadTest, "GSTNCombat.NP.MontageSync.PresentationLead", GSTN_NP_TEST_FLAGS)
bool FGSTN_MontageSyncPresentationLeadTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No lead: the shown time"), UNetMontageSimulator::ApplyPresentationLead(0.5f, 0.f, 2.f), 0.5f);
	TestEqual(TEXT("Negative lead: the shown time"), UNetMontageSimulator::ApplyPresentationLead(0.5f, -0.1f, 2.f), 0.5f);
	TestEqual(TEXT("A lead adds to the shown time"), UNetMontageSimulator::ApplyPresentationLead(0.5f, 0.01f, 2.f), 0.51f, 1e-6f);
	TestTrue(TEXT("A lead stops short of the blend out"), UNetMontageSimulator::ApplyPresentationLead(1.995f, 0.01f, 2.f) < 2.f);
	TestEqual(TEXT("A lead never draws behind the shown time"), UNetMontageSimulator::ApplyPresentationLead(2.5f, 0.01f, 2.f), 2.5f);

	// The instance follows: an instance on the shown time with a lead ahead moves forward by the lead in one update
	const float Target = UNetMontageSimulator::ApplyPresentationLead(0.5f, 0.005f, 2.f);
	const FSyncedMontagePresentationStep Landing = UNetMontageSimulator::ComputeLandOnSimTimeStep(0.5f, Target, GSTNMontageSyncTests::CardRate,
		GSTNMontageSyncTests::Step, 4.f, 0.f);
	TestFalse(TEXT("A lead within reach is played, not teleported"), Landing.bSetPosition);
	TestEqual(TEXT("It lands on the lead"), 0.5f + Landing.PlayRate * GSTNMontageSyncTests::Step, 0.505f, 1e-5f);
	return true;
}

#endif

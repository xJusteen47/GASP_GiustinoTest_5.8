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

	/** One render frame of a mesh: what FinalizeFrame does, then the animation update advancing the instance. Returns the position drawn */
	float PresentFrame(float& InstancePosition, float Target, float NominalRate, float AnimDelta)
	{
		const FSyncedMontagePresentationStep PresentStep = UNetMontageSimulator::ComputeLandOnSimTimeStep(InstancePosition, Target, NominalRate, AnimDelta);
		if (PresentStep.bSetPosition)
		{
			InstancePosition = PresentStep.Position;
		}
		InstancePosition += PresentStep.PlayRate * AnimDelta;
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
		float Instance = 0.f;
		bool bAllOn = true;
		for (int32 RenderFrame = 1; RenderFrame <= 40; ++RenderFrame)
		{
			const float Target = RenderFrame * Dt * CardRate;
			bAllOn &= FMath::IsNearlyEqual(PresentFrame(Instance, Target, CardRate, Dt), Target, Tolerance);
		}
		TestTrue(TEXT("a running montage is drawn on the simulation time every frame"), bAllOn);
	}

	// The fixed tick without smoothing: the target moves by whole steps, sometimes two in a frame, sometimes none
	{
		float Instance = 0.f;
		const int32 Ticks[] = {1, 2, 3, 3, 4, 6, 7, 8, 8, 9};
		bool bAllOn = true;
		for (const int32 Tick : Ticks)
		{
			const float Target = Tick * Step * CardRate;
			bAllOn &= FMath::IsNearlyEqual(PresentFrame(Instance, Target, CardRate, Dt), Target, Tolerance);
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

#endif

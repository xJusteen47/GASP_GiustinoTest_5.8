// Tests of the combat trace sink (E17). Headless: no world, no simulation tick. What they pin is the sink contract (the call pattern the tick
// uses reaches an installed sink once per frame and nothing with a null sink), the mapping of the sync core into the trace record, and the
// console variable that installs the debug sink. The call site inside the tick itself is verified by the PIE harness, not here.

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Input/GSTN_InputSample.h"
#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTraceBuild.h"
#include "Trace/GSTN_CombatTrace.h"
#include "Trace/GSTN_CombatTraceSink.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#define GSTN_TRACE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** Installs a sink for the scope of a test and puts back what was there (the debug module sink, or none) */
	struct FScopedTraceSink
	{
		explicit FScopedTraceSink(IGSTN_CombatTraceSink* Sink) : Previous(GSTNCombatTrace::GetSink()) { GSTNCombatTrace::SetSink(Sink); }
		~FScopedTraceSink() { GSTNCombatTrace::SetSink(Previous); }
		IGSTN_CombatTraceSink* Previous;
	};

	/** Counts only: no trace events written, so a test does not depend on the channel being open */
	class FCountingSink : public IGSTN_CombatTraceSink
	{
	public:
		virtual void OnCombatFrame(const FGSTN_CombatTraceFrame& Frame) override { Frames.Add(Frame); }
		virtual void OnCombatEvent(const FGSTN_CombatTraceEvent& Event) override { Events.Add(Event); }
		TArray<FGSTN_CombatTraceFrame> Frames;
		TArray<FGSTN_CombatTraceEvent> Events;
	};

	/** The end-of-tick pattern of UGSTN_CombatSimComponent::SimulationTick, on a state the test advances by hand */
	void TickLikeTheComponent(int32 Frame, bool bResimulated, const FGSTN_CombatSyncCore& Core)
	{
		if (IGSTN_CombatTraceSink* Sink = GSTNCombatTrace::GetSink())
		{
			Sink->OnCombatFrame(GSTNCombatTraceNP::MakeFrame(7, Frame, bResimulated, Core));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TraceSinkFramesTest, "GSTNCombat.Debug.Trace.SinkReceivesOneFramePerTick", GSTN_TRACE_TEST_FLAGS)
bool FGSTN_TraceSinkFramesTest::RunTest(const FString& Parameters)
{
	FCountingSink Sink;
	FScopedTraceSink Scope(&Sink);

	constexpr int32 NumFrames = 30;
	FGSTN_CombatSyncCore Core;
	for (int32 Frame = 1; Frame <= NumFrames; ++Frame)
	{
		FGSTN_InputSample Sample;
		Sample.Frame = Frame;
		Core.Input.History.Append(Sample);
		TickLikeTheComponent(Frame, false, Core);
	}

	TestEqual(TEXT("one CombatFrame per frame"), Sink.Frames.Num(), NumFrames);
	bool bIncreasing = true;
	for (int32 Index = 1; Index < Sink.Frames.Num(); ++Index)
	{
		bIncreasing &= Sink.Frames[Index].Frame > Sink.Frames[Index - 1].Frame;
	}
	TestTrue(TEXT("the frames increase"), bIncreasing);
	TestEqual(TEXT("the entity id is carried"), Sink.Frames.Last().EntityId, 7);
	TestEqual(TEXT("the history head is the latest input frame"), Sink.Frames.Last().HistoryHead, NumFrames);
	TestEqual(TEXT("no event without an event site"), Sink.Events.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TraceNullSinkTest, "GSTNCombat.Debug.Trace.NullSinkCallsNothing", GSTN_TRACE_TEST_FLAGS)
bool FGSTN_TraceNullSinkTest::RunTest(const FString& Parameters)
{
	FCountingSink Bystander;
	FScopedTraceSink Scope(nullptr);
	TestNull(TEXT("no sink installed"), GSTNCombatTrace::GetSink());

	FGSTN_CombatSyncCore Core;
	for (int32 Frame = 1; Frame <= 30; ++Frame)
	{
		TickLikeTheComponent(Frame, false, Core);
	}
	TestEqual(TEXT("a sink that is not installed is never called"), Bystander.Frames.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TraceMappingTest, "GSTNCombat.Debug.Trace.FrameMapsTheSyncCore", GSTN_TRACE_TEST_FLAGS)
bool FGSTN_TraceMappingTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.ActiveTimeline = 3;
	Core.TimelineStartFrame = 10;
	Core.TimelinePausedFrames = 2;
	Core.HitstopEndFrame = 20;
	Core.Hit.HitstunEndFrame = 30;

	// Re-simulated flag: a frame re-run after a rollback is reported as such, and the record is the same function of the state otherwise.
	// A forced rollback of a real NP instance needs the PIE harness: no existing headless fixture ticks the component
	const FGSTN_CombatTraceFrame Replay = GSTNCombatTraceNP::MakeFrame(1, 15, true, Core);
	const FGSTN_CombatTraceFrame First = GSTNCombatTraceNP::MakeFrame(1, 15, false, Core);
	TestTrue(TEXT("resimulated flag set"), Replay.bResimulated);
	TestFalse(TEXT("resimulated flag clear on the first pass"), First.bResimulated);
	TestEqual(TEXT("same record apart from the flag"), Replay.TimelineFrame, First.TimelineFrame);

	TestEqual(TEXT("timeline index"), First.TimelineIndex, 3);
	TestEqual(TEXT("timeline frame excludes the paused frames"), First.TimelineFrame, 3);
	TestEqual(TEXT("hitstop frames to come"), First.HitstopFrames, 4);
	TestEqual(TEXT("hitstun frames to come"), First.HitstunFrames, 14);

	Core.ActionState = EGSTN_ActionState::Actionable;
	const FGSTN_CombatTraceFrame Idle = GSTNCombatTraceNP::MakeFrame(1, 40, false, Core);
	TestEqual(TEXT("no timeline outside one"), Idle.TimelineIndex, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no hitstop left after its end"), Idle.HitstopFrames, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TraceVariableTest, "GSTNCombat.Debug.Trace.ConsoleVariableInstallsTheSink", GSTN_TRACE_TEST_FLAGS)
bool FGSTN_TraceVariableTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Combat.Debug.Trace"));
	if (!TestNotNull(TEXT("the variable exists"), Variable))
	{
		return false;
	}
	const int32 Before = Variable->GetInt();
	IGSTN_CombatTraceSink* PreviousSink = GSTNCombatTrace::GetSink();

	Variable->Set(0, ECVF_SetByConsole);
	TestNull(TEXT("off: no sink"), GSTNCombatTrace::GetSink());

	Variable->Set(1, ECVF_SetByConsole);
	TestNotNull(TEXT("on: a sink is installed"), GSTNCombatTrace::GetSink());
	TestTrue(TEXT("it is the debug module sink"), GSTNCombatTrace::GetSink() == FGSTN_CombatTraceController::GetInstalledSink());

	if (const FGSTN_CombatTraceSink* Installed = FGSTN_CombatTraceController::GetInstalledSink())
	{
		const int32 Frames = Installed->NumFrames;
		FGSTN_CombatSyncCore Core;
		TickLikeTheComponent(1, false, Core);
		TestEqual(TEXT("the installed sink counts the frame"), Installed->NumFrames, Frames + 1);
	}

	Variable->Set(0, ECVF_SetByConsole);
	TestNull(TEXT("off again: removed"), GSTNCombatTrace::GetSink());

	Variable->Set(Before, ECVF_SetByConsole);
	if (Before == 0)
	{
		GSTNCombatTrace::SetSink(PreviousSink);
	}
	return true;
}

// Micro-measure, never fails: the cost of the call site with no sink installed against the same loop with no branch
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TraceNullCostTest, "GSTNCombat.Debug.Trace.NullSinkCost", GSTN_TRACE_TEST_FLAGS)
bool FGSTN_TraceNullCostTest::RunTest(const FString& Parameters)
{
	FScopedTraceSink Scope(nullptr);
	constexpr int32 Iterations = 100000;
	FGSTN_CombatSyncCore Core;
	volatile int32 Dummy = 0;

	const double WithBranchStart = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < Iterations; ++Index)
	{
		if (IGSTN_CombatTraceSink* Installed = GSTNCombatTrace::GetSink())
		{
			Installed->OnCombatFrame(GSTNCombatTraceNP::MakeFrame(1, Index, false, Core));
		}
		Dummy = Index;
	}
	const double WithBranch = FPlatformTime::Seconds() - WithBranchStart;

	const double NoBranchStart = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < Iterations; ++Index)
	{
		Dummy = Index;
	}
	const double NoBranch = FPlatformTime::Seconds() - NoBranchStart;

	AddInfo(FString::Printf(TEXT("null-sink branch x%d: %.3f ms with, %.3f ms without (%.2f ns per call site)"), Iterations, WithBranch * 1000.0, NoBranch * 1000.0,
		(WithBranch - NoBranch) * 1.0e9 / Iterations));
	return true;
}

#endif

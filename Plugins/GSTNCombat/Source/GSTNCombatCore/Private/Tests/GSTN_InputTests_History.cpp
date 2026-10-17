#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputScript.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using EDir = EGSTN_StickDirection;

	/** Sample for a frame with nothing happening on the buttons and the stick neutral */
	FGSTN_InputSample IdleSample(int32 Frame, uint32 Held = 0)
	{
		FGSTN_InputSample Sample;
		Sample.Frame = Frame;
		Sample.Held = Held;
		return Sample;
	}

	FGSTN_InputSample TapSample(int32 Frame, uint32 ButtonMask)
	{
		FGSTN_InputSample Sample = IdleSample(Frame);
		Sample.Pressed = ButtonMask;
		Sample.Released = ButtonMask;
		return Sample;
	}

	FGSTN_InputHistory MustBuild(FAutomationTestBase& Test, const TCHAR* Notation, const UGSTN_CombatInputSettings& Settings)
	{
		FGSTN_InputHistory History;
		int32 LastFrame = 0;
		Test.TestTrue(*FString::Printf(TEXT("script builds: %s"), Notation), FGSTN_InputScript::Build(Notation, Settings, History, LastFrame));
		return History;
	}

	TArray<uint8> Save(FGSTN_InputHistory& History, int32 Frame)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		History.NetSerialize(Writer, Frame);
		return Bytes;
	}

	bool Load(FGSTN_InputHistory& OutHistory, const TArray<uint8>& Bytes, int32 Frame)
	{
		FMemoryReader Reader(Bytes);
		OutHistory.NetSerialize(Reader, Frame);
		return !Reader.IsError();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistoryRingTest, "GSTNCombat.Core.Input.History.Ring", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistoryRingTest::RunTest(const FString& Parameters)
{
	// 20 frames with a tap each: 40 events, the ring keeps the newest 16 (frames 12..19).
	FGSTN_InputHistory History;
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		History.Append(TapSample(Frame, 1u));
	}
	TestEqual(TEXT("ring is full"), History.GetNumButtonEvents(), FGSTN_InputHistory::MaxEvents);
	TestEqual(TEXT("newest event is the last release"), History.GetButtonEvent(History.GetButtonSlot(0)).Frame, 19);
	TestTrue(TEXT("newest event is a release"), History.GetButtonEvent(History.GetButtonSlot(0)).bRelease != 0);
	TestEqual(TEXT("oldest kept event is from frame 12"), History.GetButtonEvent(History.GetButtonSlot(15)).Frame, 12);
	TestTrue(TEXT("oldest kept event is a press"), History.GetButtonEvent(History.GetButtonSlot(15)).bRelease == 0);

	// The stick ring overflows the same way: a new direction every frame.
	FGSTN_InputHistory Stick;
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		FGSTN_InputSample Sample = IdleSample(Frame);
		Sample.Stick.Zone = EGSTN_StickZone::Tilt;
		Sample.Stick.StickDir = static_cast<EDir>(1 + Frame % 8);
		Stick.Append(Sample);
	}
	TestEqual(TEXT("stick ring is full"), Stick.GetNumDirectionEvents(), FGSTN_InputHistory::MaxEvents);
	TestEqual(TEXT("newest stick event"), Stick.GetDirectionEvent(0).Frame, 19);
	TestEqual(TEXT("oldest kept stick event"), Stick.GetDirectionEvent(15).Frame, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistoryOrderTest, "GSTNCombat.Core.Input.History.SameFrameOrder", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistoryOrderTest::RunTest(const FString& Parameters)
{
	// A tap of buttons 0 and 2 on one frame: presses in button order, then the releases.
	FGSTN_InputHistory History;
	History.Append(TapSample(5, 0b101u));
	TestEqual(TEXT("four events"), History.GetNumButtonEvents(), 4);

	const auto Describe = [&History](int32 Age)
	{
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		return FString::Printf(TEXT("%s%d@%d"), Event.bRelease ? TEXT("-") : TEXT("+"), static_cast<int32>(Event.Button), Event.Frame);
	};
	TestEqual(TEXT("oldest is the press of button 0"), Describe(3), FString(TEXT("+0@5")));
	TestEqual(TEXT("then the press of button 2"), Describe(2), FString(TEXT("+2@5")));
	TestEqual(TEXT("then the release of button 0"), Describe(1), FString(TEXT("-0@5")));
	TestEqual(TEXT("newest is the release of button 2"), Describe(0), FString(TEXT("-2@5")));
	TestEqual(TEXT("a tap leaves nothing held"), History.GetHeldButtons(), 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistoryAgeTest, "GSTNCombat.Core.Input.History.EventAge", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistoryAgeTest::RunTest(const FString& Parameters)
{
	// A press kept for 254 frames is still there, at 255 it is dropped.
	FGSTN_InputHistory History;
	FGSTN_InputSample Press = IdleSample(0, 1u);
	Press.Pressed = 1u;
	History.Append(Press);
	for (int32 Frame = 1; Frame <= 254; ++Frame)
	{
		History.Append(IdleSample(Frame, 1u));
	}
	TestEqual(TEXT("a 254 frame old press is kept"), History.GetNumButtonEvents(), 1);
	History.Append(IdleSample(255, 1u));
	TestEqual(TEXT("a 255 frame old press is dropped"), History.GetNumButtonEvents(), 0);
	TestEqual(TEXT("the button is still held"), History.GetHeldButtons(), 1u);

	// The newest stick event is the current stick: it is never dropped, it only becomes ancient.
	FGSTN_InputHistory Stick;
	FGSTN_InputSample Up = IdleSample(0);
	Up.Stick.StickDir = EDir::Up;
	Up.Stick.Zone = EGSTN_StickZone::Tilt;
	Stick.Append(Up);
	for (int32 Frame = 1; Frame <= 300; ++Frame)
	{
		FGSTN_InputSample Sample = IdleSample(Frame);
		Sample.Stick = Up.Stick;
		Stick.Append(Sample);
	}
	TestEqual(TEXT("one stick event"), Stick.GetNumDirectionEvents(), 1);
	TestEqual(TEXT("it is ancient"), Stick.GetDirectionEvent(0).Frame, FGSTN_InputHistory::AncientFrame);
	TestEqual(TEXT("the stick is still Up"), Stick.GetDirectionAt(300, EGSTN_DirectionSpace::Stick), EDir::Up);
	TestEqual(TEXT("and was Up long ago"), Stick.GetDirectionAt(10, EGSTN_DirectionSpace::Stick), EDir::Up);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistoryDirectionAtTest, "GSTNCombat.Core.Input.History.DirectionAt", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistoryDirectionAtTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();
	FGSTN_InputHistory History = MustBuild(*this, TEXT("2:8t 10:6t 20:5 25:+A"), *Settings);

	const auto Dir = [&History](int32 Frame) { return History.GetDirectionAt(Frame, EGSTN_DirectionSpace::Stick); };
	TestEqual(TEXT("before the first event"), Dir(1), EDir::Neutral);
	TestEqual(TEXT("on the first event"), Dir(2), EDir::Up);
	TestEqual(TEXT("between events the stick keeps its state"), Dir(9), EDir::Up);
	TestEqual(TEXT("on the second event"), Dir(10), EDir::Right);
	TestEqual(TEXT("still Right"), Dir(19), EDir::Right);
	TestEqual(TEXT("released"), Dir(20), EDir::Neutral);
	TestEqual(TEXT("at the latest frame"), Dir(25), EDir::Neutral);
	TestEqual(TEXT("in the Facing space too (camera and character at 0)"), History.GetDirectionAt(10, EGSTN_DirectionSpace::Facing), EDir::Right);

	// A flick is only visible on its own frame.
	FGSTN_InputHistory Flick = MustBuild(*this, TEXT("5:6t! 8:+A"), *Settings);
	TestTrue(TEXT("flick on its frame"), Flick.GetStickAt(5).bFlick);
	TestFalse(TEXT("not on the next ones"), Flick.GetStickAt(6).bFlick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistoryHeldTest, "GSTNCombat.Core.Input.History.HeldAndConsume", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistoryHeldTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();
	// A pressed at 1 and held; B pressed at 3, released at 6, pressed again at 8 and held; C tapped at 10.
	FGSTN_InputHistory History = MustBuild(*this, TEXT("1:+A 3:+B 6:-B 8:+B 10:*C 12:5"), *Settings);

	TestTrue(TEXT("A held since its press"), History.IsHeldContinuouslySince(0, 1));
	TestFalse(TEXT("B not held since the first press (released in between)"), History.IsHeldContinuouslySince(1, 3));
	TestTrue(TEXT("B held since the second press"), History.IsHeldContinuouslySince(1, 8));
	TestFalse(TEXT("C was only tapped"), History.IsHeldContinuouslySince(2, 10));

	// Find the press of A and consume it.
	int32 PressSlot = INDEX_NONE;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		if (!Event.bRelease && Event.Button == 0)
		{
			PressSlot = History.GetButtonSlot(Age);
		}
	}
	TestTrue(TEXT("press of A found"), PressSlot != INDEX_NONE);
	TestEqual(TEXT("not consumed yet"), static_cast<int32>(History.GetButtonEvent(PressSlot).bConsumed), 0);
	History.Consume(PressSlot);
	TestEqual(TEXT("consumed"), static_cast<int32>(History.GetButtonEvent(PressSlot).bConsumed), 1);

	// Flushing B up to frame 8 consumes both presses of B, never a release and never another button.
	History.FlushPresses(0b10u, 8);
	int32 UnconsumedB = 0;
	int32 UnconsumedC = 0;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		UnconsumedB += (!Event.bRelease && Event.Button == 1 && !Event.bConsumed) ? 1 : 0;
		UnconsumedC += (!Event.bRelease && Event.Button == 2 && !Event.bConsumed) ? 1 : 0;
	}
	TestEqual(TEXT("presses of B flushed"), UnconsumedB, 0);
	TestEqual(TEXT("press of C untouched"), UnconsumedC, 1);

	// Flushing does not stop the button from being held: only the press is consumed.
	TestTrue(TEXT("B is still down"), History.IsHeldContinuouslySince(1, 8));

	// A flush limited by frame leaves newer presses alone.
	FGSTN_InputHistory Limited = MustBuild(*this, TEXT("1:*A 5:*A"), *Settings);
	Limited.FlushPresses(1u, 3);
	int32 Remaining = 0;
	for (int32 Age = 0; Age < Limited.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent& Event = Limited.GetButtonEvent(Limited.GetButtonSlot(Age));
		Remaining += (!Event.bRelease && !Event.bConsumed) ? 1 : 0;
	}
	TestEqual(TEXT("only the older press was flushed"), Remaining, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistoryTurningTest, "GSTNCombat.Core.Input.History.TurningAddsOneEventPerSector", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistoryTurningTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();

	for (const bool bTurnCamera : { false, true })
	{
		FGSTN_InputScript Script;
		Script.SetStick(0, 0.f, EGSTN_StickZone::Tilt);
		for (int32 Degrees = 1; Degrees <= 360; ++Degrees)
		{
			if (bTurnCamera)
			{
				Script.SetCameraYaw(Degrees, static_cast<float>(Degrees));
			}
			else
			{
				Script.SetFacingYaw(Degrees, static_cast<float>(Degrees));
			}
		}

		FGSTN_InputHistory History;
		TSet<int32> EventFrames;
		for (int32 Frame = 0; Frame <= 360; ++Frame)
		{
			TestTrue(TEXT("advance"), Script.Advance(History, *Settings, Frame));
			EventFrames.Add(History.GetDirectionEvent(0).Frame);
		}
		// The first push of the stick, then one change per 45 degree sector of a full turn.
		TestEqual(bTurnCamera ? TEXT("camera turn: events") : TEXT("character turn: events"), EventFrames.Num(), 1 + 8);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistorySerializationTest, "GSTNCombat.Core.Input.History.Serialization", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistorySerializationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();

	// A busy history: buttons, a consumed press, a flick, a camera turn.
	FGSTN_InputHistory History = MustBuild(*this,
		TEXT("0:cam=40 0:face=100 3:8t 5:+A 6:+C 7:*B 9:6t! 12:-A 14:3a 20:+D 30:-D 31:5"), *Settings);
	History.Consume(History.GetButtonSlot(2));
	const int32 Frame = History.GetLatestFrame();

	TArray<uint8> Bytes = Save(History, Frame);
	FGSTN_InputHistory Loaded;
	TestTrue(TEXT("loads"), Load(Loaded, Bytes, Frame));
	TestTrue(TEXT("round trip is equal"), History == Loaded);

	// Equality is logical, not bitwise: a ring that wrapped around holds the same events at other slots than a loaded one.
	FGSTN_InputHistory Wrapped;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		FGSTN_InputSample Sample = TapSample(Index, 1u << (Index % 3));
		Sample.Stick.Zone = EGSTN_StickZone::Tilt;
		Sample.Stick.StickDir = static_cast<EDir>(1 + Index % 8);
		Wrapped.Append(Sample);
	}
	FGSTN_InputHistory WrappedLoaded;
	TestTrue(TEXT("wrapped history loads"), Load(WrappedLoaded, Save(Wrapped, 19), 19));
	TestTrue(TEXT("wrapped history round trip is equal"), Wrapped == WrappedLoaded);
	TestTrue(TEXT("different histories are not equal"), !(History == Wrapped));

	TArray<uint8> Again = Save(Loaded, Frame);
	TestTrue(TEXT("saving a loaded history gives the same bytes"), Again == Bytes);

	// A history that goes on after the load behaves like the original.
	FGSTN_InputSample Next = IdleSample(Frame + 1);
	Next.Pressed = 2u;
	Next.Held = 2u;
	History.Append(Next);
	Loaded.Append(Next);
	TestTrue(TEXT("original and loaded copy stay equal"), History == Loaded);

	// Garbage is rejected.
	TArray<uint8> Garbage = { 0xFF, 0xFF, 0x00 };
	FGSTN_InputHistory Bad;
	TestFalse(TEXT("corrupt counts are an error"), Load(Bad, Garbage, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HistorySizeTest, "GSTNCombat.Core.Input.History.SerializedSize", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HistorySizeTest::RunTest(const FString& Parameters)
{
	// Idle: nothing happened for more than 255 frames.
	FGSTN_InputHistory Idle;
	Idle.Append(TapSample(0, 1u));
	for (int32 Frame = 1; Frame <= 300; ++Frame)
	{
		Idle.Append(IdleSample(Frame));
	}
	TestTrue(TEXT("an idle history is at most 4 bytes"), Save(Idle, 300).Num() <= 4);

	// Worst case: both rings full, every frame a new stick direction and a tap.
	FGSTN_InputHistory Busy;
	for (int32 Frame = 0; Frame < 16; ++Frame)
	{
		FGSTN_InputSample Sample = TapSample(Frame, 1u << (Frame % 4));
		Sample.Stick.Zone = EGSTN_StickZone::Tilt;
		Sample.Stick.StickDir = static_cast<EDir>(1 + Frame % 8);
		Sample.Stick.WorldYaw = static_cast<uint8>(Frame + 1);
		Busy.Append(Sample);
	}
	const int32 BusyBytes = Save(Busy, 15).Num();
	AddInfo(FString::Printf(TEXT("busy history: %d bytes"), BusyBytes));
	TestTrue(TEXT("a full history is at most 100 bytes"), BusyBytes <= 100);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_DirectionClassifier.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using EDir = EGSTN_StickDirection;

	FGSTN_StickSample ClassifyOnce(const FVector2D& Stick, float CameraYaw, float FacingYaw, const UGSTN_CombatInputSettings& Settings)
	{
		FGSTN_ClassifierState State;
		return FGSTN_DirectionClassifier::Classify(Stick, CameraYaw, FacingYaw, FGSTN_ClassifierState(), 10, Settings, State);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClassifierZonesTest, "GSTNCombat.Core.Input.Classifier.Zones", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ClassifierZonesTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();

	// Dead zone 0.2, aim zone 0.5: the limits belong to the upper zone.
	TestEqual(TEXT("zero stick is Neutral"), ClassifyOnce(FVector2D::ZeroVector, 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Neutral);
	TestEqual(TEXT("just under the dead zone"), ClassifyOnce(FVector2D(0.f, 0.19999f), 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Neutral);
	TestEqual(TEXT("exactly the dead zone"), ClassifyOnce(FVector2D(0.f, 0.2f), 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Aim);
	TestEqual(TEXT("just under the aim zone"), ClassifyOnce(FVector2D(0.f, 0.49999f), 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Aim);
	TestEqual(TEXT("exactly the aim zone"), ClassifyOnce(FVector2D(0.f, 0.5f), 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Tilt);
	TestEqual(TEXT("full stick"), ClassifyOnce(FVector2D(0.f, 1.f), 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Tilt);

	// The zone depends on the magnitude only, not on where the stick points.
	TestEqual(TEXT("diagonal full stick is Tilt"), ClassifyOnce(GSTNTest::StickAt(135.f), 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Tilt);
	TestEqual(TEXT("diagonal light push is Aim"), ClassifyOnce(GSTNTest::StickAt(135.f, 0.3f), 0.f, 0.f, *Settings).Zone, EGSTN_StickZone::Aim);

	// A Neutral stick has no direction in any space.
	const FGSTN_StickSample Neutral = ClassifyOnce(FVector2D(0.f, 0.1f), 90.f, 45.f, *Settings);
	TestTrue(TEXT("Neutral has no direction"), Neutral.StickDir == EDir::Neutral && Neutral.FacingDir == EDir::Neutral && Neutral.WorldDir == EDir::Neutral);
	TestEqual(TEXT("Neutral has no world yaw"), Neutral.WorldYaw, static_cast<uint8>(0));

	// An Aim push has real directions: it aims an attack without making it directional.
	const FGSTN_StickSample Aim = ClassifyOnce(FVector2D(0.3f, 0.f), 0.f, 0.f, *Settings);
	TestEqual(TEXT("Aim keeps the direction"), Aim.StickDir, EDir::Right);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClassifierSectorsTest, "GSTNCombat.Core.Input.Classifier.Sectors", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ClassifierSectorsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();

	const EDir Expected[8] = { EDir::Up, EDir::UpRight, EDir::Right, EDir::DownRight, EDir::Down, EDir::DownLeft, EDir::Left, EDir::UpLeft };
	for (int32 Sector = 0; Sector < 8; ++Sector)
	{
		const float Centre = Sector * 45.f;
		const FString Name = FString::Printf(TEXT("sector %d"), Sector);
		TestEqual(*(Name + TEXT(" centre")), FGSTN_DirectionClassifier::AngleToDirection(Centre, 22.5f), Expected[Sector]);
		TestEqual(*(Name + TEXT(" just before the upper limit")), FGSTN_DirectionClassifier::AngleToDirection(Centre + 22.f, 22.5f), Expected[Sector]);
		TestEqual(*(Name + TEXT(" just after the lower limit")), FGSTN_DirectionClassifier::AngleToDirection(Centre - 22.f, 22.5f), Expected[Sector]);
	}

	// Angles outside [0, 360) wrap.
	TestEqual(TEXT("negative angle"), FGSTN_DirectionClassifier::AngleToDirection(-90.f, 22.5f), EDir::Left);
	TestEqual(TEXT("angle over a full turn"), FGSTN_DirectionClassifier::AngleToDirection(450.f, 22.5f), EDir::Right);
	TestEqual(TEXT("just under a full turn"), FGSTN_DirectionClassifier::AngleToDirection(359.f, 22.5f), EDir::Up);

	// A boundary angle belongs to the cardinal direction.
	TestEqual(TEXT("boundary Up/UpRight"), FGSTN_DirectionClassifier::AngleToDirection(22.5f, 22.5f), EDir::Up);

	// Narrower diagonals: with a 10 degree half-width a diagonal needs 35..55 degrees.
	TestEqual(TEXT("30 degrees with narrow diagonals"), FGSTN_DirectionClassifier::AngleToDirection(30.f, 10.f), EDir::Up);
	TestEqual(TEXT("40 degrees with narrow diagonals"), FGSTN_DirectionClassifier::AngleToDirection(40.f, 10.f), EDir::UpRight);
	TestEqual(TEXT("100 degrees with narrow diagonals"), FGSTN_DirectionClassifier::AngleToDirection(100.f, 10.f), EDir::Right);
	TestEqual(TEXT("320 degrees with narrow diagonals"), FGSTN_DirectionClassifier::AngleToDirection(320.f, 10.f), EDir::UpLeft);

	// The stick vector and its angle agree (numpad layout of the input notation).
	TestEqual(TEXT("stick up"), ClassifyOnce(FVector2D(0.f, 1.f), 0.f, 0.f, *Settings).StickDir, EDir::Up);
	TestEqual(TEXT("stick right"), ClassifyOnce(FVector2D(1.f, 0.f), 0.f, 0.f, *Settings).StickDir, EDir::Right);
	TestEqual(TEXT("stick down"), ClassifyOnce(FVector2D(0.f, -1.f), 0.f, 0.f, *Settings).StickDir, EDir::Down);
	TestEqual(TEXT("stick left"), ClassifyOnce(FVector2D(-1.f, 0.f), 0.f, 0.f, *Settings).StickDir, EDir::Left);
	TestEqual(TEXT("stick up-right"), ClassifyOnce(FVector2D(1.f, 1.f), 0.f, 0.f, *Settings).StickDir, EDir::UpRight);
	TestEqual(TEXT("stick down-left"), ClassifyOnce(FVector2D(-1.f, -1.f), 0.f, 0.f, *Settings).StickDir, EDir::DownLeft);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClassifierSpacesTest, "GSTNCombat.Core.Input.Classifier.Spaces", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ClassifierSpacesTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();
	const FVector2D Up(0.f, 1.f);
	const FVector2D Right(1.f, 0.f);

	// The same stick under camera 0 and 90 degrees: the Stick direction is the controller's, the World one follows the camera.
	const FGSTN_StickSample Camera0 = ClassifyOnce(Up, 0.f, 0.f, *Settings);
	const FGSTN_StickSample Camera90 = ClassifyOnce(Up, 90.f, 0.f, *Settings);
	TestEqual(TEXT("Stick space ignores the camera (0)"), Camera0.StickDir, EDir::Up);
	TestEqual(TEXT("Stick space ignores the camera (90)"), Camera90.StickDir, EDir::Up);
	TestEqual(TEXT("World direction with camera 0"), Camera0.WorldDir, EDir::Up);
	TestEqual(TEXT("World direction with camera 90"), Camera90.WorldDir, EDir::Right);

	// Facing: pushing up with the camera behind a character that faces 90 degrees (right of the camera) is its left.
	TestEqual(TEXT("up while facing 90 is Left"), ClassifyOnce(Up, 0.f, 90.f, *Settings).FacingDir, EDir::Left);
	TestEqual(TEXT("right while facing 90 is Up (forward)"), ClassifyOnce(Right, 0.f, 90.f, *Settings).FacingDir, EDir::Up);
	TestEqual(TEXT("left while facing 90 is Down (back)"), ClassifyOnce(FVector2D(-1.f, 0.f), 0.f, 90.f, *Settings).FacingDir, EDir::Down);
	TestEqual(TEXT("facing 180: up is Down (back)"), ClassifyOnce(Up, 0.f, 180.f, *Settings).FacingDir, EDir::Down);

	// Rotating camera and character together keeps the Facing direction.
	TestEqual(TEXT("camera and facing both 90"), ClassifyOnce(Up, 90.f, 90.f, *Settings).FacingDir, EDir::Up);

	// The Facing direction never depends on the stick space result: Stick space is unaffected by facing.
	TestEqual(TEXT("Stick space ignores the facing"), ClassifyOnce(Up, 0.f, 135.f, *Settings).StickDir, EDir::Up);

	// World reference yaw: Up of the World space is that yaw.
	Settings->WorldReferenceYaw = 90.f;
	TestEqual(TEXT("world reference yaw 90, heading 0 is Left"), ClassifyOnce(Up, 0.f, 0.f, *Settings).WorldDir, EDir::Left);
	TestEqual(TEXT("world reference yaw 90, heading 90 is Up"), ClassifyOnce(Right, 0.f, 0.f, *Settings).WorldDir, EDir::Up);
	Settings->WorldReferenceYaw = 0.f;

	// Aim heading: the stick rotated by the camera, 256 steps per turn.
	TestEqual(TEXT("world yaw, camera 0"), Camera0.WorldYaw, static_cast<uint8>(0));
	TestEqual(TEXT("world yaw, camera 90"), Camera90.WorldYaw, static_cast<uint8>(64));
	TestEqual(TEXT("world yaw, stick right camera 90"), ClassifyOnce(Right, 90.f, 0.f, *Settings).WorldYaw, static_cast<uint8>(128));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClassifierTurningTest, "GSTNCombat.Core.Input.Classifier.Turning", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ClassifierTurningTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();

	// Holding the stick while the character (or the camera) makes a full turn: the Facing/World direction changes once
	// per sector, so the history records at most eight events for it and not one per frame.
	for (const bool bTurnCamera : { false, true })
	{
		int32 Changes = 0;
		EDir Previous = EDir::Neutral;
		for (int32 Degrees = 0; Degrees <= 360; ++Degrees)
		{
			const FGSTN_StickSample Sample = bTurnCamera
				? ClassifyOnce(FVector2D(0.f, 1.f), static_cast<float>(Degrees), 0.f, *Settings)
				: ClassifyOnce(FVector2D(0.f, 1.f), 0.f, static_cast<float>(Degrees), *Settings);
			const EDir Current = bTurnCamera ? Sample.WorldDir : Sample.FacingDir;
			if (Degrees > 0 && Current != Previous)
			{
				++Changes;
			}
			Previous = Current;
			TestEqual(TEXT("Stick space does not change while turning"), Sample.StickDir, EDir::Up);
		}
		TestEqual(bTurnCamera ? TEXT("camera turn: one change per sector") : TEXT("character turn: one change per sector"), Changes, 8);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClassifierFlickTest, "GSTNCombat.Core.Input.Classifier.Flick", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ClassifierFlickTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();
	const FVector2D Neutral = FVector2D::ZeroVector;
	const FVector2D Light(0.f, 0.3f);
	const FVector2D Full(0.f, 1.f);

	// Runs a stick sequence (one entry per frame, starting at frame 0) and returns the frames that flicked.
	const auto FlickFrames = [&Settings](const TArray<FVector2D>& Sticks)
	{
		TArray<int32> Result;
		FGSTN_ClassifierState State;
		for (int32 Frame = 0; Frame < Sticks.Num(); ++Frame)
		{
			FGSTN_ClassifierState Next;
			if (FGSTN_DirectionClassifier::Classify(Sticks[Frame], 0.f, 0.f, State, Frame, *Settings, Next).bFlick)
			{
				Result.Add(Frame);
			}
			State = Next;
		}
		return Result;
	};

	// Neutral at 0, Tilt at 1: a flick, on that frame only.
	TestTrue(TEXT("immediate flick"), FlickFrames({ Neutral, Full, Full, Full }) == TArray<int32>({ 1 }));

	// Through the Aim zone, still inside FlickFrames (3): Neutral at 0, Tilt at 3.
	TestTrue(TEXT("flick at the limit"), FlickFrames({ Neutral, Light, Light, Full }) == TArray<int32>({ 3 }));

	// One frame slower: no flick.
	TestTrue(TEXT("too slow, no flick"), FlickFrames({ Neutral, Light, Light, Light, Full }).IsEmpty());

	// Tilt, release to Aim (never Neutral) for a while, push again: not a flick, Neutral was left too long ago.
	TestTrue(TEXT("re-tilt without passing Neutral"), FlickFrames({ Neutral, Full, Light, Light, Light, Full }) == TArray<int32>({ 1 }));

	// Tilt on the very first frame: nothing is known about the past, so no flick.
	TestTrue(TEXT("first frame in Tilt"), FlickFrames({ Full, Full }).IsEmpty());

	// Back to Neutral and flick again.
	TestTrue(TEXT("second flick"), FlickFrames({ Neutral, Full, Neutral, Neutral, Full }) == TArray<int32>({ 1, 4 }));

	// A custom FlickFrames.
	Settings->FlickFrames = 1;
	TestTrue(TEXT("FlickFrames = 1 rejects a two-frame push"), FlickFrames({ Neutral, Light, Full }).IsEmpty());
	TestTrue(TEXT("FlickFrames = 1 accepts a direct push"), FlickFrames({ Neutral, Full }) == TArray<int32>({ 1 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClassifierQuantizationTest, "GSTNCombat.Core.Input.Classifier.Quantization", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ClassifierQuantizationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();

	// Round trips are exact on the grid the command can carry.
	bool bStickRoundTrip = true;
	for (int32 Value = -127; Value <= 127; ++Value)
	{
		bStickRoundTrip &= FGSTN_DirectionClassifier::QuantizeStickAxis(FGSTN_DirectionClassifier::DequantizeStickAxis(static_cast<int8>(Value))) == Value;
	}
	TestTrue(TEXT("stick byte round trip"), bStickRoundTrip);
	TestEqual(TEXT("-128 reads as -1"), FGSTN_DirectionClassifier::DequantizeStickAxis(static_cast<int8>(-128)), -1.f);
	TestEqual(TEXT("stick axis clamps high"), static_cast<int32>(FGSTN_DirectionClassifier::QuantizeStickAxis(2.f)), 127);
	TestEqual(TEXT("stick axis clamps low"), static_cast<int32>(FGSTN_DirectionClassifier::QuantizeStickAxis(-2.f)), -127);

	bool bYawRoundTrip = true;
	for (int32 Value = 0; Value < 256; ++Value)
	{
		bYawRoundTrip &= FGSTN_DirectionClassifier::QuantizeYaw(FGSTN_DirectionClassifier::DequantizeYaw(static_cast<uint8>(Value))) == Value;
	}
	TestTrue(TEXT("camera yaw byte round trip"), bYawRoundTrip);
	TestEqual(TEXT("yaw wraps"), FGSTN_DirectionClassifier::QuantizeYaw(360.f), static_cast<uint8>(0));
	TestEqual(TEXT("negative yaw wraps"), FGSTN_DirectionClassifier::QuantizeYaw(-90.f), static_cast<uint8>(192));
	TestEqual(TEXT("yaw rounds to the nearest step"), FGSTN_DirectionClassifier::QuantizeYaw(1.f), static_cast<uint8>(1));

	// A float stick and the same stick after quantization classify the same, away from the sector boundaries (the
	// stick byte moves the angle by about half a degree at the lowest magnitude tested; the angles below stay 1.5
	// degrees or more from every boundary).
	int32 Mismatches = 0;
	for (const float CameraYaw : { 0.f, 90.f, 180.f })
	{
		for (const float Magnitude : { 0.4f, 0.8f, 1.f })
		{
			for (float Angle = 1.f; Angle < 360.f; Angle += 5.f)
			{
				const FVector2D Float = GSTNTest::StickAt(Angle, Magnitude);
				const FVector2D Quantized(
					FGSTN_DirectionClassifier::DequantizeStickAxis(FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Float.X))),
					FGSTN_DirectionClassifier::DequantizeStickAxis(FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Float.Y))));
				FGSTN_StickSample FromFloat = ClassifyOnce(Float, CameraYaw, 0.f, *Settings);
				FGSTN_StickSample FromQuantized = ClassifyOnce(Quantized, CameraYaw, 0.f, *Settings);

				// The aim heading is itself quantized: it may land one step apart.
				const int32 YawStepDifference = FMath::Abs(static_cast<int32>(FromFloat.WorldYaw) - static_cast<int32>(FromQuantized.WorldYaw));
				if ((YawStepDifference != 0 && YawStepDifference != 1 && YawStepDifference != 255))
				{
					++Mismatches;
				}
				FromFloat.WorldYaw = FromQuantized.WorldYaw = 0;
				if (!(FromFloat == FromQuantized))
				{
					++Mismatches;
				}
			}
		}
	}
	TestEqual(TEXT("quantized input classifies like the float one"), Mismatches, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

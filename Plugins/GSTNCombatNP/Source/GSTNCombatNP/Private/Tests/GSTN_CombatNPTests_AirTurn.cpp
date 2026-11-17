// Automation tests of the air turn over GASP's falling mode (polish M3, qa-movement-1117 defect 3): the rate entry survives the network,
// the generated move turns at the command's rate (not GASP's 300 deg/s), and it leaves alone what it must leave alone (no entry, a
// ground mode, an overriding layered move). The full loop (stick, Mover's tick with BP_MovementMode_Falling, two clients) is the
// movement QA scenario move_air_control (Tools/GASNPPTest/movement_scenarios.py).

#include "Misc/AutomationTest.h"
#include "Movement/GSTN_AirTurn.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNAirTurnTest
{
	// A start state facing Yaw, falling, with an orientation intent at IntentYaw and (Rate > 0) the air turn entry
	FMoverTickStartData MakeStart(float Yaw, float IntentYaw, float Rate)
	{
		FMoverTickStartData Start;
		Start.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>().SetTransforms_WorldSpace(FVector::ZeroVector,
			FRotator(0.0, Yaw, 0.0), FVector(400.0, 0.0, 200.0), FVector::ZeroVector);
		FCharacterDefaultInputs& Inputs = Start.InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
		Inputs.OrientationIntent = FRotator(0.0, IntentYaw, 0.0).Vector();
		if (Rate > 0.f)
		{
			Start.InputCmd.InputCollection.FindOrAddMutableDataByType<FGSTN_AirTurnInputs>().RateDegPerSec = Rate;
		}
		return Start;
	}

	// What GASP's BP_MovementMode_Falling hands over: its own turn, capped at 300 deg/s
	FProposedMove GaspMove(float Sign)
	{
		FProposedMove Move;
		Move.LinearVelocity = FVector(400.0, 0.0, 200.0);
		Move.AngularVelocityDegrees = FVector(0.0, 0.0, 300.0 * Sign);
		return Move;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirTurnInputsNetTest, "GSTNCombat.NP.AirTurn.Inputs", GSTN_NP_TEST_FLAGS)
bool FGSTN_AirTurnInputsNetTest::RunTest(const FString& Parameters)
{
	FGSTN_AirTurnInputs Sent;
	Sent.RateDegPerSec = 3600.4f;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	bool bOk = false;
	Sent.NetSerialize(Writer, nullptr, bOk);
	FGSTN_AirTurnInputs Received;
	FMemoryReader Reader(Bytes);
	Received.NetSerialize(Reader, nullptr, bOk);
	TestTrue(TEXT("serialized"), bOk);
	TestEqual(TEXT("whole degrees per second on the wire"), Received.RateDegPerSec, 3600.f);
	TestEqual(TEXT("the producer predicts with the value the server reads"), FGSTN_AirTurn::QuantizeRate(3600.4f), Received.RateDegPerSec);
	TestEqual(TEXT("negative rates clamp to 0"), FGSTN_AirTurn::QuantizeRate(-5.f), 0.f);
	TestEqual(TEXT("huge rates clamp to the wire's range"), FGSTN_AirTurn::QuantizeRate(1.e6f), FGSTN_AirTurn::MaxRateDegPerSec);
	return true;
}

// Regression: NP smoothing interpolates the input collection; without these overrides Mover's base checkf crashed the editor in PIE
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirTurnInputsSmoothingTest, "GSTNCombat.NP.AirTurn.InputsSmoothing", GSTN_NP_TEST_FLAGS)
bool FGSTN_AirTurnInputsSmoothingTest::RunTest(const FString& Parameters)
{
	FGSTN_AirTurnInputs From, To, Out;
	From.RateDegPerSec = 0.f;
	To.RateDegPerSec = 3600.f;
	FMoverDataCollection FromCollection, ToCollection, OutCollection;
	FromCollection.AddOrOverwriteData(MakeShared<FGSTN_AirTurnInputs>(From));
	ToCollection.AddOrOverwriteData(MakeShared<FGSTN_AirTurnInputs>(To));
	OutCollection.Interpolate(FromCollection, ToCollection, 0.25f);
	const FGSTN_AirTurnInputs* Lerped = OutCollection.FindDataByType<FGSTN_AirTurnInputs>();
	TestTrue(TEXT("the collection interpolates the entry"), Lerped != nullptr);
	if (Lerped) { TestEqual(TEXT("nearer frame's rate"), Lerped->RateDegPerSec, 0.f); }
	Out.Interpolate(From, To, 0.75f);
	TestEqual(TEXT("nearer frame's rate (late)"), Out.RateDegPerSec, 3600.f);
	TestTrue(TEXT("rate change reconciles"), From.ShouldReconcile(To));
	From.Merge(To);
	TestEqual(TEXT("merge keeps the turn"), From.RateDegPerSec, 3600.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirTurnApplyTest, "GSTNCombat.NP.AirTurn.Apply", GSTN_NP_TEST_FLAGS)
bool FGSTN_AirTurnApplyTest::RunTest(const FString& Parameters)
{
	using namespace GSTNAirTurnTest;
	const float Dt = 1.f / 62.5f;
	const FVector Up = FVector::UpVector;

	// The producer's step (57.6 at 3600 deg/s) is taken in one frame, not GASP's 4.8
	{
		const FMoverTickStartData Start = MakeStart(0.f, 57.6f, 3600.f);
		FProposedMove Move = GaspMove(1.f);
		TestTrue(TEXT("applied"), FGSTN_AirTurn::Apply(Start, true, Up, Dt, Move));
		TestEqual(TEXT("yaw rate = the command's rate"), Move.AngularVelocityDegrees.Z, 3600.0, 0.5);
		TestTrue(TEXT("only about the up"), FMath::IsNearlyZero(Move.AngularVelocityDegrees.X) && FMath::IsNearlyZero(Move.AngularVelocityDegrees.Y));
		TestTrue(TEXT("the linear velocity is the mode's"), Move.LinearVelocity.Equals(FVector(400.0, 0.0, 200.0)));
	}
	// The other side, across +-180
	{
		const FMoverTickStartData Start = MakeStart(170.f, -170.f, 3600.f);
		FProposedMove Move = GaspMove(-1.f);
		TestTrue(TEXT("applied across 180"), FGSTN_AirTurn::Apply(Start, true, Up, Dt, Move));
		TestEqual(TEXT("the short way: 20 degrees this frame"), Move.AngularVelocityDegrees.Z, 20.0 / Dt, 0.5);
	}
	// A wider gap than the rate allows is capped at the rate
	{
		const FMoverTickStartData Start = MakeStart(0.f, -120.f, 3600.f);
		FProposedMove Move = GaspMove(-1.f);
		FGSTN_AirTurn::Apply(Start, true, Up, Dt, Move);
		TestEqual(TEXT("capped at minus the rate"), Move.AngularVelocityDegrees.Z, -3600.0, 0.5);
	}
	// Left alone
	{
		const FMoverTickStartData Start = MakeStart(0.f, 57.6f, 0.f);
		FProposedMove Move = GaspMove(1.f);
		TestFalse(TEXT("no entry (stick idle, a move without air turn): GASP's turn"), FGSTN_AirTurn::Apply(Start, true, Up, Dt, Move));
		TestEqual(TEXT("no entry: unchanged"), Move.AngularVelocityDegrees.Z, 300.0);
	}
	{
		const FMoverTickStartData Start = MakeStart(0.f, 57.6f, 3600.f);
		FProposedMove Move = GaspMove(1.f);
		TestFalse(TEXT("a ground mode: its own facing rules"), FGSTN_AirTurn::Apply(Start, false, Up, Dt, Move));
		TestEqual(TEXT("ground: unchanged"), Move.AngularVelocityDegrees.Z, 300.0);
	}
	{
		const FMoverTickStartData Start = MakeStart(0.f, 57.6f, 3600.f);
		FProposedMove Move = GaspMove(1.f);
		Move.MixMode = EMoveMixMode::OverrideVelocity;
		Move.AngularVelocityDegrees = FVector::ZeroVector;
		TestFalse(TEXT("an overriding layered move owns the rotation"), FGSTN_AirTurn::Apply(Start, true, Up, Dt, Move));
		TestTrue(TEXT("override: unchanged"), Move.AngularVelocityDegrees.IsZero());
	}
	{
		FMoverTickStartData Start = MakeStart(0.f, 57.6f, 3600.f);
		Start.InputCmd.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>()->OrientationIntent = FVector::ZeroVector;
		FProposedMove Move = GaspMove(1.f);
		TestFalse(TEXT("no orientation intent: nothing to turn to"), FGSTN_AirTurn::Apply(Start, true, Up, Dt, Move));
	}

	// The loop: a 180 reversal at 3600 deg/s, the producer's step then this, faces the new way in 4 frames (QA bar: 8)
	{
		float Yaw = 0.f;
		int32 Frames = 0;
		while (FMath::Abs(FRotator::NormalizeAxis(180.f - Yaw)) > 0.05f && Frames < 100)
		{
			const float Gap = FRotator::NormalizeAxis(180.f - Yaw);
			const float Intent = FRotator::NormalizeAxis(Yaw + FMath::Clamp(Gap, -3600.f * Dt, 3600.f * Dt));
			const FMoverTickStartData Start = MakeStart(Yaw, Intent, 3600.f);
			FProposedMove Move = GaspMove(1.f);
			FGSTN_AirTurn::Apply(Start, true, Up, Dt, Move);
			Yaw = FRotator::NormalizeAxis(Yaw + static_cast<float>(Move.AngularVelocityDegrees.Z) * Dt);
			++Frames;
		}
		TestEqual(TEXT("180 in 4 frames"), Frames, 4);
	}
	return true;
}

#endif

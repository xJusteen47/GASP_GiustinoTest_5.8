// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "WallMovement/GSTN_WallMovementTypes.h"

namespace GSTNWallSyncStateTests
{
	/** A wall state with memory and a manual reset, as it looks on a client that has been on a wall */
	FGSTN_WallMovementSyncState MakeState()
	{
		FGSTN_WallMovementSyncState State;
		State.WallNormal = FVector(0.0, -1.0, 0.0);
		State.WallContactPoint = FVector(100.0, 200.0, 300.0);
		State.bHasWall = true;
		State.TimeWithoutWall = 0.f;
		State.PullAwayTime = 0.1f;
		State.bIsSkidding = false;
		State.bHasWallMemory = true;
		State.LastWallNormal = FVector(0.0, -1.0, 0.0);
		State.LastWallHeight = 250.f;
		State.LastWallTimeMs = 4000.0;
		State.MemoryResetTimeMs = 3000.0;
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallSyncStateReconcileTest, "GSTN.Wall.SyncState.ShouldReconcile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FGSTN_WallSyncStateReconcileTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallSyncStateTests;

	const FGSTN_WallMovementSyncState Authority = MakeState();

	{
		const FGSTN_WallMovementSyncState Predicted = MakeState();
		TestFalse(TEXT("Identical states do not reconcile"), Predicted.ShouldReconcile(Authority));
	}

	// Noise inside the tolerances must not reconcile
	{
		FGSTN_WallMovementSyncState Predicted = MakeState();
		Predicted.PullAwayTime += 0.005f;
		Predicted.TimeWithoutWall += 0.005f;
		Predicted.LastWallHeight += 0.5f;
		Predicted.LastWallTimeMs += 0.4;
		Predicted.MemoryResetTimeMs += 0.4;
		TestFalse(TEXT("Differences inside the tolerances do not reconcile"), Predicted.ShouldReconcile(Authority));
	}

	// Each field, pushed outside its tolerance (or flipped), must reconcile
	auto ExpectReconcile = [this, &Authority](const TCHAR* What, TFunctionRef<void(FGSTN_WallMovementSyncState&)> Mutate)
	{
		FGSTN_WallMovementSyncState Predicted = MakeState();
		Mutate(Predicted);
		TestTrue(FString::Printf(TEXT("%s reconciles"), What), Predicted.ShouldReconcile(Authority));
	};
	ExpectReconcile(TEXT("bHasWall"), [](FGSTN_WallMovementSyncState& S) { S.bHasWall = false; });
	ExpectReconcile(TEXT("WallNormal"), [](FGSTN_WallMovementSyncState& S) { S.WallNormal = FVector(1.0, 0.0, 0.0); });
	ExpectReconcile(TEXT("bIsSkidding"), [](FGSTN_WallMovementSyncState& S) { S.bIsSkidding = true; });
	ExpectReconcile(TEXT("PullAwayTime"), [](FGSTN_WallMovementSyncState& S) { S.PullAwayTime += 0.1f; });
	ExpectReconcile(TEXT("TimeWithoutWall"), [](FGSTN_WallMovementSyncState& S) { S.TimeWithoutWall += 0.1f; });
	ExpectReconcile(TEXT("bHasWallMemory"), [](FGSTN_WallMovementSyncState& S) { S.bHasWallMemory = false; });
	ExpectReconcile(TEXT("LastWallNormal"), [](FGSTN_WallMovementSyncState& S) { S.LastWallNormal = FVector(1.0, 0.0, 0.0); });
	ExpectReconcile(TEXT("LastWallHeight"), [](FGSTN_WallMovementSyncState& S) { S.LastWallHeight += 20.f; });
	ExpectReconcile(TEXT("LastWallTimeMs"), [](FGSTN_WallMovementSyncState& S) { S.LastWallTimeMs += 50.0; });
	ExpectReconcile(TEXT("MemoryResetTimeMs value"), [](FGSTN_WallMovementSyncState& S) { S.MemoryResetTimeMs += 50.0; });
	ExpectReconcile(TEXT("MemoryResetTimeMs set vs unset"), [](FGSTN_WallMovementSyncState& S) { S.MemoryResetTimeMs = -1.0; });

	// Off the wall and without memory, the zero normals must not count as a mismatch
	{
		FGSTN_WallMovementSyncState A;
		FGSTN_WallMovementSyncState B;
		TestFalse(TEXT("Default states do not reconcile"), A.ShouldReconcile(B));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallSyncStateSerializeTest, "GSTN.Wall.SyncState.NetSerializeRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FGSTN_WallSyncStateSerializeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallSyncStateTests;

	auto RoundTrip = [](FGSTN_WallMovementSyncState& In, FGSTN_WallMovementSyncState& Out)
	{
		FBitWriter Writer(4096);
		bool bSuccess = false;
		In.NetSerialize(Writer, nullptr, bSuccess);

		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		Out.NetSerialize(Reader, nullptr, bSuccess);
		return bSuccess && !Reader.IsError();
	};

	{
		FGSTN_WallMovementSyncState In = MakeState();
		FGSTN_WallMovementSyncState Out;
		TestTrue(TEXT("Round trip succeeds"), RoundTrip(In, Out));
		TestEqual(TEXT("MemoryResetTimeMs survives"), Out.MemoryResetTimeMs, 3000.0);
		TestEqual(TEXT("LastWallTimeMs survives"), Out.LastWallTimeMs, 4000.0);
		TestTrue(TEXT("Round-tripped state matches the original"), !In.ShouldReconcile(Out));
	}

	// Reset recorded while there is no memory: still has to arrive, or the two ends would reconcile forever
	{
		FGSTN_WallMovementSyncState In;
		In.MemoryResetTimeMs = 1234.0;
		FGSTN_WallMovementSyncState Out;
		TestTrue(TEXT("Round trip succeeds without memory"), RoundTrip(In, Out));
		TestEqual(TEXT("MemoryResetTimeMs survives without memory"), Out.MemoryResetTimeMs, 1234.0);
		TestFalse(TEXT("bHasWallMemory stays false"), Out.bHasWallMemory);
	}

	// No reset: the unset marker (negative) is restored over whatever the receiving state held
	{
		FGSTN_WallMovementSyncState In;
		FGSTN_WallMovementSyncState Out;
		Out.MemoryResetTimeMs = 99.0;
		TestTrue(TEXT("Round trip succeeds without reset"), RoundTrip(In, Out));
		TestTrue(TEXT("Unset reset time is restored"), Out.MemoryResetTimeMs < 0.0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

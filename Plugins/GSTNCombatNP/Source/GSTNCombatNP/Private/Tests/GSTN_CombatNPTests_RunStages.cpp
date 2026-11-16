// Automation tests of the run stages in Mover's sync state (docs/notes/corsa-mario-piano.md, P1): the entry survives the network,
// reconciles on a different stage, a restored sync state brings the stage back, and the stage values reach the walking mode's
// gait properties named by the profile. The full loop (stick, L3, Mover's tick, two clients) is the PIE playtest.

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Misc/AutomationTest.h"
#include "Movement/GSTN_RunStageSyncState.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/GSTN_StatsModifierTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStageSyncStateTest, "GSTNCombat.NP.RunStages.SyncState", GSTN_NP_TEST_FLAGS)
bool FGSTN_RunStageSyncStateTest::RunTest(const FString& Parameters)
{
	FGSTN_RunStageSyncState Sent;
	Sent.Run.Stage = 2;
	Sent.Run.FramesInStage = 7;
	Sent.Run.FramesSinceMoveStart = 120;
	Sent.Run.FramesSinceStickRelease = -1;
	Sent.Run.CarriedSpeed = 777.7f;
	Sent.Run.FramesSinceCarry = 3;
	Sent.Run.CarrySource = EGSTN_RunCarrySource::StickRelease;
	Sent.Run.LastFrame = 4242;

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	bool bOk = false;
	Sent.NetSerialize(Writer, nullptr, bOk);
	FGSTN_RunStageSyncState Received;
	FMemoryReader Reader(Bytes);
	Received.NetSerialize(Reader, nullptr, bOk);
	TestTrue(TEXT("serialized"), bOk);
	TestFalse(TEXT("the received state does not reconcile against the sent one"), Received.ShouldReconcile(Sent));

	FGSTN_RunStageSyncState Mispredicted = Received;
	Mispredicted.Run.Stage = 1;
	TestTrue(TEXT("a different stage reconciles"), Mispredicted.ShouldReconcile(Sent));

	// Rollback: the sync state saved for a frame brings its stage back, whatever was stepped after it
	FMoverSyncState Saved;
	Saved.SyncStateCollection.FindOrAddMutableDataByType<FGSTN_RunStageSyncState>().Run.Stage = 1;
	FMoverSyncState Live = Saved;
	Live.SyncStateCollection.FindOrAddMutableDataByType<FGSTN_RunStageSyncState>().Run.Stage = 2;
	TestEqual(TEXT("the saved copy is independent of the live one"), (int32)FGSTN_RunStageMover::Find(Saved)->Stage, 1);
	Live = Saved;
	TestEqual(TEXT("restoring the saved state restores the stage"), (int32)FGSTN_RunStageMover::Find(Live)->Stage, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStageValuesTest, "GSTNCombat.NP.RunStages.StageValues", GSTN_NP_TEST_FLAGS)
bool FGSTN_RunStageValuesTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNCombatNPRunStagesTestWorld"));
	World->AddToRoot();
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	AActor* Owner = World->SpawnActor<AActor>();
	UMoverComponent* Mover = NewObject<UMoverComponent>(Owner);
	UGSTN_TestGaitWalkingMode* Walking = NewObject<UGSTN_TestGaitWalkingMode>(Mover);
	Mover->MovementModes.Add(TEXT("Walking"), Walking);

	FGSTN_LocomotionSimTuning Tuning = FGSTN_LocomotionSimTuning::FromProfile(*GetDefault<UGSTN_LocomotionProfile>());
	const int32 Written = FGSTN_RunStageMover::ApplyStageValues(*Mover, Tuning, 2, 0.5f);
	TestEqual(TEXT("RunSpeed and SprintSpeed written (the test mode has no acceleration properties)"), Written, 2);
	TestEqual(TEXT("stage 3 speed * factor"), Walking->RunSpeed, Tuning.RunStages.Stages[2].MaxSpeed * 0.5f);
	TestEqual(TEXT("sprint gets the same"), Walking->SprintSpeed, Tuning.RunStages.Stages[2].MaxSpeed * 0.5f);
	TestEqual(TEXT("walk speed untouched"), Walking->WalkSpeed, 165.f);

	FGSTN_RunStageMover::RegisterPersistentState(*Mover);
	FGSTN_RunStageMover::RegisterPersistentState(*Mover);
	const int32 Persistent = Mover->PersistentSyncStateDataTypes.FilterByPredicate([](const FMoverDataPersistence& Entry)
		{ return Entry.RequiredType == FGSTN_RunStageSyncState::StaticStruct(); }).Num();
	TestEqual(TEXT("registered once as persistent"), Persistent, 1);

	{
		FGSTN_LocomotionSimTuning Off = Tuning;
		Off.RunStages.Stages.Reset();
		TestEqual(TEXT("an empty stage table writes nothing"), FGSTN_RunStageMover::ApplyStageValues(*Mover, Off, 0, 1.f), 0);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif

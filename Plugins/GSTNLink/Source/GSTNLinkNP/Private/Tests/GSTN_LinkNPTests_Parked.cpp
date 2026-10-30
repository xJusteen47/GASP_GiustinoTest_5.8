// Automation tests of the parked role (plan F10, m6 spec 7.3): the parked mode is entered and left by Combat.Mover.Linked, it neither moves
// nor falls whatever the input, and the step snapshot places a parked entity on its partner's BAKED socket (the grabbed on the grabber's
// hand; the latching initiator on a partner that walks and turns) from the partner's snapshot transform, the same bytes on every machine.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LinkTags.h"
#include "Movement/GSTN_LinkedModeTransition.h"
#include "Movement/GSTN_LinkedModifier.h"
#include "Movement/GSTN_LinkedMovementMode.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"

using namespace GSTNLinkNPTest;

namespace
{
	bool TransformsNear(const FTransform& A, const FTransform& B, double Tolerance = 0.01)
	{
		return A.GetLocation().Equals(B.GetLocation(), Tolerance) && A.GetRotation().Equals(B.GetRotation(), 1.e-4);
	}

	void SetModifier(FMoverSyncState& State, UMoverComponent* Mover, bool bLinked)
	{
		// A started modifier (sim time past zero): Mover ends a cancelled one only once it has started
		FMoverTimeStep Step;
		Step.BaseSimTimeMs = 1000.0;
		FMoverAuxStateContext Aux;
		if (bLinked)
		{
			State.MovementModifiers.QueueMovementModifier(MakeShared<FGSTN_LinkedModifier>());
		}
		else
		{
			State.MovementModifiers.CancelModifiersByTag(GSTNLinkTags::Mover_Linked, true);
		}
		State.MovementModifiers.GenerateActiveModifiers(Mover, Step, State, Aux);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkParkedModeByTagTest, "GSTNLink.NP.Parked.ModeEnteredByTag", Flags)
bool FGSTN_LinkParkedModeByTagTest::RunTest(const FString& Parameters)
{
	UMoverComponent* Mover = NewObject<UMoverComponent>(GetTransientPackage());
	FGCObjectScopeGuard MoverGuard(Mover);
	UGSTN_LinkedModeTransition* Entry = NewObject<UGSTN_LinkedModeTransition>(Mover);
	UGSTN_LinkedMovementMode* Mode = NewObject<UGSTN_LinkedMovementMode>(Mover);
	const UGSTN_LinkedModeTransition* Exit = Mode->Transitions.Num() == 1 ? Cast<UGSTN_LinkedModeTransition>(Mode->Transitions[0]) : nullptr;
	TestTrue(TEXT("the parked mode carries its own exit transition"), Exit && Exit->bExitTransition);
	if (!Exit)
	{
		return false;
	}

	FSimulationTickParams Params;
	FMoverSyncState& State = Params.StartState.SyncState;
	State.MovementMode = DefaultModeNames::Walking;
	TestTrue(TEXT("no tag: no entry"), Entry->Evaluate_Implementation(Params).NextMode.IsNone());

	// The modifier in Mover's state carries Combat.Mover.Linked: the next evaluation enters the parked mode
	SetModifier(State, Mover, true);
	TestTrue(TEXT("the modifier carries the tag in the state"), Mover->HasGameplayTagInState(State, GSTNLinkTags::Mover_Linked, true));
	TestEqual(TEXT("the tag enters the parked mode"), Entry->Evaluate_Implementation(Params).NextMode, UGSTN_LinkedModeTransition::GetDefaultLinkedModeName());

	State.MovementMode = UGSTN_LinkedModeTransition::GetDefaultLinkedModeName();
	TestTrue(TEXT("already parked: the entry does not re-enter"), Entry->Evaluate_Implementation(Params).NextMode.IsNone());
	TestTrue(TEXT("tag still present: the exit holds"), Exit->Evaluate_Implementation(Params).NextMode.IsNone());

	// Cancelled by tag (the end of the parked role): the exit leaves to Falling
	SetModifier(State, Mover, false);
	TestFalse(TEXT("the cancelled modifier is gone from the state"), Mover->HasGameplayTagInState(State, GSTNLinkTags::Mover_Linked, true));
	TestEqual(TEXT("no tag: the exit leaves to Falling"), Exit->Evaluate_Implementation(Params).NextMode, DefaultModeNames::Falling);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkParkedNoInputTest, "GSTNLink.NP.Parked.NoInputNoGravity", Flags)
bool FGSTN_LinkParkedNoInputTest::RunTest(const FString& Parameters)
{
	UMoverComponent* Mover = NewObject<UMoverComponent>(GetTransientPackage());
	FGCObjectScopeGuard MoverGuard(Mover);
	UGSTN_LinkedMovementMode* Mode = NewObject<UGSTN_LinkedMovementMode>(Mover);

	// A pawn that was falling fast and holds the stick: whatever it brings, the parked mode proposes nothing
	FMoverTickStartData Start;
	FMoverDefaultSyncState& StartSync = Start.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	const FVector Parked(120.0, -40.0, 300.0);
	const FRotator Facing(0.0, 75.0, 0.0);
	StartSync.SetTransforms_WorldSpace(Parked, Facing, FVector(400.0, 0.0, -900.0), FVector::ZeroVector);
	FMoverTimeStep Step;
	Step.StepMs = 16.f;
	FProposedMove Proposed;
	Proposed.LinearVelocity = FVector(1.0, 2.0, 3.0);
	Mode->GenerateMove_Implementation(FMoverSimContext(), Start, Step, Proposed);
	TestTrue(TEXT("no proposed velocity"), Proposed.LinearVelocity.IsZero());
	TestTrue(TEXT("no proposed rotation"), Proposed.AngularVelocityDegrees.IsZero());

	// Ten ticks: the capsule stays on its last real position with no velocity (no gravity, no input, no collision response needed)
	FSimulationTickParams Params;
	Params.StartState = Start;
	Params.TimeStep = Step;
	for (int32 Tick = 0; Tick < 10; ++Tick)
	{
		FMoverTickEndData End;
		Mode->SimulationTick_Implementation(Params, End);
		const FMoverDefaultSyncState* Out = End.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		if (!TestNotNull(TEXT("the tick writes the default sync state"), Out))
		{
			return false;
		}
		TestTrue(FString::Printf(TEXT("tick %d keeps the parked location"), Tick), Out->GetLocation_WorldSpace().Equals(Parked, 1.e-3));
		TestTrue(FString::Printf(TEXT("tick %d keeps the facing"), Tick), Out->GetOrientation_WorldSpace().Equals(Facing, 1.e-3));
		TestTrue(FString::Printf(TEXT("tick %d has no velocity"), Tick), Out->GetVelocity_WorldSpace().IsNearlyZero());
		Params.StartState.SyncState = End.SyncState;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkParkedHurtboxTest, "GSTNLink.NP.Parked.HurtboxFromPartnerSocket", Flags)
bool FGSTN_LinkParkedHurtboxTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	if (!TestTrue(TEXT("the test grab is registered"), Types.GrabIndex != 0))
	{
		return false;
	}
	// The grabber's baked right hand: 40 cm forward, 20 right, 50 up of its root, turned 90 degrees about up
	const FTransform Hand(FRotator(0.0, 90.0, 0.0), FVector(40.0, 20.0, 50.0));
	TMap<FName, FTransform> Sockets;
	Sockets.Add(HandSocket(), Hand);
	const uint16 GrabberBody = FGSTN_LinkAttach::AddForTest(Sockets);

	const FTransform GrabberRoot(FRotator(0.0, 30.0, 0.0), FVector(1000.0, 500.0, 90.0));
	const FTransform GrabbedCapsule(FRotator(0.0, 0.0, 0.0), FVector(1070.0, 520.0, 90.0));
	const FGSTN_LinkFragment GrabberLink = MakeLinked(Types.GrabIndex, 1, 6, 3, 100);
	const FGSTN_LinkFragment GrabbedLink = MakeLinked(Types.GrabIndex, 2, 5, 3, 100);
	TArray<FGSTN_SnapshotEntry> Entries;
	Entries.Add(MakeEntry(5, GrabberRoot, GrabberBody, &GrabberLink));
	Entries.Add(MakeEntry(6, GrabbedCapsule, 0, &GrabbedLink));
	TestTrue(TEXT("the grabbed entry is parked"), Entries[1].Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link)->IsParked());
	TestFalse(TEXT("the grabber entry is not"), Entries[0].Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link)->IsParked());

	FGSTN_LinkSnapshotContributor Contributor;
	Contributor.FinalizeSnapshot(100, Entries);
	// The anchor: the role's offset in the space of the partner's baked socket, in the partner's snapshot transform
	const FTransform Expected = Types.Grab->TargetRole.SocketOffset * Hand * GrabberRoot;
	TestTrue(TEXT("the grabbed hurtbox anchor sits on the grabber's baked hand"), TransformsNear(Entries[1].Transform, Expected));
	TestTrue(TEXT("the facing follows the anchor"), Entries[1].Facing.Equals(Expected.GetRotation().GetForwardVector(), 1.e-4));
	TestTrue(TEXT("the grabber is untouched"), TransformsNear(Entries[0].Transform, GrabberRoot));

	// Deterministic: another machine with the same snapshot bytes places it identically, bit for bit
	TArray<FGSTN_SnapshotEntry> Again;
	Again.Add(MakeEntry(5, GrabberRoot, GrabberBody, &GrabberLink));
	Again.Add(MakeEntry(6, GrabbedCapsule, 0, &GrabbedLink));
	Contributor.FinalizeSnapshot(100, Again);
	TestTrue(TEXT("identical bytes, identical anchor"), Again[1].Transform.GetLocation() == Entries[1].Transform.GetLocation());

	// Without the partner in the snapshot the parked entity keeps its own (capsule) transform
	TArray<FGSTN_SnapshotEntry> Alone;
	Alone.Add(MakeEntry(6, GrabbedCapsule, 0, &GrabbedLink));
	Contributor.FinalizeSnapshot(100, Alone);
	TestTrue(TEXT("no partner here: the capsule transform"), TransformsNear(Alone[0].Transform, GrabbedCapsule));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkParkedInitiatorFollowsTest, "GSTNLink.NP.Parked.InitiatorFollowsMovingPartner", Flags)
bool FGSTN_LinkParkedInitiatorFollowsTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	if (!TestTrue(TEXT("the test latch is registered"), Types.LatchIndex != 0))
	{
		return false;
	}
	// A big partner whose baked spine is 150 cm up its root
	const FTransform Spine(FVector(0.0, 0.0, 150.0));
	TMap<FName, FTransform> Sockets;
	Sockets.Add(SpineSocket(), Spine);
	const uint16 BigBody = FGSTN_LinkAttach::AddForTest(Sockets);

	// The latch parks the INITIATOR (role 1) with the same code as a grab parks the target: data, not a branch
	const FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, 9, 1, 50);
	const FGSTN_LinkFragment Latched = MakeLinked(Types.LatchIndex, 2, 8, 1, 50);
	const FTransform LatcherCapsule(FVector(0.0, 0.0, 90.0));
	FGSTN_LinkSnapshotContributor Contributor;

	// The partner walks, turns and jumps over three frames; the latcher's anchor follows every one of them, its capsule never moves
	const FTransform Path[] = {
		FTransform(FRotator(0.0, 0.0, 0.0), FVector(0.0, 0.0, 90.0)),
		FTransform(FRotator(0.0, 45.0, 0.0), FVector(120.0, 30.0, 90.0)),
		FTransform(FRotator(0.0, 170.0, 0.0), FVector(260.0, 100.0, 210.0)) };
	for (int32 Frame = 0; Frame < UE_ARRAY_COUNT(Path); ++Frame)
	{
		TArray<FGSTN_SnapshotEntry> Entries;
		Entries.Add(MakeEntry(8, LatcherCapsule, 0, &Latcher));
		Entries.Add(MakeEntry(9, Path[Frame], BigBody, &Latched));
		TestTrue(TEXT("the latcher is parked"), Entries[0].Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link)->IsParked());
		TestFalse(TEXT("the latched partner is not"), Entries[1].Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link)->IsParked());
		Contributor.FinalizeSnapshot(50 + Frame, Entries);
		const FTransform Expected = Types.Latch->InitiatorRole.SocketOffset * Spine * Path[Frame];
		TestTrue(FString::Printf(TEXT("frame %d: the latcher's anchor is on the partner's spine"), Frame), TransformsNear(Entries[0].Transform, Expected));
	}
	return true;
}

#endif

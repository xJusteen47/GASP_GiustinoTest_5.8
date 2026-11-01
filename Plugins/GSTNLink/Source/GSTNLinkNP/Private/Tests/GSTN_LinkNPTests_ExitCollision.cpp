// Links, Network Prediction half: automation tests of the exit collision window (M6 F15, HANDOVER 61 OPEN 2) and of the exit query that
// ignores only the pair (HANDOVER 58 OPEN 2). The window is a function of the link fragment, so the tests run it on fragments through the
// real break paths (the watchdog's command through FGSTN_LinkInboxHandlers::ApplyCommand, the throw's own end) and copy fragments to stand
// for a rollback.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LinkWatchdog.h"
#include "Sim/GSTN_LinkExitCollision.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkThrow.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	using namespace GSTNLinkNPTest;

	constexpr int32 GrabberId = 3;
	constexpr int32 GrabbedId = 7;
	constexpr int32 BystanderId = 9;
	constexpr uint8 Serial = 5;
	constexpr int32 LinkFrame = 100;
	constexpr int32 BreakFrame = 140;

	FGSTN_LinkWatchdogEntity MakeSide(int32 Id, const FGSTN_LinkFragment& Fragment, const FVector& Location, int32 Frame)
	{
		FGSTN_LinkWatchdogEntity Entity;
		Entity.EntityId = Id;
		Entity.Frame = Frame;
		Entity.Links = Fragment.Links;
		Entity.Transform = FTransform(Location);
		return Entity;
	}

	FGSTN_LinkWatchdogLogic::FParams MakeParams()
	{
		FGSTN_LinkWatchdogLogic::FParams Params;
		Params.OrphanGraceFrames = 3;
		Params.Definitions = [](uint8 Index) { return UGSTN_LinkRegistry::Get()->GetDefinition(Index); };
		return Params;
	}

	/** A released test grab: both fragments after the watchdog's Release command, applied on BreakFrame + 1 */
	struct FReleasedPair
	{
		FGSTN_LinkFragment Grabber;
		FGSTN_LinkFragment Grabbed;
		bool bApplied = false;

		explicit FReleasedPair(uint8 TypeIndex)
			: Grabber(MakeLinked(TypeIndex, 1, GrabbedId, Serial, LinkFrame + 1))
			, Grabbed(MakeLinked(TypeIndex, 2, GrabberId, Serial, LinkFrame))
		{
			FGSTN_LinkTickExtension::MarkPendingBreak(Grabber, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
			const TArray<FGSTN_LinkWatchdogEntity> Entities = { MakeSide(GrabberId, Grabber, FVector::ZeroVector, BreakFrame),
				MakeSide(GrabbedId, Grabbed, FVector(30.0, 0.0, 0.0), BreakFrame) };
			TArray<FGSTN_LinkWatchdogPost> Posts;
			TArray<FGSTN_LinkWatchdogBreak> Breaks;
			FGSTN_LinkWatchdogLogic::Evaluate(Entities, BreakFrame, MakeParams(), Posts, Breaks);
			bApplied = Posts.Num() == 2;
			for (const FGSTN_LinkWatchdogPost& Post : Posts)
			{
				bApplied &= FGSTN_LinkInboxHandlers::ApplyCommand(Post.TargetId == GrabberId ? Grabber : Grabbed, Post.Command, Post.StampFrame);
			}
		}
	};

	FGSTN_EntityRef Fighter(int32 Id)
	{
		return FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(Id));
	}

	/** The fragment's bytes, as the wire carries them */
	TArray<uint8> Bytes(FGSTN_LinkFragment& Fragment)
	{
		TArray<uint8> Out;
		FMemoryWriter Writer(Out);
		Fragment.NetSerialize(Writer);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPExitCollisionWhileLinkedTest, "GSTNLink.NP.ExitCollision.IgnoredWhileLinked", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPExitCollisionWhileLinkedTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const FGSTN_LinkFragment Grabber = MakeLinked(Types.GrabIndex, 1, GrabbedId, Serial, LinkFrame + 1);
	const FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, GrabberId, Serial, LinkFrame);
	TestTrue(TEXT("the test grab ignores partner collision (parked role, bCollisionWithPartner false)"), Types.Grab->IgnoresPartnerCollision());
	TestTrue(TEXT("the grabber ignores the grabbed"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Grabber, Registry, LinkFrame + 5) == Fighter(GrabbedId));
	TestTrue(TEXT("the grabbed ignores the grabber"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Grabbed, Registry, LinkFrame + 5) == Fighter(GrabberId));
	TestTrue(TEXT("no fragment, nobody"), FGSTN_LinkExitCollision::GetIgnoredPartner(nullptr, Registry, LinkFrame).IsNull());

	// A type whose parked role keeps the collision ignores nobody, linked or after the end
	Types.Grab->TargetRole.bCollisionWithPartner = true;
	TestFalse(TEXT("a colliding parked role"), Types.Grab->IgnoresPartnerCollision());
	TestTrue(TEXT("collision kept while linked"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Grabber, Registry, LinkFrame + 5).IsNull());
	const FReleasedPair Released(Types.GrabIndex);
	TestTrue(TEXT("released"), Released.bApplied);
	TestEqual(TEXT("no window for it"), Released.Grabbed.ExitIgnoreEndFrame, 0);
	Types.Grab->TargetRole.bCollisionWithPartner = false;

	// A predicted link (Pending, no role yet) does not ignore anybody: the response forms it
	FGSTN_LinkFragment Predicted;
	FGSTN_LinkTickExtension::MarkPredicted(Predicted, *Registry, TestGrab, GrabbedId, LinkFrame);
	TestTrue(TEXT("a pending request ignores nobody"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Predicted, Registry, LinkFrame).IsNull());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPExitCollisionWindowExpiryTest, "GSTNLink.NP.ExitCollision.WindowExpiry", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPExitCollisionWindowExpiryTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const int32 Window = Types.Grab->ExitCollisionIgnoreFrames;
	TestEqual(TEXT("default window 12 frames"), Window, 12);

	FReleasedPair Pair(Types.GrabIndex);
	if (!TestTrue(TEXT("the release applied on both sides"), Pair.bApplied))
	{
		return false;
	}
	const int32 Exit = BreakFrame + 1;
	TestEqual(TEXT("the window ends exit + 12 on the grabber"), Pair.Grabber.ExitIgnoreEndFrame, Exit + Window);
	TestEqual(TEXT("the same frame on the grabbed"), Pair.Grabbed.ExitIgnoreEndFrame, Exit + Window);
	TestTrue(TEXT("the grabber's former partner"), Pair.Grabber.ExitIgnorePartner == Fighter(GrabbedId));
	TestTrue(TEXT("the grabbed's former partner"), Pair.Grabbed.ExitIgnorePartner == Fighter(GrabberId));

	// Ignored from the exit frame through the window's last frame, on both sides alike; collision back on exit + 12
	for (int32 Frame = Exit; Frame < Exit + Window; ++Frame)
	{
		if (FGSTN_LinkExitCollision::GetIgnoredPartner(&Pair.Grabber, Registry, Frame) != Fighter(GrabbedId)
			|| FGSTN_LinkExitCollision::GetIgnoredPartner(&Pair.Grabbed, Registry, Frame) != Fighter(GrabberId))
		{
			AddError(FString::Printf(TEXT("not ignored on frame %d of the window"), Frame - Exit));
		}
	}
	TestTrue(TEXT("collision back at the end (grabber)"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Pair.Grabber, Registry, Exit + Window).IsNull());
	TestTrue(TEXT("collision back at the end (grabbed)"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Pair.Grabbed, Registry, Exit + Window).IsNull());

	// The hygiene drops the window once it has run out, so the fragment goes back to its idle bytes
	TestFalse(TEXT("not dropped inside the window"), FGSTN_LinkExitCollision::DropExpired(Pair.Grabbed, Exit + Window - 1));
	TestTrue(TEXT("dropped at its end"), FGSTN_LinkExitCollision::DropExpired(Pair.Grabbed, Exit + Window));
	TestEqual(TEXT("no end frame left"), Pair.Grabbed.ExitIgnoreEndFrame, 0);
	TestTrue(TEXT("no partner left"), Pair.Grabbed.ExitIgnorePartner.IsNull());
	TestFalse(TEXT("dropping twice changes nothing"), FGSTN_LinkExitCollision::DropExpired(Pair.Grabbed, Exit + Window + 1));

	// A zero window: collision returns on the exit frame
	Types.Grab->ExitCollisionIgnoreFrames = 0;
	const FReleasedPair Immediate(Types.GrabIndex);
	TestEqual(TEXT("no window with 0 frames"), Immediate.Grabber.ExitIgnoreEndFrame, 0);
	TestTrue(TEXT("collision on the exit frame"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Immediate.Grabber, Registry, Exit).IsNull());
	Types.Grab->ExitCollisionIgnoreFrames = Window;

	// The throw ends the link in the simulation on both sides: the thrower at its key frame F opens the window from F + 1, the thrown side's
	// exit frame
	FGSTN_LinkFragment Thrower = MakeLinked(Types.GrabIndex, 1, GrabbedId, Serial, LinkFrame + 1);
	TestTrue(TEXT("the thrower ends its link"), FGSTN_LinkThrow::EndOwnLink(Thrower, BreakFrame, nullptr));
	TestEqual(TEXT("the thrower's window from F + 1"), Thrower.ExitIgnoreEndFrame, BreakFrame + 1 + Window);
	TestTrue(TEXT("the thrower ignores the thrown"), FGSTN_LinkExitCollision::GetIgnoredPartner(&Thrower, Registry, BreakFrame + 1) == Fighter(GrabbedId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPExitCollisionRollbackTest, "GSTNLink.NP.ExitCollision.RollbackAcrossWindow", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPExitCollisionRollbackTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const int32 Window = Types.Grab->ExitCollisionIgnoreFrames;
	const int32 Exit = BreakFrame + 1;

	// The predicted side ran past the window with the fragment dropped by the hygiene; a rollback restores the state of a frame inside it
	FReleasedPair Pair(Types.GrabIndex);
	const FGSTN_LinkFragment Restored = Pair.Grabber;
	FGSTN_LinkFragment Future = Pair.Grabber;
	FGSTN_LinkExitCollision::DropExpired(Future, Exit + Window + 3);
	TestTrue(TEXT("the future dropped the window"), Future.ExitIgnoreEndFrame == 0);

	// Re-simulating from the restored frame decides every frame exactly as the first pass did: the decision is the fragment's, not a field
	// left on a component by the future
	for (int32 Frame = Exit; Frame <= Exit + Window + 2; ++Frame)
	{
		FGSTN_LinkFragment Resim = Restored;
		const FGSTN_EntityRef First = FGSTN_LinkExitCollision::GetIgnoredPartner(&Pair.Grabber, Registry, Frame);
		FGSTN_LinkExitCollision::DropExpired(Resim, Frame);
		const FGSTN_EntityRef Again = FGSTN_LinkExitCollision::GetIgnoredPartner(&Resim, Registry, Frame);
		if (First != Again)
		{
			AddError(FString::Printf(TEXT("frame %d decides differently after the rollback"), Frame));
		}
	}

	// The window is on the wire: a client that receives the authority's fragment of a frame inside the window gets the same window back
	FGSTN_LinkFragment Sent = Pair.Grabbed;
	const TArray<uint8> Wire = Bytes(Sent);
	FGSTN_LinkFragment Received;
	FMemoryReader Reader(Wire);
	Received.NetSerialize(Reader);
	TestFalse(TEXT("reads back"), Reader.IsError());
	TestTrue(TEXT("identical after the round trip"), Received == Pair.Grabbed);
	TestFalse(TEXT("no reconcile against itself"), Received.ShouldReconcile(Pair.Grabbed));

	// A client fragment without the window against the authority's with it reconciles: the window is compared like the rest of the state
	FGSTN_LinkFragment Missing = Pair.Grabbed;
	Missing.ExitIgnorePartner = FGSTN_EntityRef::Null();
	Missing.ExitIgnoreEndFrame = 0;
	TestTrue(TEXT("a missing window reconciles"), Missing.ShouldReconcile(Pair.Grabbed));

	// Once dropped, the fragment is back to its bytes without the window (an idle entity stays idle on the wire)
	FGSTN_LinkFragment Dropped = Pair.Grabbed;
	FGSTN_LinkExitCollision::DropExpired(Dropped, Exit + Window);
	FGSTN_LinkFragment NoWindow = Pair.Grabbed;
	NoWindow.ExitIgnorePartner = FGSTN_EntityRef::Null();
	NoWindow.ExitIgnoreEndFrame = 0;
	TestEqual(TEXT("the window costs 4 + 4 bytes while open"), Wire.Num() - Bytes(NoWindow).Num(), 8);
	TestTrue(TEXT("dropped equals never opened"), Dropped == NoWindow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPExitQueryIgnoresOnlyPairTest, "GSTNLink.NP.Exit.QueryIgnoresOnlyThePair", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPExitQueryIgnoresOnlyPairTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FGSTN_LinkFragment Grabber = MakeLinked(Types.GrabIndex, 1, GrabbedId, Serial, LinkFrame + 1);
	const FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, GrabberId, Serial, LinkFrame);
	const FGSTN_LinkFragment Bystander;
	FGSTN_LinkTickExtension::MarkPendingBreak(Grabber, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	const TArray<FGSTN_LinkWatchdogEntity> Entities = { MakeSide(GrabberId, Grabber, FVector::ZeroVector, BreakFrame),
		MakeSide(GrabbedId, Grabbed, FVector(30.0, 0.0, 0.0), BreakFrame), MakeSide(BystanderId, Bystander, FVector(500.0, 0.0, 0.0), BreakFrame) };

	// The pair's query is asked for the exiting and the staying entity, never with the bystander; a world that holds only the bystander's
	// capsule straight ahead of the grabber pushes the ring to its next turn
	struct FBystanderAhead : IGSTN_ExitCollisionQuery
	{
		virtual bool IsCapsuleFree(const FVector& Location, const FQuat&, float, float) const override { return !(Location.X > 60.0 && FMath::Abs(Location.Y) < 60.0); }
	};
	const FBystanderAhead World;
	TArray<TPair<int32, int32>> Asked;
	FGSTN_LinkWatchdogLogic::FParams Params = MakeParams();
	Params.WithPairCollision = [&](int32 ExitingId, int32 StayingId, TFunctionRef<void(const IGSTN_ExitCollisionQuery*)> Use)
	{
		Asked.Add({ ExitingId, StayingId });
		Use(&World);
	};
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, BreakFrame, Params, Posts, Breaks);
	if (!TestEqual(TEXT("one pair query"), Asked.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("for the exiting grabbed"), Asked[0].Key, GrabbedId);
	TestEqual(TEXT("around the staying grabber"), Asked[0].Value, GrabberId);
	const FGSTN_LinkWatchdogPost* ToGrabbed = Posts.FindByPredicate([](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == GrabbedId; });
	if (!TestNotNull(TEXT("the grabbed is posted to"), ToGrabbed))
	{
		return false;
	}
	const FVector ExitAt(ToGrabbed->Command.ExitPosition);
	TestTrue(TEXT("the exit is not where the bystander stands"), World.IsCapsuleFree(ExitAt, FQuat::Identity, 34.f, 88.f));
	TestTrue(TEXT("still a valid place"), (ToGrabbed->Command.ExitFlags & GSTNLinkExitFlags::Fallback) == 0);
	return true;
}

#endif

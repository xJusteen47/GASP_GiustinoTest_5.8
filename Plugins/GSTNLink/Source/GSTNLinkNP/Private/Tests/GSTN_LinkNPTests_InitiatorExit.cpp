// Links, Network Prediction half: automation tests of the initiator-placed exit and the per-link anchor (M7 G6). The exit is placed by the
// link tick's pure step, copied by the watchdog's pure evaluation and applied through the real inbox rule, so "the initiator's own break equals
// the command's" is checked on the fragments themselves.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LinkWatchdog.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "State/GSTN_LinkMath.h"

// One namespace per test file: the unity build merges the files of this module, so their helpers must not share a scope
namespace GSTNLinkNPTests_InitiatorExit
{

namespace
{
	using namespace GSTNLinkNPTest;

	constexpr int32 LatcherId = 4;
	constexpr int32 LatchedId = 9;
	constexpr uint8 Serial = 12;
	constexpr int32 LinkFrame = 200;
	constexpr int32 F = 260;

	/** The test latch with the initiator-placed exit on for the scope (the latcher is the parked initiator) */
	struct FScopedPlacingLatch : FScopedLinkTypes
	{
		FScopedPlacingLatch() { Latch->bInitiatorPlacesExit = true; }
	};

	FGSTN_LinkWatchdogEntity MakeSide(int32 Id, const FGSTN_LinkFragment& Fragment, const FVector& Location)
	{
		FGSTN_LinkWatchdogEntity Entity;
		Entity.EntityId = Id;
		Entity.Frame = F;
		Entity.Links = Fragment.Links;
		Entity.Exit = Fragment.Actions.Exit;
		Entity.Transform = FTransform(Location);
		return Entity;
	}

	FGSTN_LinkWatchdogLogic::FParams MakeParams(const IGSTN_ExitCollisionQuery* Collision = nullptr)
	{
		FGSTN_LinkWatchdogLogic::FParams Params;
		Params.OrphanGraceFrames = 3;
		Params.Collision = Collision;
		Params.Definitions = [](uint8 Index) { return UGSTN_LinkRegistry::Get()->GetDefinition(Index); };
		return Params;
	}

	const FGSTN_LinkWatchdogPost* PostFor(const TArray<FGSTN_LinkWatchdogPost>& Posts, int32 TargetId)
	{
		return Posts.FindByPredicate([TargetId](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == TargetId; });
	}

	struct FAllBlocked : IGSTN_ExitCollisionQuery
	{
		virtual bool IsCapsuleFree(const FVector&, const FQuat&, float, float) const override { return false; }
	};

	/** A released latch: the latcher marked its pending break and placed its exit around the latched as its view shows it, at (200, 0, 0) */
	struct FReleased
	{
		FGSTN_LinkFragment Latcher;
		FGSTN_LinkFragment Latched;
		FVector LatchedAt = FVector(200.0, 0.0, 0.0);
		bool bPlaced = false;

		explicit FReleased(uint8 TypeIndex)
			: Latcher(MakeLinked(TypeIndex, 1, LatchedId, Serial, LinkFrame + 1))
			, Latched(MakeLinked(TypeIndex, 2, LatcherId, Serial, LinkFrame))
		{
			FGSTN_LinkTickExtension::MarkPendingBreak(Latcher, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
			const FGSTN_SnapshotEntry View = MakeEntry(LatchedId, FTransform(LatchedAt), 0, &Latched);
			bPlaced = FGSTN_LinkTickExtension::StepInitiatorExit(Latcher, F, UGSTN_LinkRegistry::Get(), &View, FGSTN_LinkTickExtension::FExitSelf());
		}

		TArray<FGSTN_LinkWatchdogEntity> Entities() const
		{
			// The latcher's capsule where it rides: the definition's offset on the latched (no socket content: the root)
			return { MakeSide(LatcherId, Latcher, LatchedAt + FVector(0.0, -40.0, 20.0)), MakeSide(LatchedId, Latched, LatchedAt) };
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPInitiatorExitPlacedFromViewTest, "GSTNLink.NP.InitiatorExit.PlacedFromView", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPInitiatorExitPlacedFromViewTest::RunTest(const FString& Parameters)
{
	FScopedPlacingLatch Types;
	FReleased Pair(Types.LatchIndex);
	if (!TestTrue(TEXT("the released parked initiator places its exit"), Pair.bPlaced))
	{
		return false;
	}
	const FGSTN_LinkActions::FExit& Exit = Pair.Latcher.Actions.Exit;
	TestEqual(TEXT("due the frame after the release"), Exit.Frame, F + 1);
	TestEqual(TEXT("teleport, placed by the initiator"), Exit.Flags, static_cast<uint8>(GSTNLinkExitFlags::Teleport | GSTNLinkExitFlags::InitiatorPlaced));

	// The same placement the watchdog makes around the same partner, without collision
	FGSTN_LinkWatchdogEntity Exiting;
	FGSTN_LinkWatchdogEntity Staying;
	Staying.Transform = FTransform(Pair.LatchedAt);
	const FGSTN_ExitResult Expected = FGSTN_LinkWatchdogLogic::PlaceExit(*Types.Latch, Exiting, Staying, nullptr);
	TestTrue(TEXT("the watchdog's placer, quantised the same"), Exit.Position.Equals(Expected.Transform.GetLocation(), 0.0));

	TestFalse(TEXT("placed once: a second call keeps the record"), FGSTN_LinkTickExtension::StepInitiatorExit(Pair.Latcher, F + 1, UGSTN_LinkRegistry::Get(),
		nullptr, FGSTN_LinkTickExtension::FExitSelf()));
	TestFalse(TEXT("nothing to break before the exit's frame"), FGSTN_LinkTickExtension::StepOwnExitBreak(Pair.Latcher, F, UGSTN_LinkRegistry::Get()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPInitiatorExitOwnBreakEqualsCommandTest, "GSTNLink.NP.InitiatorExit.OwnBreakEqualsCommand", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPInitiatorExitOwnBreakEqualsCommandTest::RunTest(const FString& Parameters)
{
	FScopedPlacingLatch Types;
	FReleased Pair(Types.LatchIndex);

	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	FGSTN_LinkWatchdogLogic::Evaluate(Pair.Entities(), F, MakeParams(), Posts, Breaks);
	const FGSTN_LinkWatchdogPost* ToLatcher = PostFor(Posts, LatcherId);
	const FGSTN_LinkWatchdogPost* ToLatched = PostFor(Posts, LatchedId);
	if (!TestTrue(TEXT("both sides are posted to"), ToLatcher && ToLatched))
	{
		return false;
	}
	const FGSTN_LinkActions::FExit& Own = Pair.Latcher.Actions.Exit;
	TestTrue(TEXT("the command carries the initiator's own exit"), FVector(ToLatcher->Command.ExitPosition) == Own.Position);
	TestEqual(TEXT("and its flags"), ToLatcher->Command.ExitFlags, Own.Flags);
	TestEqual(TEXT("the pending reason"), ToLatcher->Command.Reason, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	TestEqual(TEXT("the staying side gets no exit"), ToLatched->Command.ExitFlags, static_cast<uint8>(0));

	// The authority applies the command (drained before the link tick); the initiator's machine ends the link itself: the same fragment
	FGSTN_LinkFragment ByCommand = Pair.Latcher;
	TestTrue(TEXT("the command breaks the link"), FGSTN_LinkInboxHandlers::ApplyCommand(ByCommand, ToLatcher->Command, F));
	TestFalse(TEXT("after the command the own break does nothing"), FGSTN_LinkTickExtension::StepOwnExitBreak(ByCommand, F + 1, UGSTN_LinkRegistry::Get()));
	FGSTN_LinkFragment ByItself = Pair.Latcher;
	TestTrue(TEXT("the initiator ends the link on the exit's frame"), FGSTN_LinkTickExtension::StepOwnExitBreak(ByItself, F + 1, UGSTN_LinkRegistry::Get()));
	TestTrue(TEXT("both paths leave the same fragment"), ByCommand == ByItself);
	TestTrue(TEXT("the link is idle"), ByItself.Links.Body.Phase == EGSTN_LinkPhase::None);
	TestFalse(TEXT("nothing to reconcile"), ByItself.ShouldReconcile(ByCommand));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPInitiatorExitBlockedFallsBackTest, "GSTNLink.NP.InitiatorExit.BlockedPlaceFallsBack", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPInitiatorExitBlockedFallsBackTest::RunTest(const FString& Parameters)
{
	FScopedPlacingLatch Types;
	FReleased Pair(Types.LatchIndex);
	const FAllBlocked World;
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	FGSTN_LinkWatchdogLogic::Evaluate(Pair.Entities(), F, MakeParams(&World), Posts, Breaks);
	const FGSTN_LinkWatchdogPost* ToLatcher = PostFor(Posts, LatcherId);
	if (!TestNotNull(TEXT("the latcher is posted to"), ToLatcher))
	{
		return false;
	}
	TestTrue(TEXT("an overlapping own exit is not copied"), (ToLatcher->Command.ExitFlags & GSTNLinkExitFlags::InitiatorPlaced) == 0);
	TestTrue(TEXT("the watchdog's own placement, reported as a fallback"), (ToLatcher->Command.ExitFlags & GSTNLinkExitFlags::Fallback) != 0);

	FGSTN_LinkFragment Authority = Pair.Latcher;
	FGSTN_LinkInboxHandlers::ApplyCommand(Authority, ToLatcher->Command, F);
	FGSTN_LinkFragment Predicted = Pair.Latcher;
	FGSTN_LinkTickExtension::StepOwnExitBreak(Predicted, F + 1, UGSTN_LinkRegistry::Get());
	TestTrue(TEXT("the predicted exit is corrected"), Predicted.ShouldReconcile(Authority));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPInitiatorExitOnlyWhenAskedTest, "GSTNLink.NP.InitiatorExit.OnlyWhenAsked", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPInitiatorExitOnlyWhenAskedTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FReleased Latch(Types.LatchIndex);
	TestFalse(TEXT("off by default: the exit stays server-given"), Latch.bPlaced);

	// A grab parks its target, not its initiator: the flag does nothing there
	Types.Grab->bInitiatorPlacesExit = true;
	FGSTN_LinkFragment Grabber = MakeLinked(Types.GrabIndex, 1, LatchedId, Serial, LinkFrame);
	FGSTN_LinkTickExtension::MarkPendingBreak(Grabber, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	const FGSTN_SnapshotEntry View = MakeEntry(LatchedId, FTransform::Identity, 0, nullptr);
	TestFalse(TEXT("a grabber places nothing"), FGSTN_LinkTickExtension::StepInitiatorExit(Grabber, F, UGSTN_LinkRegistry::Get(), &View, FGSTN_LinkTickExtension::FExitSelf()));

	// No pending break, or no partner in the view: nothing either
	Types.Latch->bInitiatorPlacesExit = true;
	FGSTN_LinkFragment Riding = MakeLinked(Types.LatchIndex, 1, LatchedId, Serial, LinkFrame);
	TestFalse(TEXT("no release, no exit"), FGSTN_LinkTickExtension::StepInitiatorExit(Riding, F, UGSTN_LinkRegistry::Get(), &View, FGSTN_LinkTickExtension::FExitSelf()));
	FGSTN_LinkTickExtension::MarkPendingBreak(Riding, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	TestFalse(TEXT("no partner in the view, no exit"), FGSTN_LinkTickExtension::StepInitiatorExit(Riding, F, UGSTN_LinkRegistry::Get(), nullptr, FGSTN_LinkTickExtension::FExitSelf()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPInitiatorExitMissedFrameTest, "GSTNLink.NP.InitiatorExit.MissedFrameBreaksPartner", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPInitiatorExitMissedFrameTest::RunTest(const FString& Parameters)
{
	FScopedPlacingLatch Types;
	FReleased Pair(Types.LatchIndex);
	// The watchdog did not see frame F: at F + 1 the latcher has ended the link by itself, the latched still holds it
	FGSTN_LinkTickExtension::StepOwnExitBreak(Pair.Latcher, F + 1, UGSTN_LinkRegistry::Get());
	FGSTN_LinkWatchdogLogic::FParams Params = MakeParams();
	// Well inside the orphan grace, so only the new rule can break it
	Params.OrphanGraceFrames = 1000;
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	FGSTN_LinkWatchdogLogic::Evaluate(Pair.Entities(), F + 1, Params, Posts, Breaks);
	TestEqual(TEXT("one break"), Breaks.Num(), 1);
	TestEqual(TEXT("one command, to the side that stays"), Posts.Num(), 1);
	const FGSTN_LinkWatchdogPost* ToLatched = PostFor(Posts, LatchedId);
	if (!TestNotNull(TEXT("the latched is posted to"), ToLatched))
	{
		return false;
	}
	TestEqual(TEXT("with the initiator's reason"), ToLatched->Command.Reason, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	TestFalse(TEXT("not an orphan"), Breaks.Num() == 1 && Breaks[0].bOrphan);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPInitiatorExitReconcileToleranceTest, "GSTNLink.NP.InitiatorExit.ReconcileTolerance", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPInitiatorExitReconcileToleranceTest::RunTest(const FString& Parameters)
{
	FScopedPlacingLatch Types;
	FReleased Pair(Types.LatchIndex);
	FGSTN_LinkFragment Authority = Pair.Latcher;
	Authority.Actions.Exit.Position += FVector(2.0, -1.0, 0.0);
	Authority.Actions.Exit.YawDeg += 2.f;
	TestFalse(TEXT("an exit a few cm off (the rewound view) is the same exit"), Pair.Latcher.ShouldReconcile(Authority));
	Authority.Actions.Exit.Position += FVector(30.0, 0.0, 0.0);
	TestTrue(TEXT("one well off is a misprediction"), Pair.Latcher.ShouldReconcile(Authority));

	// The tolerance holds for initiator-placed exits only: a watchdog exit is exact
	FGSTN_LinkFragment Watchdog = Pair.Latcher;
	Watchdog.Actions.Exit.Flags = GSTNLinkExitFlags::Teleport;
	FGSTN_LinkFragment WatchdogAuthority = Watchdog;
	WatchdogAuthority.Actions.Exit.Position += FVector(1.0, 0.0, 0.0);
	TestTrue(TEXT("a server-given exit is compared exactly"), Watchdog.ShouldReconcile(WatchdogAuthority));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPAnchorOverrideTest, "GSTNLink.NP.Anchor.Override", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPAnchorOverrideTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, LatchedId, Serial, LinkFrame);
	const int32 BytesWithout = PayloadBytes(Latcher);
	Latcher.Links.Body.SetAnchorOverride(FVector(12.4, -3.6, 150.0));
	FVector Anchor;
	TestTrue(TEXT("the flag is set"), Latcher.Links.Body.GetAnchorOverride(Anchor));
	TestTrue(TEXT("whole centimetres"), Anchor == FVector(12.0, -4.0, 150.0));
	TestEqual(TEXT("three int16 more on the wire, only with the flag"), PayloadBytes(Latcher), BytesWithout + 6);

	FGSTN_SyncFragmentArray List = ListOf(Latcher);
	bool bError = false;
	FGSTN_SyncFragmentArray Loaded = ReadBack(List, bError);
	const FGSTN_LinkFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Loaded);
	TestFalse(TEXT("reads back cleanly"), bError);
	TestTrue(TEXT("equal after the round trip"), Read && *Read == Latcher);

	// The snapshot substitution parks the latcher at its own anchor on the latched (no socket content: the root), the role's rotation kept
	const FVector LatchedAt(500.0, 0.0, 0.0);
	FGSTN_LinkFragment Latched = MakeLinked(Types.LatchIndex, 2, LatcherId, Serial, LinkFrame);
	const FGSTN_SnapshotEntry Partner = MakeEntry(LatchedId, FTransform(LatchedAt), 0, &Latched);
	const FGSTN_SnapshotEntry Self = MakeEntry(LatcherId, FTransform::Identity, 0, &Latcher);
	const FGSTN_LinkSnapshotExt* Ext = Self.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
	FTransform Parked;
	TestTrue(TEXT("parked"), Ext && FGSTN_LinkSnapshotContributor::ComputeParkedTransform(*Ext, Partner, Parked));
	TestTrue(TEXT("at the anchor, not the definition's offset"), Parked.GetLocation().Equals(LatchedAt + FVector(12.0, -4.0, 150.0), 0.01));

	// The watchdog's leash measures from the anchor too
	FGSTN_LinkWatchdogEntity Initiator;
	Initiator.Links = Latcher.Links;
	Initiator.Transform = FTransform(LatchedAt + FVector(12.0, -4.0, 150.0));
	FGSTN_LinkWatchdogEntity Target;
	Target.Transform = FTransform(LatchedAt);
	TestTrue(TEXT("no distance at the anchor"), FGSTN_LinkWatchdogLogic::ParkedDistance(*Types.Latch, Initiator, Target) < 0.01f);

	// Two machines' anchors of one hit, a centimetre apart, are the same anchor; a different one is not
	FGSTN_LinkFragment Authority = Latcher;
	Authority.Links.Body.SetAnchorOverride(FVector(13.0, -4.0, 150.0));
	TestFalse(TEXT("within the tolerance"), Latcher.ShouldReconcile(Authority));
	Authority.Links.Body.SetAnchorOverride(FVector(60.0, -4.0, 150.0));
	TestTrue(TEXT("outside it"), Latcher.ShouldReconcile(Authority));
	Latcher.Links.Body.ClearAnchorOverride();
	TestEqual(TEXT("cleared: the old bytes"), PayloadBytes(Latcher), BytesWithout);
	return true;
}

} // namespace GSTNLinkNPTests_InitiatorExit

#endif

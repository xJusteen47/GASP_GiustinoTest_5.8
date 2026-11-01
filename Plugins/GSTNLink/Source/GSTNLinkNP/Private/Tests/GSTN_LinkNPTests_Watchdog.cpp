// Links, Network Prediction half: automation tests of the watchdog, the break rules as the watchdog runs them, LinkBreakOnHit and the exit (F11).
// The watchdog's rules are a pure function over entity views; the commands it posts are applied through the real inbox rule
// (FGSTN_LinkInboxHandlers::ApplyCommand), so "both sides clear at F+1" is checked on the fragments themselves.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LinkTags.h"
#include "GSTN_LinkWatchdog.h"
#include "Movement/GSTN_LinkExitRequest.h"
#include "Pipeline/GSTN_HitStep_LinkBreakOnHit.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkTickExtension.h"

namespace
{
	using namespace GSTNLinkNPTest;

	constexpr int32 GrabberId = 3;
	constexpr int32 GrabbedId = 7;
	constexpr uint8 Serial = 5;
	constexpr int32 LinkFrame = 100;

	FGSTN_LinkWatchdogEntity MakeSide(int32 Id, const FGSTN_LinkFragment& Fragment, const FVector& Location, int32 Frame)
	{
		FGSTN_LinkWatchdogEntity Entity;
		Entity.EntityId = Id;
		Entity.Frame = Frame;
		Entity.Links = Fragment.Links;
		Entity.Transform = FTransform(Location);
		return Entity;
	}

	/** A live test grab between GrabberId and GrabbedId, the grabbed's capsule where the link expects it (30 cm in front of the grabber) */
	struct FPair
	{
		FGSTN_LinkFragment Grabber;
		FGSTN_LinkFragment Grabbed;
		FVector GrabberAt = FVector::ZeroVector;
		FVector GrabbedAt = FVector(30.0, 0.0, 0.0);
		bool bGrabberDead = false;
		bool bGrabbedDead = false;
		FGameplayTagContainer GrabberTags;

		explicit FPair(uint8 TypeIndex)
			: Grabber(MakeLinked(TypeIndex, 1, GrabbedId, Serial, LinkFrame + 1))
			, Grabbed(MakeLinked(TypeIndex, 2, GrabberId, Serial, LinkFrame))
		{
		}

		TArray<FGSTN_LinkWatchdogEntity> Entities(int32 Frame) const
		{
			FGSTN_LinkWatchdogEntity A = MakeSide(GrabberId, Grabber, GrabberAt, Frame);
			A.bDead = bGrabberDead;
			A.StateTags = GrabberTags;
			FGSTN_LinkWatchdogEntity B = MakeSide(GrabbedId, Grabbed, GrabbedAt, Frame);
			B.bDead = bGrabbedDead;
			return { A, B };
		}
	};

	FGSTN_LinkWatchdogLogic::FParams MakeParams(const IGSTN_ExitCollisionQuery* Collision = nullptr)
	{
		FGSTN_LinkWatchdogLogic::FParams Params;
		Params.OrphanGraceFrames = 3;
		Params.Collision = Collision;
		Params.Definitions = [](uint8 Index) { return UGSTN_LinkRegistry::Get()->GetDefinition(Index); };
		return Params;
	}

	struct FResult
	{
		TArray<FGSTN_LinkWatchdogPost> Posts;
		TArray<FGSTN_LinkWatchdogBreak> Breaks;
	};

	FResult Run(const FPair& Pair, int32 Frame, const IGSTN_ExitCollisionQuery* Collision = nullptr)
	{
		FResult Result;
		FGSTN_LinkWatchdogLogic::Evaluate(Pair.Entities(Frame), Frame, MakeParams(Collision), Result.Posts, Result.Breaks);
		return Result;
	}

	const FGSTN_LinkWatchdogPost* PostFor(const FResult& Result, int32 TargetId)
	{
		return Result.Posts.FindByPredicate([TargetId](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == TargetId; });
	}

	/** Blocks every capsule whose centre is in front of the anchor (X > 10): a wall right ahead of the grabber */
	struct FWallAhead : IGSTN_ExitCollisionQuery
	{
		mutable int32 Asked = 0;
		virtual bool IsCapsuleFree(const FVector& Location, const FQuat& Rotation, float Radius, float HalfHeight) const override
		{
			++Asked;
			return Location.X + Radius <= 10.0;
		}
	};

	struct FAllBlocked : IGSTN_ExitCollisionQuery
	{
		virtual bool IsCapsuleFree(const FVector&, const FQuat&, float, float) const override { return false; }
	};
}

// ---- Watchdog

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPWatchdogBreakAtF1BothSidesTest, "GSTNLink.NP.Watchdog.BreakAtF1BothSides", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPWatchdogBreakAtF1BothSidesTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	// The grab immunity comes from the type's grab rule (data): the test type gets one with its defaults (30 frames after a release)
	Types.Grab->Rules.Add(NewObject<UGSTN_LinkRule_Grab>(Types.Grab));
	FPair Pair(Types.GrabIndex);
	TestTrue(TEXT("a release marks the grabber's link pending"), FGSTN_LinkTickExtension::MarkPendingBreak(Pair.Grabber, static_cast<uint8>(EGSTN_LinkBreakReason::Release)));

	const int32 F = 140;
	const FResult Result = Run(Pair, F);
	TestEqual(TEXT("one break"), Result.Breaks.Num(), 1);
	TestEqual(TEXT("a command for each side"), Result.Posts.Num(), 2);
	const FGSTN_LinkWatchdogPost* ToGrabber = PostFor(Result, GrabberId);
	const FGSTN_LinkWatchdogPost* ToGrabbed = PostFor(Result, GrabbedId);
	if (!TestTrue(TEXT("both sides are posted to"), ToGrabber && ToGrabbed))
	{
		return false;
	}
	TestEqual(TEXT("stamped F"), ToGrabber->StampFrame, F);
	TestEqual(TEXT("stamped F (other side)"), ToGrabbed->StampFrame, F);
	TestEqual(TEXT("the reason is the pending one"), ToGrabber->Command.Reason, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	TestEqual(TEXT("the same serial on both"), ToGrabbed->Command.Serial, Serial);
	TestTrue(TEXT("the parked side teleports"), (ToGrabbed->Command.ExitFlags & GSTNLinkExitFlags::Teleport) != 0);
	TestTrue(TEXT("the grabber does not"), (ToGrabber->Command.ExitFlags & GSTNLinkExitFlags::Teleport) == 0);
	TestEqual(TEXT("the grab target's immunity: F+1 + the grab rule's 30 frames"), ToGrabbed->Command.ImmunityEndFrame, F + 1 + 30);

	// Both consume at F+1: the same frame, the link gone on both, the exit due on F+1 on the parked side only
	TestTrue(TEXT("grabber applies"), FGSTN_LinkInboxHandlers::ApplyCommand(Pair.Grabber, ToGrabber->Command, ToGrabber->StampFrame));
	TestTrue(TEXT("grabbed applies"), FGSTN_LinkInboxHandlers::ApplyCommand(Pair.Grabbed, ToGrabbed->Command, ToGrabbed->StampFrame));
	TestTrue(TEXT("both idle"), Pair.Grabber.Links.Body.IsIdle() && Pair.Grabbed.Links.Body.IsIdle());
	TestEqual(TEXT("serial kept"), Pair.Grabbed.Links.Body.Serial, Serial);
	TestEqual(TEXT("exit frame F+1 on both"), Pair.Grabber.Actions.Exit.Frame, F + 1);
	TestEqual(TEXT("exit frame F+1 (grabbed)"), Pair.Grabbed.Actions.Exit.Frame, F + 1);
	TestTrue(TEXT("the exit request is due on F+1 for the parked side"), FGSTN_LinkExitRequest::IsDue(Pair.Grabbed.Actions.Exit, F + 1));
	TestFalse(TEXT("not for the grabber"), FGSTN_LinkExitRequest::IsDue(Pair.Grabber.Actions.Exit, F + 1));
	TestFalse(TEXT("not again on F+2"), FGSTN_LinkExitRequest::IsDue(Pair.Grabbed.Actions.Exit, F + 2));
	TestEqual(TEXT("the target is grab immune"), Pair.Grabbed.GrabImmunityEndFrame, F + 31);
	TestFalse(TEXT("re-applying the same command changes nothing"), FGSTN_LinkInboxHandlers::ApplyCommand(Pair.Grabbed, ToGrabbed->Command, ToGrabbed->StampFrame));

	// Host as grabber or grabbed: the rule depends on roles and ids only, so swapping which id is lower gives the same frames
	FPair Swapped(Types.GrabIndex);
	FGSTN_LinkTickExtension::MarkPendingBreak(Swapped.Grabbed, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	const FResult SwappedResult = Run(Swapped, F);
	TestEqual(TEXT("a pending flag on either side breaks the same frame"), SwappedResult.Posts.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPWatchdogOrphanAfterGraceTest, "GSTNLink.NP.Watchdog.OrphanAfterGrace", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPWatchdogOrphanAfterGraceTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FPair Pair(Types.GrabIndex);
	// The formation frame: the target holds role 2, the initiator has no response yet
	Pair.Grabber = FGSTN_LinkFragment();
	for (int32 Frame = LinkFrame; Frame < LinkFrame + 3; ++Frame)
	{
		TestEqual(*FString::Printf(TEXT("no break inside the grace (frame %d)"), Frame), Run(Pair, Frame).Posts.Num(), 0);
	}
	const FResult Result = Run(Pair, LinkFrame + 3);
	TestEqual(TEXT("an orphan after the grace"), Result.Breaks.Num(), 1);
	TestTrue(TEXT("flagged orphan"), Result.Breaks.Num() == 1 && Result.Breaks[0].bOrphan);
	TestEqual(TEXT("posted to the orphaned side only"), Result.Posts.Num(), 1);
	TestTrue(TEXT("Forced"), Result.Posts.Num() == 1 && Result.Posts[0].TargetId == GrabbedId && Result.Posts[0].Command.Reason == static_cast<uint8>(EGSTN_LinkBreakReason::Forced));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPWatchdogStaleSerialIgnoredTest, "GSTNLink.NP.Watchdog.StaleSerialIgnored", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPWatchdogStaleSerialIgnoredTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FPair Pair(Types.GrabIndex);
	FGSTN_LinkCommandEntry Stale;
	Stale.Kind = EGSTN_LinkCommandKind::Break;
	Stale.Serial = Serial - 1;
	Stale.Reason = static_cast<uint8>(EGSTN_LinkBreakReason::Forced);
	TestFalse(TEXT("a break of an older serial is ignored"), FGSTN_LinkInboxHandlers::ApplyCommand(Pair.Grabbed, Stale, 150));
	Stale.Serial = Serial + 1;
	TestFalse(TEXT("and of a newer one"), FGSTN_LinkInboxHandlers::ApplyCommand(Pair.Grabbed, Stale, 150));
	TestTrue(TEXT("the link is untouched"), Pair.Grabbed.Links.Body.IsActive() && Pair.Grabbed.Links.Body.Serial == Serial);

	// A partner that holds another serial is not this link's partner: the matched-pair rules do not run, only the orphan rule after the grace
	Pair.Grabber.Links.Body.Serial = Serial + 1;
	TestEqual(TEXT("healthy pair rules skipped for mismatched serials"), Run(Pair, LinkFrame + 1).Posts.Num(), 0);
	const FResult Late = Run(Pair, LinkFrame + 10);
	TestEqual(TEXT("both one-sided links are orphans after the grace"), Late.Breaks.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPWatchdogOnePathForPipelineBreaksTest, "GSTNLink.NP.Watchdog.OnePathForPipelineBreaks", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPWatchdogOnePathForPipelineBreaksTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FGSTN_RoleHitBreak Rule;
	Rule.RoleTag = TestGrabbed;
	Types.Grab->Break.HitBreaks.Add(Rule);
	FPair Pair(Types.GrabIndex);

	// What LinkBreakOnHit hands the applier for a third party's hit on the grabbed: the rule qualifies, the applier marks PendingBreak (Hit)
	TestTrue(TEXT("a third party's hit qualifies"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Grab, 2, false, 300.f));
	TestTrue(TEXT("the mark the applier sets"), FGSTN_LinkTickExtension::MarkPendingBreak(Pair.Grabbed, static_cast<uint8>(EGSTN_LinkBreakReason::Hit)));
	TestFalse(TEXT("nothing breaks the link directly"), Pair.Grabbed.Links.Body.IsIdle());

	const FResult Result = Run(Pair, 160);
	TestEqual(TEXT("the watchdog breaks it, like any other cause"), Result.Posts.Num(), 2);
	TestTrue(TEXT("reason Hit"), Result.Breaks.Num() == 1 && Result.Breaks[0].Reason == EGSTN_LinkBreakReason::Hit);
	return true;
}

// ---- Break rules as the watchdog runs them

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPBreakDistanceTest, "GSTNLink.NP.Break.Distance", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPBreakDistanceTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FPair Pair(Types.GrabIndex);
	TestEqual(TEXT("MaxDistanceCm 0: never"), Run(Pair, 150).Posts.Num(), 0);
	Types.Grab->Break.MaxDistanceCm = 100.f;
	TestEqual(TEXT("the parked capsule where the link expects it: no break"), Run(Pair, 150).Posts.Num(), 0);
	Pair.GrabberAt = FVector(60.0, 0.0, 0.0);
	TestEqual(TEXT("60 cm off: within the leash"), Run(Pair, 150).Posts.Num(), 0);
	Pair.GrabberAt = FVector(500.0, 0.0, 0.0);
	const FResult Result = Run(Pair, 150);
	TestTrue(TEXT("the grabber moved away: Distance"), Result.Breaks.Num() == 1 && Result.Breaks[0].Reason == EGSTN_LinkBreakReason::Distance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPBreakTimeoutTest, "GSTNLink.NP.Break.Timeout", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPBreakTimeoutTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	Types.Grab->MaxFrames = 60;
	FPair Pair(Types.GrabIndex);
	// The pair is decided by the lower id (the grabber, formed one frame after the target): its LinkFrame is LinkFrame + 1
	TestEqual(TEXT("before MaxFrames"), Run(Pair, LinkFrame + 59).Posts.Num(), 0);
	const FResult Result = Run(Pair, LinkFrame + 61);
	TestTrue(TEXT("Timeout"), Result.Breaks.Num() == 1 && Result.Breaks[0].Reason == EGSTN_LinkBreakReason::Timeout);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPBreakDeathTest, "GSTNLink.NP.Break.Death", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPBreakDeathTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FPair Pair(Types.GrabIndex);
	Pair.bGrabbedDead = true;
	FResult Result = Run(Pair, 150);
	TestTrue(TEXT("the grabbed dies: PartnerDeath from the grabber's side"), Result.Breaks.Num() == 1 && Result.Breaks[0].Reason == EGSTN_LinkBreakReason::PartnerDeath);
	Pair.bGrabbedDead = false;
	Pair.bGrabberDead = true;
	Result = Run(Pair, 150);
	TestTrue(TEXT("the grabber dies: OwnDeath"), Result.Breaks.Num() == 1 && Result.Breaks[0].Reason == EGSTN_LinkBreakReason::OwnDeath);
	Types.Grab->Break.bBreakOnOwnDeath = false;
	Types.Grab->Break.bBreakOnPartnerDeath = false;
	TestEqual(TEXT("a type that survives death keeps the link"), Run(Pair, 150).Posts.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPBreakPartnerStateTest, "GSTNLink.NP.Break.PartnerState", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPBreakPartnerStateTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	// Any state tag works for the rule; the immunity tag stands in for hitstun here (no test-only state tag exists)
	const FGameplayTag Forbidden = GSTNLinkTags::Immunity_Latch;
	Types.Grab->Break.PartnerMustNot.AddTag(Forbidden);
	FPair Pair(Types.GrabIndex);
	TestEqual(TEXT("without the tag: no break"), Run(Pair, 150).Posts.Num(), 0);
	Pair.GrabberTags.AddTag(Forbidden);
	const FResult Result = Run(Pair, 150);
	TestTrue(TEXT("the grabber in a forbidden state: PartnerState (the grabbed is released)"),
		Result.Breaks.Num() == 1 && Result.Breaks[0].Reason == EGSTN_LinkBreakReason::PartnerState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPBreakHitByRoleTest, "GSTNLink.NP.Break.HitByRole", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPBreakHitByRoleTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	TestFalse(TEXT("no hit rule: a hit never breaks"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Grab, 2, false, 1000.f));
	FGSTN_RoleHitBreak Rule;
	Rule.RoleTag = TestGrabbed;
	Rule.MinKnockbackToBreak = 200.f;
	Types.Grab->Break.HitBreaks.Add(Rule);
	TestFalse(TEXT("below MinKnockbackToBreak"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Grab, 2, false, 150.f));
	TestTrue(TEXT("at or above it"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Grab, 2, false, 250.f));
	TestFalse(TEXT("the partner's hit (a pummel) keeps the grab"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Grab, 2, true, 250.f));
	TestFalse(TEXT("a role without a rule (the grabber) is not broken by hits"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Grab, 1, false, 1000.f));
	Types.Grab->Break.HitBreaks[0].bPartnerHitsBreak = true;
	TestTrue(TEXT("bPartnerHitsBreak lets the partner's hit break"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Grab, 2, true, 250.f));
	return true;
}

// ---- Exit

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPExitNeverInsideGeometryTest, "GSTNLink.NP.Exit.NeverInsideGeometry", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPExitNeverInsideGeometryTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FPair Pair(Types.GrabIndex);
	FGSTN_LinkTickExtension::MarkPendingBreak(Pair.Grabber, static_cast<uint8>(EGSTN_LinkBreakReason::Release));

	const FWallAhead Wall;
	const FResult Result = Run(Pair, 150, &Wall);
	const FGSTN_LinkWatchdogPost* ToGrabbed = PostFor(Result, GrabbedId);
	if (!TestNotNull(TEXT("the grabbed gets a command"), ToGrabbed))
	{
		return false;
	}
	const FVector Exit(ToGrabbed->Command.ExitPosition);
	TestTrue(TEXT("the placer asked the world"), Wall.Asked > 0);
	TestTrue(TEXT("the exit is a free place"), Wall.IsCapsuleFree(Exit, FQuat::Identity, 34.f, 88.f));
	TestTrue(TEXT("not the fallback"), (ToGrabbed->Command.ExitFlags & GSTNLinkExitFlags::Fallback) == 0);

	const FAllBlocked Blocked;
	const FResult Nowhere = Run(Pair, 150, &Blocked);
	const FGSTN_LinkWatchdogPost* Fallback = PostFor(Nowhere, GrabbedId);
	TestTrue(TEXT("no free place: reported as the fallback, never as a valid exit"), Fallback && (Fallback->Command.ExitFlags & GSTNLinkExitFlags::Fallback) != 0
		&& Nowhere.Breaks.Num() == 1 && Nowhere.Breaks[0].bExitFallback);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPExitDeterministicTest, "GSTNLink.NP.Exit.DeterministicForEqualInputs", GSTNLinkNPTest::Flags)

bool FGSTN_LinkNPExitDeterministicTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FPair Pair(Types.GrabIndex);
	Pair.GrabberAt = FVector(123.456, -78.9, 10.0);
	Pair.GrabbedAt = FVector(150.0, -80.0, 10.0);
	FGSTN_LinkTickExtension::MarkPendingBreak(Pair.Grabber, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
	const FWallAhead Wall;
	const FResult A = Run(Pair, 150, &Wall);
	const FResult B = Run(Pair, 150, &Wall);
	const FGSTN_LinkWatchdogPost* PA = PostFor(A, GrabbedId);
	const FGSTN_LinkWatchdogPost* PB = PostFor(B, GrabbedId);
	if (!TestTrue(TEXT("both runs post"), PA && PB))
	{
		return false;
	}
	TestTrue(TEXT("the same exit position, bit for bit"), FMemory::Memcmp(&PA->Command.ExitPosition, &PB->Command.ExitPosition, sizeof(FVector3f)) == 0);
	TestEqual(TEXT("the same yaw"), PA->Command.ExitYawDeg, PB->Command.ExitYawDeg);
	TestEqual(TEXT("the same flags"), PA->Command.ExitFlags, PB->Command.ExitFlags);

	// The command carries floats: the quantised exit is what both the authority and every client apply
	FGSTN_LinkFragment Applied = Pair.Grabbed;
	FGSTN_LinkInboxHandlers::ApplyCommand(Applied, PA->Command, 150);
	TestEqual(TEXT("the exit record holds the command's floats"), Applied.Actions.Exit.Position, FVector(PA->Command.ExitPosition));
	return true;
}

#endif

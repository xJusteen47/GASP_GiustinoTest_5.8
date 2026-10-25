// Automation tests of the victim side of the combat tick (combat-m4-hits 7, M4 H9). They need no world: the snapshot is built by hand and
// the steps are the pure functions FGSTN_CombatVictim::Resolve and StepReaction over a sync core.

#include "Misc/AutomationTest.h"
#include "GSTN_VictimTestTypes.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Simulation/GSTN_HitInbox.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/**
	 * A victim: entity 2 on team 2 at the origin, one sphere hurtbox, the standard rules (weight driven, push back from 540 cm/s, launch from
	 * 1080, tumble from 1800, a stagger on every hit). Hits arrive from an attacker at -X, so the launch direction is +X
	 */
	struct FVictimFixture
	{
		static constexpr int32 VictimId = 2;

		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set;
		FGSTN_SnapshotEntry Self;
		FGSTN_CombatSyncCore Core;
		FGSTN_SyncFragmentArray Fragments;
		FGSTN_VictimResult Result;
		TArray<FGSTN_HitInboxEntry> Entries;
		bool bGrounded = true;
		bool bOnWall = false;

		FVictimFixture()
		{
			Rules.Reset(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
			Set.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			FGSTN_Hurtbox Hurtbox;
			Hurtbox.Shape.Type = EGSTN_ShapeType::Sphere;
			Hurtbox.Shape.Extent = FVector(40.0);
			Set->Hurtboxes.Add(Hurtbox);

			Self.EntityId = VictimId;
			Self.Team = FGenericTeamId(2);
			Self.Transform = FTransform::Identity;
			Self.Hurtboxes = Set.Get();
			Self.HurtStates.SetNum(1);
			Self.Reaction = FGSTN_VictimReactionState::Make(0.0, 1.0, 0, FGSTN_StaggerThreshold());
		}

		/** An entry from attacker 1, stamped Frame, with damage 10 and a knockback bracket of BaseKnockback + 20 x 10 (the weight is 100, so K is the bracket) */
		static FGSTN_HitInboxEntry MakeEntry(int32 Frame, float BaseKnockback, int32 AttackerId = 1)
		{
			FGSTN_HitInboxEntry Entry;
			Entry.Frame = Frame;
			Entry.AttackerId = AttackerId;
			Entry.AttackerTeam = 1;
			Entry.Hit.Damage = 10.f;
			Entry.Hit.BaseKnockback = BaseKnockback;
			Entry.Hit.KnockbackGrowth = 20.f;
			Entry.Hit.AngleDeg = 30.f;
			Entry.Hit.YawSource = EGSTN_YawSource::AttackerFacing;
			Entry.HitLocation = FVector(-40.0, 0.0, 0.0);
			Entry.AttackerYaw = 0.f;
			Entry.Relationship = GSTNHitRelationship::ToIndex(GSTNCombatHitTags::Relationship_Enemy.GetTag());
			return Entry;
		}

		/** Resolves Entries on Frame (the frame after the hits) */
		void Run(int32 Frame)
		{
			FGSTN_VictimTickInput In;
			In.Frame = Frame;
			In.Self = &Self;
			In.Rules = Rules.Get();
			In.Entries = Entries;
			In.bGrounded = bGrounded;
			In.bOnWall = bOnWall;
			In.Fragments = &Fragments;
			FGSTN_CombatVictim::Resolve(In, Core, Result);
		}

		FGSTN_ReactionMove Step(int32 Frame, const FVector2D& Stick = FVector2D::ZeroVector, float Resistance = 0.f)
		{
			FGSTN_ReactionStepInput In;
			In.Frame = Frame;
			In.Rules = Rules.Get();
			In.Up = Self.Up;
			In.Facing = Self.Facing;
			In.bGrounded = bGrounded;
			In.Stick = Stick;
			In.KnockbackResistance = Resistance;
			return FGSTN_CombatVictim::StepReaction(In, Core);
		}

		void ExpectStaggered(FAutomationTestBase& Test, EGSTN_HitReaction Reaction, const TCHAR* What) const
		{
			Test.TestTrue(FString::Printf(TEXT("%s: in hitstun"), What), Core.ActionState == EGSTN_ActionState::Hitstun || Core.ActionState == EGSTN_ActionState::Tumble);
			Test.TestTrue(FString::Printf(TEXT("%s: the tier"), What), Core.Hit.HasPendingReaction() && Core.Hit.GetPendingReaction() == Reaction);
		}
	};

	/** Counts what the test extension reached it with */
	struct FCountingApplier : IGSTN_OutcomeExtensionApplier
	{
		mutable TArray<int32> Values;
		mutable int32 Fragments = 0;

		virtual const UScriptStruct* GetExtensionStruct() const override { return FGSTN_TestVictimExtension::StaticStruct(); }
		virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const override
		{
			Values.Add(Extension.Get<FGSTN_TestVictimExtension>().Value);
			Fragments += Context.VictimFragments ? 1 : 0;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimDrainAtNextFrameTest, "GSTNCombat.NP.Hits.Victim.DrainAtNextFrame", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimDrainAtNextFrameTest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	FGSTN_HitInbox Inbox;
	Inbox.Add(FVictimFixture::MakeEntry(10, 600.f));

	TArray<FGSTN_HitInboxEntry> Drained;
	Inbox.Drain(10, Drained);
	TestEqual(TEXT("the frame of the hit does not see it"), Drained.Num(), 0);
	Inbox.Drain(11, Drained);
	TestEqual(TEXT("the next frame does"), Drained.Num(), 1);

	Fixture.Entries = Drained;
	Fixture.Run(11);
	TestEqual(TEXT("one hit resolved"), Fixture.Result.Hits.Num(), 1);
	TestEqual(TEXT("the window opens on the frame of the hit"), Fixture.Core.HitstopStartFrame, 10);
	TestTrue(TEXT("and it already stands on the frame of the drain"), Fixture.Core.IsInHitstop(11));
	TestTrue(TEXT("the freeze starts now and ends with the window"), Fixture.Result.FreezeFrames == Fixture.Core.HitstopEndFrame - 11 && Fixture.Result.FreezeFrames > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimTradeBothResolveTest, "GSTNCombat.NP.Hits.Victim.TradeBothResolve", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimTradeBothResolveTest::RunTest(const FString& Parameters)
{
	// Two fighters hit each other on frame 10; whichever resolves first, both land and end in the same state
	FVictimFixture A;
	FVictimFixture B;
	A.Entries = { FVictimFixture::MakeEntry(10, 1500.f, 2) };
	B.Entries = { FVictimFixture::MakeEntry(10, 1500.f, 1) };
	A.Core.HitstopStartFrame = 10;
	A.Core.HitstopEndFrame = 14;
	B.Core.HitstopStartFrame = 10;
	B.Core.HitstopEndFrame = 14;

	A.Run(11);
	B.Run(11);
	FVictimFixture A2;
	FVictimFixture B2;
	A2.Entries = A.Entries;
	B2.Entries = B.Entries;
	A2.Core = A.Core;
	B2.Core = B.Core;
	// The other order of resolution (and a fresh state each) gives the same answer
	B2.Core = FGSTN_CombatSyncCore();
	A2.Core = FGSTN_CombatSyncCore();
	A2.Core.HitstopStartFrame = 10;
	A2.Core.HitstopEndFrame = 14;
	B2.Core.HitstopStartFrame = 10;
	B2.Core.HitstopEndFrame = 14;
	B2.Run(11);
	A2.Run(11);

	A.ExpectStaggered(*this, EGSTN_HitReaction::Launch, TEXT("A"));
	B.ExpectStaggered(*this, EGSTN_HitReaction::Launch, TEXT("B"));
	TestTrue(TEXT("A is the same in either order"), A.Core.Hit == A2.Core.Hit && A.Core.ActionState == A2.Core.ActionState && A.Core.HitstopEndFrame == A2.Core.HitstopEndFrame);
	TestTrue(TEXT("B is the same in either order"), B.Core.Hit == B2.Core.Hit && B.Core.ActionState == B2.Core.ActionState && B.Core.HitstopEndFrame == B2.Core.HitstopEndFrame);
	TestTrue(TEXT("the trade made one window per fighter: the longer of the two"), A.Core.HitstopEndFrame >= 14);
	TestEqual(TEXT("a window that already covered the end asks for no second freeze or only the extra"), A.Result.FreezeFrames, FMath::Max(A.Core.HitstopEndFrame - 11, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimStaggeredSetsStateAndWindowsTest, "GSTNCombat.NP.Hits.Victim.StaggeredSetsStateAndWindows", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimStaggeredSetsStateAndWindowsTest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	Fixture.Core.ActionState = EGSTN_ActionState::InTimeline;
	Fixture.Core.ActiveTimeline = 3;
	Fixture.Core.TimelineStartFrame = 4;
	// K = 1500 + 200 = 1700: a launch below the tumble point
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 1500.f) };
	Fixture.Run(11);

	TestEqual(TEXT("one hit"), Fixture.Result.Hits.Num(), 1);
	const FGSTN_VictimHit& Hit = Fixture.Result.Hits[0];
	TestTrue(TEXT("staggered"), Hit.bStaggered);
	TestTrue(TEXT("a launch, no tumble"), Hit.Reaction == EGSTN_HitReaction::Launch && !Hit.bTumble);
	TestTrue(TEXT("hitstun state"), Fixture.Core.ActionState == EGSTN_ActionState::Hitstun);
	TestTrue(TEXT("the move is cancelled: no timeline left, a CancelTimeline request on the frame"), Fixture.Core.ActiveTimeline == INDEX_NONE && Fixture.Result.bCancelledMove
		&& Fixture.Core.Requests.Find(EGSTN_RequestType::CancelTimeline, 11, 3) != nullptr);
	TestEqual(TEXT("hitstop starts on the frame of the hit"), Fixture.Core.HitstopStartFrame, 10);
	TestTrue(TEXT("and lasts the hitstop of the pipeline"), Hit.HitstopFrames > 0 && Fixture.Core.HitstopEndFrame == 10 + Hit.HitstopFrames);
	TestEqual(TEXT("hitstun ends a hitstun after the window"), Fixture.Core.Hit.HitstunEndFrame, Fixture.Core.HitstopEndFrame + Hit.HitstunFrames);
	TestTrue(TEXT("the pending launch points away from the attacker"), Fixture.Core.Hit.GetPendingLaunch().X > 1000.0 && Fixture.Core.Hit.GetPendingLaunch().Z > 0.0);
	TestTrue(TEXT("the damage request is in the outcome"), Hit.Outcome.Requests.ContainsByPredicate([](const FGSTN_HitRequest& Request) { return Request.Kind == EGSTN_HitRequestKind::Damage && Request.Target == EGSTN_HitRequestTarget::Victim; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimNotStaggeredKeepsMoveTest, "GSTNCombat.NP.Hits.Victim.NotStaggeredKeepsMove", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimNotStaggeredKeepsMoveTest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	Fixture.Rules->StaggerThreshold = 1000.f;
	Fixture.Core.ActionState = EGSTN_ActionState::InTimeline;
	Fixture.Core.ActiveTimeline = 3;
	Fixture.Core.TimelineStartFrame = 4;
	// Stagger force defaults to the damage (10), far below 1000
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 1500.f) };
	Fixture.Run(11);

	TestFalse(TEXT("not staggered"), Fixture.Result.Hits[0].bStaggered);
	TestTrue(TEXT("the move runs on"), Fixture.Core.ActionState == EGSTN_ActionState::InTimeline && Fixture.Core.ActiveTimeline == 3 && !Fixture.Result.bCancelledMove);
	TestFalse(TEXT("no pending reaction"), Fixture.Core.Hit.HasPendingReaction());
	TestTrue(TEXT("the hitstop window and the freeze are there"), Fixture.Core.IsInHitstop(11) && Fixture.Result.FreezeFrames > 0);
	TestFalse(TEXT("the freeze keeps the velocity"), Fixture.Result.bFreezeZeroVelocity);
	TestTrue(TEXT("the damage still goes out"), Fixture.Result.Hits[0].Outcome.Damage > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimAppliersCalledPerExtensionTest, "GSTNCombat.NP.Hits.Victim.AppliersCalledPerExtension", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimAppliersCalledPerExtensionTest::RunTest(const FString& Parameters)
{
	FCountingApplier Applier;
	FGSTN_OutcomeApplierRegistry::Get().Register(&Applier);

	FVictimFixture Fixture;
	Fixture.Rules->HitPipeline.Add(UGSTN_TestVictimExtensionStep::StaticClass());
	Fixture.Rules->ResolveHitPipeline();
	// A staggered entry and, with the threshold up, one that is not: the appliers run for both
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 1500.f, 1), FVictimFixture::MakeEntry(10, 1500.f, 3) };
	Fixture.Entries[1].Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
	Fixture.Entries[1].Hit.StaggerForce = 0.f;
	Fixture.Rules->StaggerThreshold = 5.f;
	Fixture.Run(11);
	FGSTN_OutcomeApplierRegistry::Get().Unregister(&Applier);

	TestEqual(TEXT("both hits resolved"), Fixture.Result.Hits.Num(), 2);
	TestTrue(TEXT("one staggered and one not"), Fixture.Result.Hits[0].bStaggered && !Fixture.Result.Hits[1].bStaggered);
	TestEqual(TEXT("the applier ran once per extension entry, per hit"), Applier.Values.Num(), 4);
	TestTrue(TEXT("in the order of the entries"), Applier.Values == TArray<int32>({ 1, 2, 1, 2 }));
	TestEqual(TEXT("and was handed the victim's fragments each time"), Applier.Fragments, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimPushBackPromotedWhenAirborneTest, "GSTNCombat.NP.Hits.Victim.PushBackPromotedWhenAirborne", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimPushBackPromotedWhenAirborneTest::RunTest(const FString& Parameters)
{
	// K = 600 + 200 = 800: the push back tier
	for (int32 Case = 0; Case < 2; ++Case)
	{
		FVictimFixture Fixture;
		Fixture.bGrounded = Case == 1;
		Fixture.bOnWall = Case == 1;
		Fixture.Entries = { FVictimFixture::MakeEntry(10, 600.f) };
		Fixture.Run(11);
		const FString What = Case == 0 ? TEXT("airborne") : TEXT("on a wall");
		TestTrue(*(What + TEXT(": promoted")), Fixture.Result.Hits[0].bPromoted);
		TestTrue(*(What + TEXT(": a launch")), Fixture.Core.Hit.GetPendingReaction() == EGSTN_HitReaction::Launch);
		TestTrue(*(What + TEXT(": the launch goes up too")), Fixture.Core.Hit.GetPendingLaunch().Z > 100.0);
		TestFalse(*(What + TEXT(": under the tumble point")), Fixture.Result.Hits[0].bTumble);
	}

	FVictimFixture Grounded;
	Grounded.Entries = { FVictimFixture::MakeEntry(10, 600.f) };
	Grounded.Run(11);
	TestFalse(TEXT("on the ground it stays a push back"), Grounded.Result.Hits[0].bPromoted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimHitstopEndLaunchWithDITest, "GSTNCombat.NP.Hits.Victim.HitstopEndLaunchWithDI", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimHitstopEndLaunchWithDITest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 1500.f) };
	Fixture.Run(11);
	const int32 End = Fixture.Core.HitstopEndFrame;
	const FVector Launch = Fixture.Core.Hit.GetPendingLaunch();

	TestTrue(TEXT("nothing moves during the window"), Fixture.Step(End - 1).Kind == FGSTN_ReactionMove::EKind::None && Fixture.Core.Hit.HasPendingReaction());

	FVictimFixture WithDI = Fixture;
	const FGSTN_ReactionMove Plain = Fixture.Step(End);
	TestTrue(TEXT("the launch is requested when the window ends"), Plain.Kind == FGSTN_ReactionMove::EKind::Launch);
	TestTrue(TEXT("and clears the pending reaction"), !Fixture.Core.Hit.HasPendingReaction() && Fixture.Core.ActionState == EGSTN_ActionState::Hitstun);
	TestTrue(TEXT("with no stick it is the launch of the hit"), Plain.Velocity.Equals(Launch, 1.0));
	TestEqual(TEXT("it lasts the hitstun that is left"), Plain.Frames, Fixture.Core.Hit.HitstunEndFrame - End);

	const FGSTN_ReactionMove Turned = WithDI.Step(End, FVector2D(1.0, 0.0));
	TestTrue(TEXT("a stick across the launch turns it"), !Turned.Velocity.Equals(Plain.Velocity, 10.0));
	TestTrue(TEXT("without changing the speed"), FMath::IsNearlyEqual(Turned.Velocity.Size(), Plain.Velocity.Size(), 1.0));

	// DI is relative to the victim's up, never world Z: with gravity along +Y the stick plane is XZ and the launch keeps its speed
	FVictimFixture Tilted;
	Tilted.Self.Up = FVector(0.0, 1.0, 0.0);
	Tilted.Core = Fixture.Core;
	Tilted.Core.ActionState = EGSTN_ActionState::Hitstun;
	Tilted.Core.Hit.SetPendingReaction(EGSTN_HitReaction::Launch);
	Tilted.Core.Hit.SetPendingLaunch(FVector(1000.0, 800.0, 0.0));
	const FVector Plane = FGSTN_CombatVictim::StickToPlane(FVector2D(1.0, 1.0), 0.0, Tilted.Self.Up);
	TestTrue(TEXT("the stick lies in the plane normal to up"), FMath::IsNearlyZero(Plane.Dot(Tilted.Self.Up), 1e-6));
	const FGSTN_ReactionMove TiltedMove = Tilted.Step(End, FVector2D(0.0, 1.0));
	TestTrue(TEXT("a launch on a tilted gravity keeps its speed"), TiltedMove.Kind == FGSTN_ReactionMove::EKind::Launch && FMath::IsNearlyEqual(TiltedMove.Velocity.Size(), FVector(1000.0, 800.0, 0.0).Size(), 5.0));

	// The knockback resistance scales the speed
	FVictimFixture Resisted = Fixture;
	Resisted.Core.ActionState = EGSTN_ActionState::Hitstun;
	Resisted.Core.Hit.SetPendingReaction(EGSTN_HitReaction::Launch);
	Resisted.Core.Hit.SetPendingLaunch(Launch);
	TestTrue(TEXT("resistance 0.5 halves it"), FMath::IsNearlyEqual(Resisted.Step(End, FVector2D::ZeroVector, 0.5f).Velocity.Size(), Launch.Size() * 0.5, 2.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimPushBackStaysGroundedTest, "GSTNCombat.NP.Hits.Victim.PushBackStaysGrounded", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimPushBackStaysGroundedTest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 600.f) };
	Fixture.Run(11);
	Fixture.ExpectStaggered(*this, EGSTN_HitReaction::PushBack, TEXT("push back"));
	TestTrue(TEXT("the lock is at least the floor of the rules"), Fixture.Result.Hits[0].HitstunFrames >= Fixture.Rules->MinStaggerLockFrames);

	const int32 End = Fixture.Core.HitstopEndFrame;
	const FGSTN_ReactionMove Move = Fixture.Step(End, FVector2D(1.0, 0.0));
	TestTrue(TEXT("a push back, no DI"), Move.Kind == FGSTN_ReactionMove::EKind::PushBack);
	TestTrue(TEXT("horizontal on the victim's plane"), FMath::IsNearlyZero(Move.Velocity.Dot(Fixture.Self.Up), 1e-3) && Move.Velocity.X > 100.0);
	TestTrue(TEXT("the speed falls to zero by the end of the hitstun"), FMath::IsNearlyEqual(Move.DecayPerFrame * Move.Frames, 1.f, 1e-4f));
	TestTrue(TEXT("the state stays hitstun (not tumble) until the end"), Fixture.Core.ActionState == EGSTN_ActionState::Hitstun);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimStaggerInPlaceZeroVelocityTest, "GSTNCombat.NP.Hits.Victim.StaggerInPlaceZeroVelocity", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimStaggerInPlaceZeroVelocityTest::RunTest(const FString& Parameters)
{
	// K = 300 + 200 = 500, under the push back threshold
	FVictimFixture Fixture;
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 300.f) };
	Fixture.Run(11);
	Fixture.ExpectStaggered(*this, EGSTN_HitReaction::StaggerInPlace, TEXT("stagger in place"));
	TestTrue(TEXT("the freeze leaves the victim at rest"), Fixture.Result.bFreezeZeroVelocity);
	TestTrue(TEXT("nothing is pending to launch"), Fixture.Core.Hit.GetPendingLaunch().IsNearlyZero());
	TestTrue(TEXT("the lock is the floor of the rules"), Fixture.Core.Hit.HitstunEndFrame - Fixture.Core.HitstopEndFrame == Fixture.Rules->MinStaggerLockFrames);

	const FGSTN_ReactionMove Move = Fixture.Step(Fixture.Core.HitstopEndFrame);
	TestTrue(TEXT("no move at the end of the window"), Move.Kind == FGSTN_ReactionMove::EKind::None && !Fixture.Core.Hit.HasPendingReaction());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimSecondHitRecomputesTierTest, "GSTNCombat.NP.Hits.Victim.SecondHitRecomputesTier", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimSecondHitRecomputesTierTest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 600.f) };
	Fixture.Run(11);
	Fixture.ExpectStaggered(*this, EGSTN_HitReaction::PushBack, TEXT("first hit"));
	const int32 FirstEnd = Fixture.Core.HitstopEndFrame;

	// A harder hit on the next frame, inside the window and the hitstun: no special case, the tier is the new hit's
	Fixture.Entries = { FVictimFixture::MakeEntry(11, 2000.f) };
	Fixture.Run(12);
	Fixture.ExpectStaggered(*this, EGSTN_HitReaction::Launch, TEXT("second hit"));
	TestTrue(TEXT("tumble now"), Fixture.Core.ActionState == EGSTN_ActionState::Tumble);
	TestTrue(TEXT("the window grew to cover the new hit"), Fixture.Core.HitstopStartFrame == 10 && Fixture.Core.HitstopEndFrame >= FirstEnd);
	TestTrue(TEXT("hitstun counts from the end of the longer window"), Fixture.Core.Hit.HitstunEndFrame > Fixture.Core.HitstopEndFrame);

	// And a gentler one replaces it back down
	Fixture.Entries = { FVictimFixture::MakeEntry(12, 300.f) };
	Fixture.Run(13);
	Fixture.ExpectStaggered(*this, EGSTN_HitReaction::StaggerInPlace, TEXT("third hit"));
	TestTrue(TEXT("hitstun again, not tumble"), Fixture.Core.ActionState == EGSTN_ActionState::Hitstun);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimSubThresholdDuringHitstunNoChangeTest, "GSTNCombat.NP.Hits.Victim.SubThresholdDuringHitstunNoChange", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimSubThresholdDuringHitstunNoChangeTest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	Fixture.Rules->StaggerThreshold = 5.f;
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 1500.f) };
	Fixture.Run(11);
	Fixture.ExpectStaggered(*this, EGSTN_HitReaction::Launch, TEXT("first hit"));
	const FGSTN_CombatSyncCore Before = Fixture.Core;

	Fixture.Entries = { FVictimFixture::MakeEntry(12, 2000.f) };
	Fixture.Entries[0].Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
	Fixture.Entries[0].Hit.StaggerForce = 1.f;
	Fixture.Run(13);

	TestFalse(TEXT("below the threshold"), Fixture.Result.Hits[0].bStaggered);
	TestTrue(TEXT("state, windows and pending reaction are untouched"), Fixture.Core.ActionState == Before.ActionState && Fixture.Core.HitstopStartFrame == Before.HitstopStartFrame
		&& Fixture.Core.HitstopEndFrame == Before.HitstopEndFrame && Fixture.Core.Hit == Before.Hit);
	TestEqual(TEXT("no second freeze"), Fixture.Result.FreezeFrames, 0);
	TestTrue(TEXT("the damage still goes out"), Fixture.Result.Hits[0].Outcome.Damage > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimTailCancelAfterHitstunEndTest, "GSTNCombat.NP.Hits.Victim.TailCancelAfterHitstunEnd", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimTailCancelAfterHitstunEndTest::RunTest(const FString& Parameters)
{
	FVictimFixture Fixture;
	Fixture.Entries = { FVictimFixture::MakeEntry(10, 300.f) };
	Fixture.Run(11);
	const int32 End = Fixture.Core.HitstopEndFrame;
	const int32 HitstunEnd = Fixture.Core.Hit.HitstunEndFrame;
	Fixture.Step(End);

	TestTrue(TEXT("locked during the hitstun"), FGSTN_CombatVictim::IsActionLocked(Fixture.Core));
	Fixture.Step(HitstunEnd - 1);
	TestTrue(TEXT("still locked on the last frame of it"), FGSTN_CombatVictim::IsActionLocked(Fixture.Core));
	Fixture.Step(HitstunEnd);
	TestTrue(TEXT("Actionable on the first frame of the tail: any command is allowed"), Fixture.Core.ActionState == EGSTN_ActionState::Actionable && !FGSTN_CombatVictim::IsActionLocked(Fixture.Core));

	// A tumble goes on until the victim lands
	FVictimFixture Tumble;
	Tumble.Entries = { FVictimFixture::MakeEntry(10, 2000.f) };
	Tumble.Run(11);
	TestTrue(TEXT("tumbling"), Tumble.Core.ActionState == EGSTN_ActionState::Tumble);
	Tumble.Step(Tumble.Core.HitstopEndFrame);
	Tumble.bGrounded = false;
	Tumble.Step(Tumble.Core.Hit.HitstunEndFrame + 20);
	TestTrue(TEXT("still tumbling in the air after the hitstun"), Tumble.Core.ActionState == EGSTN_ActionState::Tumble);
	Tumble.bGrounded = true;
	Tumble.Step(Tumble.Core.Hit.HitstunEndFrame + 21);
	TestTrue(TEXT("Actionable once it lands"), Tumble.Core.ActionState == EGSTN_ActionState::Actionable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimRollbackReDeliversTest, "GSTNCombat.NP.Hits.Victim.RollbackReDelivers", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimRollbackReDeliversTest::RunTest(const FString& Parameters)
{
	// The state before the hit is restored, the inbox drops what the re-simulation re-delivers, and the same entry gives the same state
	FGSTN_HitInbox Inbox;
	Inbox.Add(FVictimFixture::MakeEntry(8, 600.f));
	Inbox.Add(FVictimFixture::MakeEntry(10, 1500.f));
	Inbox.TrimFrom(10);
	TestEqual(TEXT("entries from the first re-simulated frame on are dropped, earlier ones are kept"), Inbox.Num(), 1);

	FVictimFixture First;
	const FGSTN_CombatSyncCore Restored = First.Core;
	First.Entries = { FVictimFixture::MakeEntry(10, 1500.f) };
	First.Run(11);
	const FGSTN_CombatSyncCore Resolved = First.Core;

	FVictimFixture Again;
	Again.Core = Restored;
	Again.Entries = { FVictimFixture::MakeEntry(10, 1500.f) };
	Again.Run(11);
	TestTrue(TEXT("the re-delivered hit gives the same state"), Again.Core.Hit == Resolved.Hit && Again.Core.ActionState == Resolved.ActionState
		&& Again.Core.HitstopStartFrame == Resolved.HitstopStartFrame && Again.Core.HitstopEndFrame == Resolved.HitstopEndFrame && Again.Core.Requests == Resolved.Requests);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

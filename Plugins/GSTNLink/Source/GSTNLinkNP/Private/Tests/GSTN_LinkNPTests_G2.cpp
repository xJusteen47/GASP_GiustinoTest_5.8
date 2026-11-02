// Automation tests of the M7 G2 follow-ups of the link plugin: partner moves (GSTNLink.NP.PartnerMove.*), the parked facing
// (GSTNLink.NP.Facing.*) and the wire forms they add (GSTNLink.NP.Fragment.PartnerMoveAndFacingRoundTrip). No world: the extension's steps, the
// inbox rule, the combat inbox and the watchdog's pure evaluator on fragments and entity views.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/GSTN_LinkActionProvider.h"
#include "GSTN_LinkTags.h"
#include "GSTN_LinkWatchdog.h"
#include "Placement/GSTN_Placer_Ring.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "State/GSTN_LinkMath.h"

// One namespace per test file: the unity build merges the files of this module, so their helpers must not share a scope
namespace GSTNLinkNPTests_G2
{

using namespace GSTNLinkNPTest;

namespace
{
	constexpr int32 InitiatorId = 31;
	constexpr int32 PartnerId = 32;
	constexpr uint8 Serial = 5;
	constexpr int32 LinkFrame = 100;
	constexpr int32 KeyFrame = 200;

	/** Action ids of the test table: three start offsets, an outcome break, a long move with a duration limit, one with requirements */
	constexpr int16 MoveOffset0 = 1;
	constexpr int16 MoveOffset1 = 2;
	constexpr int16 MoveOffset12 = 3;
	constexpr int16 MoveBreak = 4;
	constexpr int16 MoveLimited = 5;
	constexpr int16 MoveNeedsTag = 6;
	constexpr int32 OutcomeAt = 15;

	/** The fixture's link types plus an action table registered for the test latch (both of its roles act, its initiator is parked) */
	struct FScopedMoves
	{
		FScopedLinkTypes Types;
		UGSTN_LinkActionTable* Table = nullptr;
		UGSTN_Placer_Ring* MovePlacer = nullptr;

		FScopedMoves()
		{
			Table = NewObject<UGSTN_LinkActionTable>(GetTransientPackage());
			Table->AddToRoot();
			MovePlacer = NewObject<UGSTN_Placer_Ring>(GetTransientPackage());
			MovePlacer->AddToRoot();
			MovePlacer->RadiusCm = 300.f;
			const int32 Offsets[] = { 0, 1, 12 };
			const int16 Ids[] = { MoveOffset0, MoveOffset1, MoveOffset12 };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FGSTN_LinkActionInfo Info;
				Info.PartnerAbility = UObject::StaticClass();
				Info.PartnerStartOffsetFrames = Offsets[Index];
				Table->SetAction(Ids[Index], Info);
			}
			FGSTN_LinkActionInfo Break;
			Break.PartnerAbility = UObject::StaticClass();
			Break.PartnerStartOffsetFrames = 2;
			Break.OutcomeFrame = OutcomeAt;
			Break.Outcome = EGSTN_LinkActionOutcome::Break;
			Break.BreakReason = GSTNLinkTags::BreakReason_Escape;
			Break.ExitPlacer = MovePlacer;
			Table->SetAction(MoveBreak, Break);
			FGSTN_LinkActionInfo Limited;
			Limited.PartnerAbility = UObject::StaticClass();
			Limited.MaxDurationFrames = 20;
			Table->SetAction(MoveLimited, Limited);
			FGSTN_LinkActionInfo NeedsTag;
			NeedsTag.PartnerAbility = UObject::StaticClass();
			NeedsTag.PartnerRequirements.RequireTags.AddTag(GSTNLinkTags::Immunity_Latch);
			Table->SetAction(MoveNeedsTag, NeedsTag);
			UGSTN_LinkRegistry::Get()->RegisterActionProvider(TestLatch, Table);
		}

		~FScopedMoves()
		{
			UGSTN_LinkRegistry::Get()->UnregisterActionProvider(TestLatch);
			MovePlacer->RemoveFromRoot();
			Table->RemoveFromRoot();
		}

		FGSTN_LinkActionInfo Info(int16 Id) const
		{
			FGSTN_LinkActionInfo Out;
			UGSTN_LinkRegistry::Get()->ResolveLinkAction(Types.LatchIndex, Id, Out);
			return Out;
		}
	};

	FGSTN_LinkFragment InitiatorSide(const FScopedMoves& Scope) { return MakeLinked(Scope.Types.LatchIndex, 1, PartnerId, Serial, LinkFrame); }
	FGSTN_LinkFragment PartnerSide(const FScopedMoves& Scope) { return MakeLinked(Scope.Types.LatchIndex, 2, InitiatorId, Serial, LinkFrame); }

	/** One frame of StepPartnerMoves for an entity whose timeline (if any) started on TimelineStart */
	FGSTN_LinkTickExtension::FPartnerMoveResult Step(FGSTN_LinkFragment& Fragment, int32 Frame, int32 TimelineStart = INDEX_NONE, bool bKnockedOut = false,
		const FGameplayTagContainer& Tags = FGameplayTagContainer())
	{
		FGSTN_LinkTickExtension::FPartnerMoveFrame In;
		In.Frame = Frame;
		In.OwnTags = Tags;
		In.bKnockedOut = bKnockedOut;
		In.bInTimeline = TimelineStart != INDEX_NONE;
		In.TimelineStartFrame = TimelineStart;
		FGSTN_LinkTickExtension::FPartnerMoveResult Result;
		FGSTN_LinkTickExtension::StepPartnerMoves(Fragment, In, UGSTN_LinkRegistry::Get(), Result);
		return Result;
	}

	/** The initiator's frame with its timeline (started on TimelineStart) crossing the timeline frames (Previous, Current] */
	bool StepCrossing(FGSTN_LinkFragment& Fragment, int32 Frame, int32 TimelineStart, int32 Previous, int32 Current)
	{
		FGSTN_LinkTickExtension::FPartnerMoveFrame In;
		In.Frame = Frame;
		In.bInTimeline = true;
		In.TimelineStartFrame = TimelineStart;
		In.bTimelineCrossed = true;
		In.TimelinePrevious = Previous;
		In.TimelineCurrent = Current;
		FGSTN_LinkTickExtension::FPartnerMoveResult Result;
		return FGSTN_LinkTickExtension::StepPartnerMoves(Fragment, In, UGSTN_LinkRegistry::Get(), Result);
	}

	FGSTN_LinkWatchdogEntity WatchdogEntity(int32 Id, const FGSTN_LinkFragment& Fragment, const FTransform& Transform)
	{
		FGSTN_LinkWatchdogEntity Entity;
		Entity.EntityId = Id;
		Entity.Links = Fragment.Links;
		Entity.ActiveMove = Fragment.Actions.Active;
		Entity.Transform = Transform;
		return Entity;
	}
}

// ---- Partner moves

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerMoveTargetFrameTest, "GSTNLink.NP.PartnerMove.StartsAtTargetFrame_0_1_12", Flags)
bool FGSTN_LinkPartnerMoveTargetFrameTest::RunTest(const FString& Parameters)
{
	FScopedMoves Scope;
	const int32 Offsets[] = { 0, 1, 12 };
	const int16 Ids[] = { MoveOffset0, MoveOffset1, MoveOffset12 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const int32 Expected = KeyFrame + FMath::Max(Offsets[Index], 1);
		FGSTN_LinkFragment Initiator = InitiatorSide(Scope);
		FGSTN_LinkFragment Partner = PartnerSide(Scope);

		FGSTN_LinkPartnerMoveEntry Entry;
		TestTrue(FString::Printf(TEXT("offset %d: the key starts the move"), Offsets[Index]), FGSTN_LinkTickExtension::StartPartnerMove(Initiator, Ids[Index], KeyFrame, Scope.Info(Ids[Index]), Entry));
		TestTrue(TEXT("the initiator records it"), Initiator.Actions.Active.Id == Ids[Index] && Initiator.Actions.Active.StartFrame == KeyFrame
			&& Initiator.Actions.Active.Side == GSTNLinkActionSide::Initiator);
		TestEqual(TEXT("the entry carries the link's serial"), static_cast<int32>(Entry.Serial), static_cast<int32>(Serial));
		TestEqual(TEXT("target frame = S + max(offset, 1)"), Entry.TargetFrame, Expected);
		FGSTN_LinkPartnerMoveEntry Second;
		TestFalse(TEXT("one partner move at a time"), FGSTN_LinkTickExtension::StartPartnerMove(Initiator, Ids[Index], KeyFrame + 1, Scope.Info(Ids[Index]), Second));

		// Drained at S + 1 (the inbox rule), scheduled, started by the link tick on its frame
		TestTrue(TEXT("the partner schedules it"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Partner, Entry, KeyFrame));
		TestEqual(TEXT("pending start frame"), Partner.Actions.PendingStart.Frame, Expected);
		int32 Started = INDEX_NONE;
		for (int32 Frame = KeyFrame + 1; Frame <= KeyFrame + 15; ++Frame)
		{
			const FGSTN_LinkTickExtension::FPartnerMoveResult Result = Step(Partner, Frame, Started);
			if (Result.StartAbility)
			{
				TestEqual(TEXT("started once"), Started, static_cast<int32>(INDEX_NONE));
				TestTrue(TEXT("the provider's ability"), Result.StartAbility == UObject::StaticClass());
				Started = Frame;
			}
		}
		TestEqual(FString::Printf(TEXT("offset %d: the part starts on its target frame"), Offsets[Index]), Started, Expected);
		TestTrue(TEXT("the partner records its side"), Partner.Actions.Active.Id == Ids[Index] && Partner.Actions.Active.StartFrame == Expected
			&& Partner.Actions.Active.Side == GSTNLinkActionSide::Partner && Partner.Actions.PendingStart.Id == 0);
		TestTrue(TEXT("its commands are locked while the part plays"), FGSTN_LinkTickExtension::IsCommandLocked(Partner, UGSTN_LinkRegistry::Get()));
		TestFalse(TEXT("the initiator's are not"), FGSTN_LinkTickExtension::IsCommandLocked(Initiator, UGSTN_LinkRegistry::Get()));

		// The part ends with its forced timeline: the record goes and the lock lifts
		Step(Partner, Expected + 20, INDEX_NONE);
		TestEqual(TEXT("part over"), static_cast<int32>(Partner.Actions.Active.Id), 0);
		TestFalse(TEXT("unlocked"), FGSTN_LinkTickExtension::IsCommandLocked(Partner, UGSTN_LinkRegistry::Get()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerMoveSerialTest, "GSTNLink.NP.PartnerMove.SerialMismatchDropped", Flags)
bool FGSTN_LinkPartnerMoveSerialTest::RunTest(const FString& Parameters)
{
	FScopedMoves Scope;
	FGSTN_LinkFragment Initiator = InitiatorSide(Scope);
	FGSTN_LinkPartnerMoveEntry Entry;
	FGSTN_LinkTickExtension::StartPartnerMove(Initiator, MoveOffset1, KeyFrame, Scope.Info(MoveOffset1), Entry);

	// The partner holds a newer link (serial 6): the entry is not about it
	FGSTN_LinkFragment Partner = MakeLinked(Scope.Types.LatchIndex, 2, InitiatorId, Serial + 1, LinkFrame + 50);
	const FGSTN_LinkFragment Before = Partner;
	TestFalse(TEXT("another serial: dropped"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Partner, Entry, KeyFrame));
	TestTrue(TEXT("nothing changed"), Partner == Before);

	FGSTN_LinkFragment Idle;
	TestFalse(TEXT("no link: dropped"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Idle, Entry, KeyFrame));
	TestTrue(TEXT("still idle"), Idle.IsDefault());

	// The right serial but requirements the partner does not meet: its part is dropped on its frame, the link kept, an abort reported
	FGSTN_LinkFragment Initiator2 = InitiatorSide(Scope);
	FGSTN_LinkPartnerMoveEntry NeedsTag;
	FGSTN_LinkTickExtension::StartPartnerMove(Initiator2, MoveNeedsTag, KeyFrame, Scope.Info(MoveNeedsTag), NeedsTag);
	FGSTN_LinkFragment Partner2 = PartnerSide(Scope);
	TestTrue(TEXT("scheduled"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Partner2, NeedsTag, KeyFrame));
	const FGSTN_LinkTickExtension::FPartnerMoveResult Missing = Step(Partner2, KeyFrame + 1);
	TestTrue(TEXT("requirements fail: no start"), Missing.StartAbility == nullptr && Missing.AbortedId == MoveNeedsTag);
	TestTrue(TEXT("the link stays"), Partner2.Links.Body.IsActive() && !Partner2.Links.Body.HasFlag(GSTNLinkFlags::PendingBreak));
	FGSTN_LinkFragment Partner3 = PartnerSide(Scope);
	FGSTN_LinkInboxHandlers::ApplyPartnerMove(Partner3, NeedsTag, KeyFrame);
	FGameplayTagContainer Tags;
	Tags.AddTag(GSTNLinkTags::Immunity_Latch);
	TestTrue(TEXT("requirements met: it starts"), Step(Partner3, KeyFrame + 1, INDEX_NONE, false, Tags).StartAbility != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerMoveOutcomeTest, "GSTNLink.NP.PartnerMove.OutcomeBreaksBothSides", Flags)
bool FGSTN_LinkPartnerMoveOutcomeTest::RunTest(const FString& Parameters)
{
	FScopedMoves Scope;
	const int32 TimelineStart = KeyFrame - 5;
	FGSTN_LinkFragment Initiator = InitiatorSide(Scope);
	FGSTN_LinkFragment Partner = PartnerSide(Scope);
	FGSTN_LinkPartnerMoveEntry Entry;
	FGSTN_LinkTickExtension::StartPartnerMove(Initiator, MoveBreak, KeyFrame, Scope.Info(MoveBreak), Entry);
	FGSTN_LinkInboxHandlers::ApplyPartnerMove(Partner, Entry, KeyFrame);
	Step(Partner, Entry.TargetFrame, INDEX_NONE);

	// Before the outcome frame nothing happens; the frame whose clock step crosses it marks the break with the action's reason
	TestFalse(TEXT("frame 13 -> 14: no outcome"), StepCrossing(Initiator, TimelineStart + 14, TimelineStart, OutcomeAt - 2, OutcomeAt - 1));
	TestTrue(TEXT("frame 14 -> 15: the outcome"), StepCrossing(Initiator, TimelineStart + 15, TimelineStart, OutcomeAt - 1, OutcomeAt));
	TestTrue(TEXT("pending break"), Initiator.Links.Body.HasFlag(GSTNLinkFlags::PendingBreak));
	TestEqual(TEXT("with the action's reason"), static_cast<int32>(Initiator.Links.Body.BreakReason), static_cast<int32>(EGSTN_LinkBreakReason::Escape));
	TestEqual(TEXT("the record stays for the exit"), static_cast<int32>(Initiator.Actions.Active.Id), static_cast<int32>(MoveBreak));

	// The watchdog breaks both sides on one frame; the parked side (the latch's initiator) leaves through the action's placer
	const FTransform PartnerTransform(FVector(1000.0, 0.0, 0.0));
	const FTransform Parked = UGSTN_LinkMath::ParkedTransform(PartnerTransform, Scope.Types.Latch->InitiatorRole);
	TArray<FGSTN_LinkWatchdogEntity> Entities = { WatchdogEntity(InitiatorId, Initiator, Parked), WatchdogEntity(PartnerId, Partner, PartnerTransform) };
	FGSTN_LinkWatchdogLogic::FParams Params;
	Params.Definitions = [](uint8 Index) { return UGSTN_LinkRegistry::Get()->GetDefinition(Index); };
	Params.ActionExitPlacer = [](uint8 TypeIndex, int16 Id) -> const UGSTN_LinkExitPlacer*
	{
		FGSTN_LinkActionInfo Info;
		return UGSTN_LinkRegistry::Get()->ResolveLinkAction(TypeIndex, Id, Info) ? Info.ExitPlacer : nullptr;
	};
	const int32 BreakFrame = TimelineStart + 15;
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, BreakFrame, Params, Posts, Breaks);
	TestEqual(TEXT("one break"), Breaks.Num(), 1);
	TestEqual(TEXT("two commands"), Posts.Num(), 2);
	const FGSTN_LinkWatchdogPost* ToInitiator = Posts.FindByPredicate([](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == InitiatorId; });
	if (TestNotNull(TEXT("a command to the parked initiator"), ToInitiator))
	{
		TestEqual(TEXT("reason Escape"), static_cast<int32>(ToInitiator->Command.Reason), static_cast<int32>(EGSTN_LinkBreakReason::Escape));
		const double Radius = FVector::Dist2D(FVector(ToInitiator->Command.ExitPosition), PartnerTransform.GetLocation());
		TestTrue(FString::Printf(TEXT("the action's placer (ring 300 cm, got %.1f)"), Radius), FMath::IsNearlyEqual(Radius, 300.0, 1.0));
	}

	// Both apply the command at BreakFrame + 1: both links end, and the partner move with them
	for (const FGSTN_LinkWatchdogPost& Post : Posts)
	{
		FGSTN_LinkFragment& Side = Post.TargetId == InitiatorId ? Initiator : Partner;
		TestTrue(TEXT("applied"), FGSTN_LinkInboxHandlers::ApplyCommand(Side, Post.Command, Post.StampFrame));
	}
	TestTrue(TEXT("initiator idle"), Initiator.Links.Body.Phase == EGSTN_LinkPhase::None && Initiator.Actions.Active.Id == 0);
	TestTrue(TEXT("partner idle"), Partner.Links.Body.Phase == EGSTN_LinkPhase::None && Partner.Actions.Active.Id == 0 && Partner.Actions.PendingStart.Id == 0);

	// Without a move playing, the same break leaves through the type's own placer (the ring's default 120 cm)
	FGSTN_LinkFragment Plain = InitiatorSide(Scope);
	Plain.Links.Body.Flags |= GSTNLinkFlags::PendingBreak;
	TArray<FGSTN_LinkWatchdogEntity> PlainEntities = { WatchdogEntity(InitiatorId, Plain, Parked), WatchdogEntity(PartnerId, PartnerSide(Scope), PartnerTransform) };
	Posts.Reset();
	Breaks.Reset();
	FGSTN_LinkWatchdogLogic::Evaluate(PlainEntities, BreakFrame, Params, Posts, Breaks);
	const FGSTN_LinkWatchdogPost* PlainExit = Posts.FindByPredicate([](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == InitiatorId; });
	if (TestNotNull(TEXT("a plain break"), PlainExit))
	{
		const double Radius = FVector::Dist2D(FVector(PlainExit->Command.ExitPosition), PartnerTransform.GetLocation());
		TestTrue(FString::Printf(TEXT("the type's placer (ring 120 cm, got %.1f)"), Radius), FMath::IsNearlyEqual(Radius, 120.0, 1.0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerMoveRollbackTest, "GSTNLink.NP.PartnerMove.RollbackTrimsEntries", Flags)
bool FGSTN_LinkPartnerMoveRollbackTest::RunTest(const FString& Parameters)
{
	FScopedMoves Scope;
	FGSTN_LinkFragment Initiator = InitiatorSide(Scope);
	FGSTN_LinkPartnerMoveEntry Move;
	FGSTN_LinkTickExtension::StartPartnerMove(Initiator, MoveOffset12, KeyFrame, Scope.Info(MoveOffset12), Move);

	FGSTN_InboxEntry Entry = FGSTN_LinkInboxHandlers::MakePartnerMove(Move);
	Entry.Frame = KeyFrame;
	Entry.SenderId = InitiatorId;
	FGSTN_CombatInbox Inbox;
	Inbox.Add(Entry);
	// A rollback that re-simulates the key's frame drops the entry: the initiator's re-simulation posts it again
	Inbox.TrimFrom(KeyFrame);
	TestEqual(TEXT("trimmed"), Inbox.Num(), 0);
	Inbox.Add(Entry);
	TArray<FGSTN_InboxEntry> Drained;
	Inbox.Drain(KeyFrame, Drained);
	TestEqual(TEXT("not delivered on its own frame"), Drained.Num(), 0);
	Inbox.Drain(KeyFrame + 1, Drained);
	TestEqual(TEXT("delivered at S + 1"), Drained.Num(), 1);
	const FGSTN_LinkPartnerMoveEntry* Payload = Drained.Num() == 1 ? Drained[0].Payload.GetPtr<FGSTN_LinkPartnerMoveEntry>() : nullptr;
	if (!TestNotNull(TEXT("the payload"), Payload))
	{
		return true;
	}

	// The partner's state is restored by the rollback; the re-posted entry lands on the same state, and a second arrival changes nothing
	const FGSTN_LinkFragment Restored = PartnerSide(Scope);
	FGSTN_LinkFragment First = Restored;
	TestTrue(TEXT("first pass schedules"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(First, *Payload, Drained[0].Frame));
	FGSTN_LinkFragment Resimulated = Restored;
	TestTrue(TEXT("re-simulation schedules"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Resimulated, *Payload, Drained[0].Frame));
	TestTrue(TEXT("same state"), First == Resimulated);
	TestFalse(TEXT("a repeated entry is idempotent"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Resimulated, *Payload, Drained[0].Frame));

	// Once the part plays, the entry still changes nothing (Active matches it)
	Step(Resimulated, Move.TargetFrame, INDEX_NONE);
	TestFalse(TEXT("idempotent while playing"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Resimulated, *Payload, Drained[0].Frame));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerMoveKOTest, "GSTNLink.NP.PartnerMove.AbortOnPartnerKO", Flags)
bool FGSTN_LinkPartnerMoveKOTest::RunTest(const FString& Parameters)
{
	FScopedMoves Scope;
	FGSTN_LinkFragment Initiator = InitiatorSide(Scope);
	FGSTN_LinkPartnerMoveEntry Move;
	FGSTN_LinkTickExtension::StartPartnerMove(Initiator, MoveOffset1, KeyFrame, Scope.Info(MoveOffset1), Move);

	// Knocked out when its part is due: aborted, the link breaks (Forced, the type's default exit), no part starts
	FGSTN_LinkFragment Partner = PartnerSide(Scope);
	FGSTN_LinkInboxHandlers::ApplyPartnerMove(Partner, Move, KeyFrame);
	const FGSTN_LinkTickExtension::FPartnerMoveResult KO = Step(Partner, Move.TargetFrame, INDEX_NONE, true);
	TestTrue(TEXT("no start"), KO.StartAbility == nullptr);
	TestEqual(TEXT("abort reported"), static_cast<int32>(KO.AbortedId), static_cast<int32>(MoveOffset1));
	TestTrue(TEXT("pending break, Forced"), Partner.Links.Body.HasFlag(GSTNLinkFlags::PendingBreak)
		&& Partner.Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::Forced);
	TestTrue(TEXT("no record left (default exit)"), Partner.Actions.Active.Id == 0 && Partner.Actions.PendingStart.Id == 0);

	// Knocked out while playing: the same
	FGSTN_LinkFragment Playing = PartnerSide(Scope);
	FGSTN_LinkInboxHandlers::ApplyPartnerMove(Playing, Move, KeyFrame);
	Step(Playing, Move.TargetFrame, INDEX_NONE);
	TestEqual(TEXT("playing"), static_cast<int32>(Playing.Actions.Active.Id), static_cast<int32>(MoveOffset1));
	TestEqual(TEXT("KO mid-part aborts"), static_cast<int32>(Step(Playing, Move.TargetFrame + 1, Move.TargetFrame, true).AbortedId), static_cast<int32>(MoveOffset1));
	TestTrue(TEXT("mid-part: pending break"), Playing.Links.Body.HasFlag(GSTNLinkFlags::PendingBreak) && Playing.Actions.Active.Id == 0);

	// MaxDurationFrames: an initiator whose move still plays 20 frames on aborts too
	FGSTN_LinkFragment Long = InitiatorSide(Scope);
	FGSTN_LinkPartnerMoveEntry LongMove;
	FGSTN_LinkTickExtension::StartPartnerMove(Long, MoveLimited, KeyFrame, Scope.Info(MoveLimited), LongMove);
	TestEqual(TEXT("19 frames: still playing"), static_cast<int32>(Step(Long, KeyFrame + 19, KeyFrame - 1).AbortedId), 0);
	TestEqual(TEXT("20 frames: aborted"), static_cast<int32>(Step(Long, KeyFrame + 20, KeyFrame - 1).AbortedId), static_cast<int32>(MoveLimited));
	TestTrue(TEXT("the link breaks"), Long.Links.Body.HasFlag(GSTNLinkFlags::PendingBreak) && Long.Actions.Active.Id == 0);
	return true;
}

// ---- Parked facing

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFacingLimitTest, "GSTNLink.NP.Facing.StickWithinLimit", Flags)
bool FGSTN_LinkFacingLimitTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	Types.Latch->InitiatorRole.YawLimitDeg = 90.f;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, PartnerId, Serial, LinkFrame);
	const FTransform Anchor = FTransform::Identity;

	TestTrue(TEXT("idle stick to the side: turns"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, Anchor, FVector(0.0, 1.0, 0.0), true, false));
	TestEqual(TEXT("clamped to half the 90 degree arc"), static_cast<int32>(Latcher.Links.Body.LocalYawQ), static_cast<int32>(UGSTN_LinkMath::LocalYawQFromDeg(45.0)));
	TestEqual(TEXT("45 degrees = 4096"), static_cast<int32>(Latcher.Links.Body.LocalYawQ), 4096);
	const FVector Thirty(FMath::Cos(FMath::DegreesToRadians(-30.0)), FMath::Sin(FMath::DegreesToRadians(-30.0)), 0.0);
	FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, Anchor, Thirty, true, false);
	TestEqual(TEXT("inside the arc: the stick's direction"), static_cast<int32>(Latcher.Links.Body.LocalYawQ), static_cast<int32>(UGSTN_LinkMath::LocalYawQFromDeg(-30.0)));
	TestEqual(TEXT("in whole quanta"), static_cast<int32>(Latcher.Links.Body.LocalYawQ) % UGSTN_LinkMath::LocalYawQuantum, 0);

	const int16 Kept = Latcher.Links.Body.LocalYawQ;
	TestFalse(TEXT("a stick below a quarter tilt keeps the turn"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, Anchor, FVector(0.1, 0.1, 0.0), true, false));
	TestFalse(TEXT("busy and no move starting: kept"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, Anchor, FVector(0.0, 1.0, 0.0), false, false));
	TestEqual(TEXT("kept"), static_cast<int32>(Latcher.Links.Body.LocalYawQ), static_cast<int32>(Kept));

	// Relative to the anchor: a placement turned 90 degrees faces the same world direction with no turn
	const FTransform Turned(FRotator(0.0, 90.0, 0.0));
	FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, Turned, FVector(0.0, 1.0, 0.0), true, false);
	TestEqual(TEXT("anchor-relative"), static_cast<int32>(Latcher.Links.Body.LocalYawQ), 0);

	// Limit 0 never turns
	Types.Latch->InitiatorRole.YawLimitDeg = 0.f;
	TestFalse(TEXT("limit 0: fixed"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, Anchor, FVector(0.0, 1.0, 0.0), true, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFacingAimedTest, "GSTNLink.NP.Facing.AimedMoveSetsYaw", Flags)
bool FGSTN_LinkFacingAimedTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, PartnerId, Serial, LinkFrame);

	// The frame a move starts aims it (the default arc is free: straight back is 180 degrees, the wrap's -180)
	TestTrue(TEXT("a move starting aims"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, FTransform::Identity, FVector(-1.0, 0.0, 0.0), false, true));
	TestEqual(TEXT("behind the anchor"), static_cast<int32>(Latcher.Links.Body.LocalYawQ), -16384);
	TestFalse(TEXT("the frames after the start do not re-aim"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, FTransform::Identity, FVector(1.0, 0.0, 0.0), false, false));
	TestEqual(TEXT("kept during the move"), static_cast<int32>(Latcher.Links.Body.LocalYawQ), -16384);

	// The snapshot substitution composes the turn: the parked entry faces where the move aimed
	const FTransform PartnerTransform(FVector(500.0, 0.0, 0.0));
	const FGSTN_LinkFragment Partner = MakeLinked(Types.LatchIndex, 2, InitiatorId, Serial, LinkFrame);
	TArray<FGSTN_SnapshotEntry> Entries = { MakeEntry(InitiatorId, FTransform::Identity, 0, &Latcher), MakeEntry(PartnerId, PartnerTransform, 0, &Partner) };
	FGSTN_LinkSnapshotContributor Contributor;
	Contributor.FinalizeSnapshot(LinkFrame + 10, Entries);
	const FTransform Parked = UGSTN_LinkMath::ParkedTransform(PartnerTransform, Types.Latch->InitiatorRole);
	TestTrue(TEXT("same place"), Entries[0].Transform.GetLocation().Equals(Parked.GetLocation(), 0.01));
	TestTrue(TEXT("turned half a turn"), Entries[0].Facing.Equals(-Parked.GetRotation().GetForwardVector(), 0.001));

	// A parked role that does not act (the grabbed) never turns
	FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, InitiatorId, Serial, LinkFrame);
	TestFalse(TEXT("the grabbed does not turn"), FGSTN_LinkTickExtension::StepFacing(Grabbed, Registry, FTransform::Identity, FVector(0.0, 1.0, 0.0), true, true));
	// Nor does a role that is not parked (the latched enemy)
	FGSTN_LinkFragment Latched = Partner;
	TestFalse(TEXT("an unparked role does not turn"), FGSTN_LinkTickExtension::StepFacing(Latched, Registry, FTransform::Identity, FVector(0.0, 1.0, 0.0), true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFacingLatchTest, "GSTNLink.NP.Facing.LatchUnchanged", Flags)
bool FGSTN_LinkFacingLatchTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedLinkTypes Types;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	TestEqual(TEXT("the default arc is free"), Types.Latch->InitiatorRole.YawLimitDeg, 360.f);

	// The latch at its defaults, a latcher that never pushes the stick: no turn, the same state and the same bytes as before G2
	FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, PartnerId, Serial, LinkFrame);
	const FGSTN_LinkFragment Before = Latcher;
	TestFalse(TEXT("idle, no stick"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, FTransform::Identity, FVector::ZeroVector, true, false));
	TestFalse(TEXT("its attack, no stick"), FGSTN_LinkTickExtension::StepFacing(Latcher, Registry, FTransform::Identity, FVector::ZeroVector, false, true));
	TestTrue(TEXT("unchanged"), Latcher == Before);
	TestEqual(TEXT("same payload bytes"), PayloadBytes(Latcher), PayloadBytes(Before));

	// The substitution with no turn is the placement of F10 exactly
	const FTransform PartnerTransform(FRotator(0.0, 30.0, 0.0), FVector(200.0, 100.0, 0.0));
	const FGSTN_LinkFragment Partner = MakeLinked(Types.LatchIndex, 2, InitiatorId, Serial, LinkFrame);
	TArray<FGSTN_SnapshotEntry> Entries = { MakeEntry(InitiatorId, FTransform::Identity, 0, &Latcher), MakeEntry(PartnerId, PartnerTransform, 0, &Partner) };
	FTransform Expected;
	const FGSTN_LinkSnapshotExt* Ext = Entries[0].Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
	TestTrue(TEXT("parked"), Ext && FGSTN_LinkSnapshotContributor::ComputeParkedTransform(*Ext, Entries[1], Expected));
	FGSTN_LinkSnapshotContributor Contributor;
	Contributor.FinalizeSnapshot(LinkFrame + 1, Entries);
	TestTrue(TEXT("the F10 placement"), Entries[0].Transform.Equals(Expected, 0.0001));
	TestEqual(TEXT("LocalYawQ 0 on the wire"), Ext ? static_cast<int32>(Ext->LocalYawQ8) : -1, 0);
	return true;
}

// ---- Wire forms

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkG2RoundTripTest, "GSTNLink.NP.Fragment.PartnerMoveAndFacingRoundTrip", Flags)
bool FGSTN_LinkG2RoundTripTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedLinkTypes Types;

	// A turned latcher playing a partner's part with another scheduled: every new field round-trips through the fragment list
	FGSTN_LinkFragment Fragment = MakeLinked(Types.LatchIndex, 1, PartnerId, Serial, LinkFrame);
	Fragment.Links.Body.LocalYawQ = UGSTN_LinkMath::LocalYawQFromDeg(-67.5);
	Fragment.Actions.Active.Id = 3;
	Fragment.Actions.Active.StartFrame = 210;
	Fragment.Actions.Active.Side = GSTNLinkActionSide::Partner;
	Fragment.Actions.PendingStart.Frame = 230;
	Fragment.Actions.PendingStart.Id = 4;
	FGSTN_SyncFragmentArray List = ListOf(Fragment);
	bool bError = false;
	FGSTN_SyncFragmentArray Loaded = ReadBack(List, bError);
	TestFalse(TEXT("reads back cleanly"), bError);
	const FGSTN_LinkFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Loaded);
	TestTrue(TEXT("equal after the round trip"), Read && *Read == Fragment);
	FGSTN_LinkFragment OtherSide = Fragment;
	OtherSide.Actions.Active.Side = GSTNLinkActionSide::Initiator;
	TestFalse(TEXT("the side is compared"), OtherSide == Fragment);
	TestTrue(TEXT("a side difference reconciles"), OtherSide.ShouldReconcile(Fragment));

	// The snapshot byte holds every turn the simulation writes exactly, and the extension still fits its blob
	static_assert(sizeof(FGSTN_LinkSnapshotExt) <= 32, "the link snapshot extension must fit the snapshot blob");
	int32 Mismatches = 0;
	for (int32 Q = -16384; Q < 16384; Q += UGSTN_LinkMath::LocalYawQuantum)
	{
		FGSTN_LinkFragment Turned = MakeLinked(Types.LatchIndex, 1, PartnerId, Serial, LinkFrame);
		Turned.Links.Body.LocalYawQ = static_cast<int16>(Q);
		FGSTN_LinkSnapshotExt Ext;
		Ext.SetFromFragment(Turned);
		Mismatches += Ext.ToLinkStates().Body.LocalYawQ == Q ? 0 : 1;
	}
	TestEqual(TEXT("the yaw byte is exact for all 256 quanta"), Mismatches, 0);
	TestEqual(TEXT("190 degrees wraps to -170"), static_cast<int32>(UGSTN_LinkMath::LocalYawQFromDeg(190.0)), static_cast<int32>(UGSTN_LinkMath::LocalYawQFromDeg(-170.0)));
	TestEqual(TEXT("180 is -180"), static_cast<int32>(UGSTN_LinkMath::LocalYawQFromDeg(180.0)), -16384);

	// The partner-move entry survives the inbox's instanced payload
	FGSTN_LinkPartnerMoveEntry Move;
	Move.Serial = 9;
	Move.Id = 1234;
	Move.TargetFrame = 777;
	const FGSTN_InboxEntry Entry = FGSTN_LinkInboxHandlers::MakePartnerMove(Move);
	const FGSTN_LinkPartnerMoveEntry* Payload = Entry.Payload.GetPtr<FGSTN_LinkPartnerMoveEntry>();
	TestTrue(TEXT("kind and payload"), Entry.Kind == GSTNLinkInboxKinds::PartnerMove && Payload && Payload->Serial == 9 && Payload->Id == 1234 && Payload->TargetFrame == 777);
	return true;
}

}

#endif

// Automation tests of the hit bounce (owner 2026-11-23, the Cr.L Aerial stomp): the fragment on the wire, the arm / fire / lock rules, the limit
// and its resets, and a scripted run through the hooks with a rollback. No world: the extension's Mover reads and its launch are replaced.

#include "Misc/AutomationTest.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_HitBounceSim.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNHitBounceNPTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** The module registers the fragment at startup; a test re-registers in case an earlier test unregistered it */
	struct FScopedFragment
	{
		FScopedFragment() { FGSTN_HitBounceFragmentRegistration::Register(); }
		~FScopedFragment() { FGSTN_HitBounceFragmentRegistration::Register(); }
	};

	TArray<uint8> Payload(const FGSTN_HitBounceFragment& Fragment)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		const_cast<FGSTN_HitBounceFragment&>(Fragment).NetSerialize(Writer);
		return Bytes;
	}

	/** The extension with the world parts replaced: a Mover facing +X under 2940 cm/s^2, the launches recorded, the data's numbers */
	class FTestExtension : public FGSTN_HitBounceTickExtension
	{
	public:
		mutable TArray<TPair<int32, FGSTN_HitBounceLaunch>> Launches;
		FGSTN_HitBounceOverrides Overrides;

	protected:
		virtual FGSTN_HitBounceMoverFrame ReadMoverFrame(const FGSTN_CombatTickContext& Context) const override
		{
			FGSTN_HitBounceMoverFrame Frame;
			Frame.Facing = FVector(1, 0, 0);
			Frame.Gravity = 2940.0;
			Frame.bValid = true;
			return Frame;
		}

		virtual void ApplyLaunch(FGSTN_CombatTickContext& Context, const FGSTN_HitBounceLaunch& Launch) const override
		{
			Launches.Emplace(Context.Frame, Launch);
		}

		virtual FGSTN_HitBounceOverrides GetOverrides() const override { return Overrides; }
	};

	/** A dair-like move: 43 frames, the hit bounce armed by hits on timeline frames 15 to 42 */
	FGSTN_CombatTimelineData MakeMove()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 43;
		FGSTN_TimelineTrack_HitBounce Track;
		Track.StartFrame = 15;
		Track.EndFrame = 43;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HitBounce>(Track));
		return Data;
	}

	FGSTN_AttackerHit MakeHit(bool bBlocked = false)
	{
		FGSTN_AttackerHit Hit;
		Hit.VictimId = 7;
		Hit.bBlocked = bBlocked;
		return Hit;
	}

	/**
	 * A small stand-in for the combat tick around the extension: the state a frame writes starts as the previous one, AfterClocks, then (with
	 * Hits) AfterExecute. The hitstop window and the timeline state are written by the script, as the attacker step and the clock would
	 */
	struct FRun
	{
		FTestExtension& Extension;
		const FGSTN_CombatTimelineData& Move;
		FGSTN_CombatSyncState State;
		bool bGrounded = false;
		bool bLastBlocked = false;

		FRun(FTestExtension& InExtension, const FGSTN_CombatTimelineData& InMove) : Extension(InExtension), Move(InMove) {}

		void StartMove(int32 Frame)
		{
			State.Core.ActionState = EGSTN_ActionState::InTimeline;
			State.Core.ActiveTimeline = 0;
			State.Core.TimelineStartFrame = Frame;
			State.Core.TimelinePausedFrames = 0;
			State.Core.RecoilStartFrame = INDEX_NONE;
		}

		/** One frame; with Hits the attacker step of the frame hit (its hitstop window written as the attacker step does) */
		void Tick(int32 Frame, TArray<FGSTN_AttackerHit> Hits = {}, int32 HitstopFrames = 8)
		{
			const FGSTN_CombatSyncState In = State;
			FGSTN_CombatSyncState& Out = State;
			FGSTN_CombatTickContext Context(Frame, In, Out);
			Context.bGrounded = bGrounded;
			Context.RunningTimeline = Out.Core.ActionState == EGSTN_ActionState::InTimeline ? &Move : nullptr;
			// The hitstop frames stand the timeline still, as the clock does
			if (Out.Core.ActionState == EGSTN_ActionState::InTimeline && Out.Core.IsInHitstop(Frame))
			{
				++Out.Core.TimelinePausedFrames;
			}
			Extension.OnCombatHook(EGSTN_CombatHookPoint::AfterClocks, Context);
			// No services in a test: the model's cancel of the move is played here
			if (const FGSTN_HitBounceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_HitBounceFragment>(Out.Fragments); Fragment && Fragment->LaunchFrame == Frame)
			{
				Out.Core.ActionState = EGSTN_ActionState::Actionable;
				Out.Core.ActiveTimeline = INDEX_NONE;
				Out.Core.TimelineStartFrame = INDEX_NONE;
			}
			if (Hits.Num() > 0 && Out.Core.ActionState == EGSTN_ActionState::InTimeline)
			{
				Out.Core.HitstopStartFrame = Frame;
				Out.Core.HitstopEndFrame = Frame + HitstopFrames;
				Context.AttackerHits = Hits;
				Extension.OnCombatHook(EGSTN_CombatHookPoint::AfterExecute, Context);
			}
		}

		const FGSTN_HitBounceFragment* Fragment() const { return FGSTN_SyncFragmentList::Find<FGSTN_HitBounceFragment>(State.Fragments); }

		bool Blocks(int32 Frame) const
		{
			FGSTN_CombatTickContext Context(Frame, State, const_cast<FGSTN_CombatSyncState&>(State));
			return Extension.BlocksActions(Context);
		}
	};

	TArray<uint8> Bytes(FGSTN_CombatSyncState& State)
	{
		FBitWriter Writer(1024, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		TArray<uint8> Result;
		Result.Append(Writer.GetData(), Writer.GetNumBytes());
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitBounceFragmentWireTest, "GSTNCombat.NP.HitBounce.Wire", GSTNHitBounceNPTest::Flags)
bool FGSTN_HitBounceFragmentWireTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitBounceNPTest;
	FScopedFragment Scope;
	TestEqual(TEXT("registered under its feature id"), static_cast<int32>(FGSTN_SyncFragmentRegistry::FindId(FGSTN_HitBounceFragment::StaticStruct())), static_cast<int32>(GSTNFeatureIds::HitBounce));
	TestEqual(TEXT("a default fragment is zero bytes"), Payload(FGSTN_HitBounceFragment()).Num(), 0);

	FGSTN_HitBounceFragment Live;
	Live.Count = 2;
	Live.ArmedFrame = 120;
	Live.ArmedTimelineStart = 100;
	Live.LaunchFrame = 131;
	Live.LockFrames = 24;
	const TArray<uint8> Bytes = Payload(Live);
	TestEqual(TEXT("mask + count + armed + launch"), Bytes.Num(), 1 + 1 + 8 + 5);
	FGSTN_HitBounceFragment Read;
	FMemoryReader Reader(Bytes);
	Read.NetSerialize(Reader);
	TestTrue(TEXT("round trip"), Read == Live);
	TestFalse(TEXT("equal: no reconcile"), Read.ShouldReconcile(Live));
	Read.Count = 1;
	TestTrue(TEXT("a different count reconciles"), Read.ShouldReconcile(Live));

	FGSTN_HitBounceFragment CountOnly;
	CountOnly.Count = 1;
	TestEqual(TEXT("only the count: mask + 1 byte"), Payload(CountOnly).Num(), 2);

	TArray<uint8> Corrupt = { 0x80 };
	FMemoryReader Bad(Corrupt);
	FGSTN_HitBounceFragment Junk;
	Junk.NetSerialize(Bad);
	TestTrue(TEXT("an unknown presence bit is an error"), Bad.IsError());
	TestTrue(TEXT("and leaves the default"), Junk.IsDefault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitBounceRulesTest, "GSTNCombat.NP.HitBounce.Rules", GSTNHitBounceNPTest::Flags)
bool FGSTN_HitBounceRulesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitBounceNPTest;
	using EArmed = FGSTN_HitBounceTickExtension::EArmedAction;
	const FGSTN_CombatTimelineData Move = MakeMove();
	const FGSTN_TimelineTrack_HitBounce& Track = *FGSTN_HitBounceRules::FindTrack(Move);
	const FGSTN_HitBounceTuning Tuning = FGSTN_HitBounceRules::Resolve(Track, FGSTN_HitBounceOverrides());

	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.TimelineStartFrame = 100;
	FGSTN_HitBounceFragment Fragment;
	TestFalse(TEXT("before the window: no arm"), FGSTN_HitBounceTickExtension::TryArm(Fragment, Core, 110, Track, Tuning, true));
	TestFalse(TEXT("a whiff: no arm"), FGSTN_HitBounceTickExtension::TryArm(Fragment, Core, 116, Track, Tuning, false));
	TestFalse(TEXT("a timeline started on the hit's frame did not make the hit"), FGSTN_HitBounceTickExtension::TryArm(Fragment, Core, 100, Track, Tuning, true));
	TestTrue(TEXT("a hit on timeline frame 16 arms"), FGSTN_HitBounceTickExtension::TryArm(Fragment, Core, 116, Track, Tuning, true));
	TestEqual(TEXT("the hit's frame"), Fragment.ArmedFrame, 116);

	Core.HitstopStartFrame = 116;
	Core.HitstopEndFrame = 124;
	TestTrue(TEXT("in the hitstop: wait"), FGSTN_HitBounceTickExtension::ResolveArmed(Fragment, Core, 120) == EArmed::Wait);
	TestTrue(TEXT("the frame the freeze finishes: wait"), FGSTN_HitBounceTickExtension::ResolveArmed(Fragment, Core, 124) == EArmed::Wait);
	TestTrue(TEXT("the frame after: fire"), FGSTN_HitBounceTickExtension::ResolveArmed(Fragment, Core, 125) == EArmed::Fire);
	FGSTN_CombatSyncCore Replaced = Core;
	Replaced.TimelineStartFrame = 122;
	TestTrue(TEXT("another move replaced it: disarm"), FGSTN_HitBounceTickExtension::ResolveArmed(Fragment, Replaced, 125) == EArmed::Disarm);
	FGSTN_CombatSyncCore NoHitstop = Core;
	NoHitstop.HitstopEndFrame = 50;
	TestTrue(TEXT("no hitstop: fires the frame after the hit"), FGSTN_HitBounceTickExtension::ResolveArmed(Fragment, NoHitstop, 117) == EArmed::Fire);

	FGSTN_HitBounceTickExtension::MarkLaunched(Fragment, 125, Tuning);
	TestEqual(TEXT("one bounce"), static_cast<int32>(Fragment.Count), 1);
	TestFalse(TEXT("no longer armed"), Fragment.IsArmed());
	TestTrue(TEXT("locked on the launch frame"), Fragment.IsLocked(125));
	TestTrue(TEXT("locked on its 24th frame"), Fragment.IsLocked(148));
	TestFalse(TEXT("free 24 frames after the launch"), Fragment.IsLocked(149));

	FGSTN_HitBounceFragment Ground = Fragment;
	FGSTN_HitBounceTickExtension::StepBookkeeping(Ground, 126, false, true);
	TestEqual(TEXT("the ground the bounce left from does not reset it"), static_cast<int32>(Ground.Count), 1);
	FGSTN_HitBounceTickExtension::StepBookkeeping(Ground, 140, false, true);
	TestEqual(TEXT("landing resets the count"), static_cast<int32>(Ground.Count), 0);
	TestTrue(TEXT("landing keeps the lock (like a landing lag)"), Ground.IsLocked(140));
	FGSTN_HitBounceTickExtension::StepBookkeeping(Ground, 149, false, true);
	TestTrue(TEXT("the lock that ran out is cleared: nothing left"), Ground.IsDefault());

	FGSTN_HitBounceFragment Hit = Fragment;
	FGSTN_HitBounceTickExtension::StepBookkeeping(Hit, 130, true, false);
	TestTrue(TEXT("being hit resets everything"), Hit.IsDefault());

	TArray<FGSTN_AttackerHit> Hits = { MakeHit(true), MakeHit(false) };
	TestEqual(TEXT("the shielded hit is skipped, the clean one bounces"), FGSTN_HitBounceTickExtension::FindBouncingHit(Hits, Tuning), 1);
	Hits.RemoveAt(1);
	TestEqual(TEXT("only a shielded hit: no bounce"), FGSTN_HitBounceTickExtension::FindBouncingHit(Hits, Tuning), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitBounceRunTest, "GSTNCombat.NP.HitBounce.Run", GSTNHitBounceNPTest::Flags)
bool FGSTN_HitBounceRunTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitBounceNPTest;
	FScopedFragment Scope;
	FTestExtension Extension;
	const FGSTN_CombatTimelineData Move = MakeMove();
	FRun Run(Extension, Move);

	// Bounce 1: the move starts at 85, its frame 15 hits at 100, 8 frames of hitstop, the launch one frame after the window
	Run.StartMove(85);
	for (int32 Frame = 86; Frame < 100; ++Frame)
	{
		Run.Tick(Frame);
	}
	Run.Tick(100, { MakeHit() });
	TestTrue(TEXT("armed by the hit"), Run.Fragment() && Run.Fragment()->IsArmed());
	FGSTN_CombatSyncState Saved;
	for (int32 Frame = 101; Frame <= 109; ++Frame)
	{
		if (Frame == 104)
		{
			Saved = Run.State;
		}
		Run.Tick(Frame);
	}
	TestEqual(TEXT("one launch"), Extension.Launches.Num(), 1);
	TestEqual(TEXT("on frame 109 (window [100, 108), the freeze finishes on 108)"), Extension.Launches.Num() ? Extension.Launches[0].Key : -1, 109);
	TestTrue(TEXT("up and back"), Extension.Launches.Num() && Extension.Launches[0].Value.Velocity.Z > 1000.0 && Extension.Launches[0].Value.Velocity.X < 0.0);
	TestEqual(TEXT("count 1"), Run.Fragment() ? static_cast<int32>(Run.Fragment()->Count) : -1, 1);
	TestTrue(TEXT("no command on the launch frame"), Run.Blocks(109));
	TestTrue(TEXT("none on frame 132"), Run.Blocks(132));
	TestFalse(TEXT("free on frame 133 (24 frames)"), Run.Blocks(133));

	// Rollback: the saved state (end of frame 103) re-simulated from 104 to 109 ends in the same bytes
	const TArray<uint8> Original = Bytes(Run.State);
	const int32 LaunchesBefore = Extension.Launches.Num();
	Run.State = Saved;
	for (int32 Frame = 104; Frame <= 109; ++Frame)
	{
		Run.Tick(Frame);
	}
	TestTrue(TEXT("re-simulation gives the same state"), Bytes(Run.State) == Original);
	TestEqual(TEXT("and launches again on the same frame (Mover is rolled back with it)"), Extension.Launches.Num(), LaunchesBefore + 1);

	// Bounce 2 in the air: a new Cr.L Aerial at 140 hits at 156
	for (int32 Frame = 110; Frame < 140; ++Frame)
	{
		Run.Tick(Frame);
	}
	Run.StartMove(140);
	for (int32 Frame = 141; Frame < 156; ++Frame)
	{
		Run.Tick(Frame);
	}
	Run.Tick(156, { MakeHit() });
	for (int32 Frame = 157; Frame <= 165; ++Frame)
	{
		Run.Tick(Frame);
	}
	TestEqual(TEXT("count 2"), Run.Fragment() ? static_cast<int32>(Run.Fragment()->Count) : -1, 2);

	// The third in a row is a normal move: nothing armed, no launch, the move goes on
	for (int32 Frame = 166; Frame < 190; ++Frame)
	{
		Run.Tick(Frame);
	}
	Run.StartMove(190);
	const int32 LaunchesAtLimit = Extension.Launches.Num();
	for (int32 Frame = 191; Frame < 206; ++Frame)
	{
		Run.Tick(Frame);
	}
	Run.Tick(206, { MakeHit() });
	for (int32 Frame = 207; Frame <= 220; ++Frame)
	{
		Run.Tick(Frame);
	}
	TestEqual(TEXT("at the limit: no launch"), Extension.Launches.Num(), LaunchesAtLimit);
	TestTrue(TEXT("the move still runs"), Run.State.Core.ActionState == EGSTN_ActionState::InTimeline);

	// The owner raises the limit by hand to 3: the same hit now bounces
	Extension.Overrides.MaxConsecutiveBounces = 3.f;
	Run.StartMove(221);
	for (int32 Frame = 222; Frame < 237; ++Frame)
	{
		Run.Tick(Frame);
	}
	Run.Tick(237, { MakeHit() });
	for (int32 Frame = 238; Frame <= 246; ++Frame)
	{
		Run.Tick(Frame);
	}
	TestEqual(TEXT("limit 3: a third bounce"), Extension.Launches.Num(), LaunchesAtLimit + 1);
	Extension.Overrides = FGSTN_HitBounceOverrides();

	// Landing resets the count and the fragment goes away once the lock is over
	Run.bGrounded = true;
	for (int32 Frame = 247; Frame <= 275; ++Frame)
	{
		Run.Tick(Frame);
	}
	TestNull(TEXT("on the ground after the lock: no fragment left"), Run.Fragment());

	// A shielded hit does not bounce and leaves no fragment
	Run.bGrounded = false;
	Run.StartMove(280);
	for (int32 Frame = 281; Frame < 296; ++Frame)
	{
		Run.Tick(Frame);
	}
	Run.Tick(296, { MakeHit(true) });
	TestNull(TEXT("shield: nothing armed"), Run.Fragment());
	return true;
}

#endif

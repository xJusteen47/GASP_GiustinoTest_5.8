// Automation tests of the drill fragment and tick extension (plan D3): registration under the feature id, the wire (an absent fragment
// is one bit, an idle one only the list framing, active ones within the documented bytes), the phase machine from scripted inputs,
// rollback (a restored older state re-simulated gives the same frames) and the projection of the Combat.State.* tags.

#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_SpinDrillTags.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_DrillTickExtension.h"
#include "Simulation/GSTN_CombatSimTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogGSTNSpinDrillNPTest, Log, All);

namespace GSTNDrillNPTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** The list framing of one present fragment: any-present bit, count, type id, payload length */
	constexpr int64 FramingBits = 1 + 8 + 8 + 8;

	/** The module registers the fragment at startup; a test re-registers in case an earlier test unregistered it */
	struct FScopedFragment
	{
		FScopedFragment() { FGSTN_DrillFragmentRegistration::Register(); }
		~FScopedFragment() { FGSTN_DrillFragmentRegistration::Register(); }
	};

	int32 PayloadBytes(const FGSTN_DrillFragment& Fragment)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		const_cast<FGSTN_DrillFragment&>(Fragment).NetSerialize(Writer);
		return Bytes.Num();
	}

	FGSTN_SyncFragmentArray ListOf(const FGSTN_DrillFragment& Fragment)
	{
		FGSTN_SyncFragmentArray List;
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DrillFragment>(List) = Fragment;
		return List;
	}

	int64 BitsOfList(FGSTN_SyncFragmentArray& Fragments)
	{
		FBitWriter Writer(1024, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		return Writer.GetNumBits();
	}

	FGSTN_SyncFragmentArray ReadBack(FGSTN_SyncFragmentArray& Fragments, bool& bOutError)
	{
		FBitWriter Writer(1024, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_SyncFragmentArray Loaded;
		FGSTN_SyncFragmentList::NetSerialize(Reader, Loaded);
		bOutError = Reader.IsError() || !Reader.AtEnd();
		return Loaded;
	}

	FGSTN_DrillFragment GroundSpin()
	{
		FGSTN_DrillFragment Fragment;
		Fragment.Phase = EGSTN_DrillPhase::Spinning;
		Fragment.PhaseStartFrame = 100;
		return Fragment;
	}

	FGSTN_DrillFragment AirSpinCcw()
	{
		FGSTN_DrillFragment Fragment = GroundSpin();
		Fragment.Flags = GSTNDrillFlags::CounterClockwise | GSTNDrillFlags::AirSpin;
		Fragment.AirSpinsUsed = 1;
		Fragment.LowGravityUntilFrame = 130;
		return Fragment;
	}

	FGSTN_DrillFragment LatchedAfterAirSpin()
	{
		FGSTN_DrillFragment Fragment = AirSpinCcw();
		Fragment.Phase = EGSTN_DrillPhase::Boring;
		Fragment.Flags = GSTNDrillFlags::CounterClockwise;
		Fragment.PhaseStartFrame = 120;
		Fragment.LatchFrame = 110;
		Fragment.SeatFrames = 10;
		Fragment.MaxBoreFrames = 90;
		Fragment.HitsLanded = 3;
		return Fragment;
	}

	FGSTN_DrillFrameInputs Request(EGSTN_DrillRequest What, bool bClockwise = true)
	{
		FGSTN_DrillFrameInputs Inputs;
		Inputs.Request = What;
		Inputs.bClockwise = bClockwise;
		return Inputs;
	}

	/** Both stepping hooks of one frame on a fragment, as the extension runs them */
	void StepFrame(FGSTN_DrillFragment& Fragment, int32 Frame, const FGSTN_DrillFrameInputs& Inputs, const FGSTN_DrillSimTuning& Tuning)
	{
		FGSTN_DrillTickExtension::StepAfterClocks(Fragment, Frame, Inputs, Tuning);
		FGSTN_DrillTickExtension::StepAfterExecute(Fragment, Frame, Inputs, Tuning);
	}

	/** The extension with the frame's inputs taken from a script instead of the (not yet built) abilities */
	class FScriptedDrillExtension : public FGSTN_DrillTickExtension
	{
	public:
		TMap<int32, FGSTN_DrillFrameInputs> Script;

	protected:
		virtual FGSTN_DrillFrameInputs GatherInputs(EGSTN_CombatHookPoint Point, const FGSTN_CombatTickContext& Context) const override
		{
			const FGSTN_DrillFrameInputs* Found = Script.Find(Context.Frame);
			return Found ? *Found : FGSTN_DrillFrameInputs();
		}
	};

	/** One combat tick of the extension on a sync state: the four hook points in order */
	void RunTick(FGSTN_DrillTickExtension& Extension, FGSTN_CombatSyncState& State, int32 Frame, UAbilitySystemComponent* ASC = nullptr)
	{
		const FGSTN_CombatSyncState In = State;
		FGSTN_CombatSyncState Out = State;
		FGSTN_CombatTickContext Context(Frame, In, Out);
		Context.AbilitySystem = ASC;
		for (const EGSTN_CombatHookPoint Point : { EGSTN_CombatHookPoint::AfterClocks, EGSTN_CombatHookPoint::BeforeCandidates,
			EGSTN_CombatHookPoint::AfterExecute, EGSTN_CombatHookPoint::BeforeTagProjection })
		{
			Extension.OnCombatHook(Point, Context);
		}
		State = Out;
	}

	FGSTN_DrillFragment ReadFragment(const FGSTN_CombatSyncState& State)
	{
		const FGSTN_DrillFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(State.Fragments);
		return Fragment ? *Fragment : FGSTN_DrillFragment();
	}

	/** A transient game world with one actor owning a stock ability system component: loose tags need no more (as the defence tests) */
	struct FRig
	{
		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNSpinDrillNPTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Owner = World->SpawnActor<AActor>();
			ASC = NewObject<UAbilitySystemComponent>(Owner);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Owner, Owner);
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
	};
}

using namespace GSTNDrillNPTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPRegisteredTest, "GSTNSpinDrill.NP.Fragment.Registered", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPRegisteredTest::RunTest(const FString& Parameters)
{
	FScopedFragment Registered;
	TestEqual(TEXT("the spin drill feature id"), static_cast<int32>(GSTNFeatureIds::SpinDrill), 19);
	TestTrue(TEXT("the module registered the fragment under it"), FGSTN_SyncFragmentRegistry::FindStruct(GSTNFeatureIds::SpinDrill) == FGSTN_DrillFragment::StaticStruct());
	TestEqual(TEXT("the tick extension shares the id"), static_cast<int32>(FGSTN_DrillTickExtension().GetTypeId()), static_cast<int32>(GSTNFeatureIds::SpinDrill));
	TestTrue(TEXT("and runs after the links"), FGSTN_DrillTickExtension::ExtensionPriority > 120);
	TestTrue(TEXT("the signature names the fragment"), FGSTN_SyncFragmentRegistry::Describe().Contains(TEXT("GSTN_DrillFragment")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPWireTest, "GSTNSpinDrill.NP.Fragment.RoundTripAndBits", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPWireTest::RunTest(const FString& Parameters)
{
	FScopedFragment Registered;

	// An entity that never spun has no fragment: the list is the one flag bit, as without the plugin
	FGSTN_SyncFragmentArray None;
	const int64 NoneBits = BitsOfList(None);
	TestEqual(TEXT("no fragment: one bit"), NoneBits, static_cast<int64>(1));

	// A fragment at its defaults writes no payload: only the framing
	const FGSTN_DrillFragment Idle;
	TestTrue(TEXT("a fresh fragment is the default"), Idle.IsDefault());
	TestEqual(TEXT("idle payload is empty"), PayloadBytes(Idle), 0);
	FGSTN_SyncFragmentArray IdleList = ListOf(Idle);
	const int64 IdleBits = BitsOfList(IdleList);
	TestEqual(TEXT("idle: the framing only"), IdleBits, FramingBits);

	struct FCase
	{
		const TCHAR* Name;
		FGSTN_DrillFragment Fragment;
		int32 ExpectedBytes;
	};
	FGSTN_DrillFragment GroundLatched = LatchedAfterAirSpin();
	GroundLatched.AirSpinsUsed = 0;
	GroundLatched.LowGravityUntilFrame = INDEX_NONE;
	const FCase Cases[] = {
		{ TEXT("ground spin"), GroundSpin(), 1 + 6 },
		{ TEXT("air spin"), AirSpinCcw(), 1 + 6 + 5 },
		{ TEXT("latched after a ground spin"), GroundLatched, 1 + 6 + 9 },
		{ TEXT("latched after an air spin"), LatchedAfterAirSpin(), 1 + 6 + 5 + 9 },
	};
	UE_LOG(LogGSTNSpinDrillNPTest, Display, TEXT("Fragment bits: none %lld, idle %lld (0 B payload)"), NoneBits, IdleBits);
	for (const FCase& Case : Cases)
	{
		const int32 Bytes = PayloadBytes(Case.Fragment);
		TestEqual(*FString::Printf(TEXT("%s payload bytes"), Case.Name), Bytes, Case.ExpectedBytes);
		FGSTN_SyncFragmentArray List = ListOf(Case.Fragment);
		const int64 Bits = BitsOfList(List);
		TestEqual(*FString::Printf(TEXT("%s list bits"), Case.Name), Bits, FramingBits + 8 * Bytes);
		UE_LOG(LogGSTNSpinDrillNPTest, Display, TEXT("Fragment bytes: %s %d B payload, %lld bits with framing"), Case.Name, Bytes, Bits);

		bool bError = false;
		const FGSTN_SyncFragmentArray Loaded = ReadBack(List, bError);
		TestFalse(*FString::Printf(TEXT("%s reads back without error"), Case.Name), bError);
		const FGSTN_DrillFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Loaded);
		TestTrue(*FString::Printf(TEXT("%s arrives equal"), Case.Name), Read && *Read == Case.Fragment);
		TestFalse(*FString::Printf(TEXT("%s does not reconcile"), Case.Name), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, List));
	}

	// Every field reconciles exactly
	FGSTN_DrillFragment Changed = LatchedAfterAirSpin();
	Changed.HitsLanded = 4;
	TestTrue(TEXT("one hit apart reconciles"), Changed.ShouldReconcile(LatchedAfterAirSpin()));
	TestFalse(TEXT("equal does not"), LatchedAfterAirSpin().ShouldReconcile(LatchedAfterAirSpin()));

	// A foreign or corrupt payload is refused and leaves the default
	for (const uint8 BadMask : { static_cast<uint8>(0), static_cast<uint8>(1 << 5) })
	{
		TArray<uint8> Bytes = { BadMask, 0, 0, 0, 0, 0, 0 };
		FMemoryReader Reader(Bytes);
		FGSTN_DrillFragment Read = LatchedAfterAirSpin();
		Read.NetSerialize(Reader);
		TestTrue(*FString::Printf(TEXT("mask 0x%X is an error"), BadMask), Reader.IsError());
		TestTrue(*FString::Printf(TEXT("mask 0x%X leaves the default"), BadMask), Read.IsDefault());
	}
	TArray<uint8> BadPhase = { 1, 200, 0, 100, 0, 0, 0 };
	FMemoryReader PhaseReader(BadPhase);
	FGSTN_DrillFragment ReadPhase;
	ReadPhase.NetSerialize(PhaseReader);
	TestTrue(TEXT("an unknown phase is an error"), PhaseReader.IsError() && ReadPhase.IsDefault());

	// Reading nothing into a live fragment clears it
	FGSTN_DrillFragment Live = LatchedAfterAirSpin();
	TArray<uint8> NoBytes;
	FMemoryReader Empty(NoBytes);
	Live.NetSerialize(Empty);
	TestTrue(TEXT("an empty payload gives the default"), !Empty.IsError() && Live.IsDefault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPPhasesTest, "GSTNSpinDrill.NP.Tick.PhaseTransitions", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPPhasesTest::RunTest(const FString& Parameters)
{
	const FGSTN_DrillSimTuning Tuning = FGSTN_DrillSimTuning::MakeDefault();
	const auto PhaseIs = [this](const TCHAR* What, const FGSTN_DrillFragment& Fragment, EGSTN_DrillPhase Expected)
	{
		TestEqual(What, static_cast<int32>(Fragment.Phase), static_cast<int32>(Expected));
	};

	// Spin and cancel: only the opposite sense, only from CancelFromFrame (6) on; the fragment is the default again after
	FGSTN_DrillFragment F;
	StepFrame(F, 100, Request(EGSTN_DrillRequest::Spin, true), Tuning);
	PhaseIs(TEXT("ground spin"), F, EGSTN_DrillPhase::Spinning);
	TestTrue(TEXT("clockwise"), F.IsClockwise());
	StepFrame(F, 110, Request(EGSTN_DrillRequest::Cancel, true), Tuning);
	PhaseIs(TEXT("the same sense does not cancel"), F, EGSTN_DrillPhase::Spinning);
	StepFrame(F, 105, Request(EGSTN_DrillRequest::Cancel, false), Tuning);
	PhaseIs(TEXT("the opposite sense before frame 6 does not cancel"), F, EGSTN_DrillPhase::Spinning);
	StepFrame(F, 106, Request(EGSTN_DrillRequest::Cancel, false), Tuning);
	PhaseIs(TEXT("the opposite sense from frame 6 cancels"), F, EGSTN_DrillPhase::None);
	TestTrue(TEXT("a cancelled ground spin leaves the default"), F.IsDefault());

	// Air spin: once per airtime, low gravity for the spin plus the extra frames, given back by a landing
	FGSTN_DrillFrameInputs Air = Request(EGSTN_DrillRequest::AirSpin, false);
	Air.SpinFrames = 20;
	StepFrame(F, 200, Air, Tuning);
	PhaseIs(TEXT("air spin"), F, EGSTN_DrillPhase::Spinning);
	TestTrue(TEXT("counter-clockwise air spin"), !F.IsClockwise() && (F.Flags & GSTNDrillFlags::AirSpin) != 0);
	TestEqual(TEXT("one air spin used"), static_cast<int32>(F.AirSpinsUsed), 1);
	TestEqual(TEXT("low gravity until the spin's end"), F.LowGravityUntilFrame, 220);
	FGSTN_DrillFrameInputs Ended;
	Ended.bPhaseTimelineEnded = true;
	StepFrame(F, 220, Ended, Tuning);
	PhaseIs(TEXT("the spin's timeline ended"), F, EGSTN_DrillPhase::None);
	TestEqual(TEXT("low gravity cleared once passed"), F.LowGravityUntilFrame, static_cast<int32>(INDEX_NONE));
	StepFrame(F, 225, Air, Tuning);
	PhaseIs(TEXT("a second air spin in the same airtime is refused"), F, EGSTN_DrillPhase::None);
	FGSTN_DrillFrameInputs Hit;
	Hit.RefreshEvents = static_cast<int32>(EGSTN_AirSpinRefresh::Hit);
	StepFrame(F, 226, Hit, Tuning);
	TestEqual(TEXT("a hit does not give it back by default"), static_cast<int32>(F.AirSpinsUsed), 1);
	FGSTN_DrillFrameInputs Land;
	Land.RefreshEvents = static_cast<int32>(EGSTN_AirSpinRefresh::Land);
	StepFrame(F, 230, Land, Tuning);
	TestEqual(TEXT("landing gives it back"), static_cast<int32>(F.AirSpinsUsed), 0);
	TestTrue(TEXT("and the fragment is the default again"), F.IsDefault());

	// Dive, latch (not before MinDiveFramesBeforeLatch = 2), seat, bore, hop out on release
	StepFrame(F, 300, Request(EGSTN_DrillRequest::Spin, false), Tuning);
	StepFrame(F, 302, Request(EGSTN_DrillRequest::Dive), Tuning);
	PhaseIs(TEXT("dive"), F, EGSTN_DrillPhase::Diving);
	TestFalse(TEXT("the dive keeps the spin's sense"), F.IsClockwise());
	FGSTN_DrillFrameInputs Latch = Request(EGSTN_DrillRequest::Latch);
	Latch.LatchSeatFrames = 6;
	Latch.LatchMaxBoreFrames = 20;
	StepFrame(F, 303, Latch, Tuning);
	PhaseIs(TEXT("no latch on the dive's second frame"), F, EGSTN_DrillPhase::Diving);
	StepFrame(F, 304, Latch, Tuning);
	PhaseIs(TEXT("latched"), F, EGSTN_DrillPhase::Seating);
	TestEqual(TEXT("the latch frame is the next frame"), F.LatchFrame, 305);
	TestTrue(TEXT("seat and maximum stored"), F.SeatFrames == 6 && F.MaxBoreFrames == 20);
	StepFrame(F, 310, FGSTN_DrillFrameInputs(), Tuning);
	PhaseIs(TEXT("still seating at frame 5 of 6"), F, EGSTN_DrillPhase::Seating);
	FGSTN_DrillFrameInputs Hitting;
	Hitting.bHitLanded = true;
	StepFrame(F, 311, Hitting, Tuning);
	PhaseIs(TEXT("boring after the seat"), F, EGSTN_DrillPhase::Boring);
	StepFrame(F, 317, Hitting, Tuning);
	TestEqual(TEXT("two hits counted"), static_cast<int32>(F.HitsLanded), 2);
	FGSTN_DrillFrameInputs Release;
	Release.bCrouchHeld = false;
	FGSTN_DrillFragment Popped = F;
	StepFrame(Popped, 318, Release, Tuning);
	PhaseIs(TEXT("release without a jump: hop out"), Popped, EGSTN_DrillPhase::None);
	TestTrue(TEXT("and the latch is cleared"), Popped.IsDefault());

	// The other ends from the same bore
	FGSTN_DrillFrameInputs Boost = Release;
	Boost.bJumpBoost = true;
	FGSTN_DrillFragment Launched = F;
	StepFrame(Launched, 318, Boost, Tuning);
	PhaseIs(TEXT("release with a jump: launch"), Launched, EGSTN_DrillPhase::Launching);
	TestEqual(TEXT("the launch clears the latch"), Launched.LatchFrame, static_cast<int32>(INDEX_NONE));
	FGSTN_DrillFragment MaxedOut = F;
	StepFrame(MaxedOut, 325, FGSTN_DrillFrameInputs(), Tuning);
	PhaseIs(TEXT("maximum reached without a jump: hop out"), MaxedOut, EGSTN_DrillPhase::None);
	FGSTN_DrillFragment Gone = F;
	FGSTN_DrillFrameInputs TargetGone;
	TargetGone.bTargetGone = true;
	StepFrame(Gone, 318, TargetGone, Tuning);
	PhaseIs(TEXT("target gone: final jump"), Gone, EGSTN_DrillPhase::Launching);
	FGSTN_DrillSimTuning Plunging = Tuning;
	Plunging.bContinueThroughDestroyed = true;
	FGSTN_DrillFragment Through = F;
	StepFrame(Through, 318, TargetGone, Plunging);
	PhaseIs(TEXT("target gone, keep plunging"), Through, EGSTN_DrillPhase::Diving);
	FGSTN_DrillFragment Staggered = F;
	FGSTN_DrillFrameInputs Stagger = Boost;
	Stagger.bDrillerInHitstun = true;
	StepFrame(Staggered, 318, Stagger, Tuning);
	PhaseIs(TEXT("hitstun wins over a boosted release"), Staggered, EGSTN_DrillPhase::None);

	// A launch after an air spin gives the air spin back (the launch tuning refreshes by default)
	FGSTN_DrillFragment AirLaunch = F;
	AirLaunch.AirSpinsUsed = 1;
	StepFrame(AirLaunch, 318, Boost, Tuning);
	TestEqual(TEXT("launch refreshes the air spin"), static_cast<int32>(AirLaunch.AirSpinsUsed), 0);

	// Dive endings without a latch: bounce (no refresh) and ground lag
	FGSTN_DrillFragment Bounced;
	StepFrame(Bounced, 400, Air, Tuning);
	StepFrame(Bounced, 402, Request(EGSTN_DrillRequest::Dive), Tuning);
	StepFrame(Bounced, 405, Request(EGSTN_DrillRequest::Bounce), Tuning);
	PhaseIs(TEXT("bounce ends the dive"), Bounced, EGSTN_DrillPhase::None);
	TestEqual(TEXT("a bounce does not refresh the air spin"), static_cast<int32>(Bounced.AirSpinsUsed), 1);
	FGSTN_DrillFragment Grounded;
	StepFrame(Grounded, 500, Request(EGSTN_DrillRequest::Spin), Tuning);
	StepFrame(Grounded, 501, Request(EGSTN_DrillRequest::Dive), Tuning);
	StepFrame(Grounded, 504, Request(EGSTN_DrillRequest::GroundLand), Tuning);
	PhaseIs(TEXT("ground lag"), Grounded, EGSTN_DrillPhase::Lag);
	StepFrame(Grounded, 522, Ended, Tuning);
	PhaseIs(TEXT("lag ends with its timeline"), Grounded, EGSTN_DrillPhase::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPRollbackTest, "GSTNSpinDrill.NP.Tick.RollbackResimulates", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPRollbackTest::RunTest(const FString& Parameters)
{
	FScopedFragment Registered;
	FScriptedDrillExtension Extension;
	FGSTN_DrillFrameInputs Air = Request(EGSTN_DrillRequest::AirSpin, false);
	Air.SpinFrames = 10;
	FGSTN_DrillFrameInputs Latch = Request(EGSTN_DrillRequest::Latch);
	Latch.LatchSeatFrames = 4;
	Latch.LatchMaxBoreFrames = 30;
	FGSTN_DrillFrameInputs Hitting;
	Hitting.bHitLanded = true;
	FGSTN_DrillFrameInputs Boost;
	Boost.bCrouchHeld = false;
	Boost.bJumpBoost = true;
	FGSTN_DrillFrameInputs Ended;
	Ended.bPhaseTimelineEnded = true;
	FGSTN_DrillFrameInputs Land;
	Land.RefreshEvents = static_cast<int32>(EGSTN_AirSpinRefresh::Land);
	Extension.Script.Add(2, Air);
	Extension.Script.Add(5, Request(EGSTN_DrillRequest::Dive));
	Extension.Script.Add(8, Latch);
	Extension.Script.Add(14, Hitting);
	Extension.Script.Add(20, Hitting);
	Extension.Script.Add(24, Boost);
	Extension.Script.Add(30, Ended);
	Extension.Script.Add(36, Land);

	// The first run, every frame's state kept as the Network Prediction history would
	constexpr int32 LastFrame = 40;
	FGSTN_CombatSyncState State;
	TArray<FGSTN_CombatSyncState> History;
	for (int32 Frame = 0; Frame <= LastFrame; ++Frame)
	{
		RunTick(Extension, State, Frame);
		History.Add(State);
	}
	TestTrue(TEXT("no fragment before the first request"), FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(History[1].Fragments) == nullptr);
	TestEqual(TEXT("air spin at frame 2"), static_cast<int32>(ReadFragment(History[2]).Phase), static_cast<int32>(EGSTN_DrillPhase::Spinning));
	TestEqual(TEXT("boring at frame 20"), static_cast<int32>(ReadFragment(History[20]).Phase), static_cast<int32>(EGSTN_DrillPhase::Boring));
	TestEqual(TEXT("two hits by frame 20"), static_cast<int32>(ReadFragment(History[20]).HitsLanded), 2);
	TestEqual(TEXT("launching at frame 24"), static_cast<int32>(ReadFragment(History[24]).Phase), static_cast<int32>(EGSTN_DrillPhase::Launching));
	TestTrue(TEXT("default again after the landing"), ReadFragment(History[LastFrame]).IsDefault());

	// Rollback: restore every earlier frame in turn and re-simulate to the end; each frame must come out the same
	for (const int32 Restore : { 1, 4, 9, 15, 23, 31 })
	{
		FGSTN_CombatSyncState Replayed = History[Restore];
		bool bSame = true;
		for (int32 Frame = Restore + 1; Frame <= LastFrame; ++Frame)
		{
			RunTick(Extension, Replayed, Frame);
			bSame &= !FGSTN_SyncFragmentList::ShouldReconcile(Replayed.Fragments, History[Frame].Fragments)
				&& ReadFragment(Replayed) == ReadFragment(History[Frame]);
		}
		TestTrue(*FString::Printf(TEXT("restored at frame %d, every later frame is the same"), Restore), bSame);
	}

	// The wire carries it: a state serialized at the latch reads back equal
	bool bError = false;
	const FGSTN_SyncFragmentArray Loaded = ReadBack(History[12].Fragments, bError);
	TestTrue(TEXT("the latched state survives the wire"), !bError && !FGSTN_SyncFragmentList::ShouldReconcile(Loaded, History[12].Fragments));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPTagsTest, "GSTNSpinDrill.NP.Tick.TagProjection", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPTagsTest::RunTest(const FString& Parameters)
{
	FScopedFragment Registered;
	const FGSTN_DrillSimTuning Tuning = FGSTN_DrillSimTuning::MakeDefault();
	const auto Tags = [&Tuning](const FGSTN_DrillFragment& Fragment)
	{
		FGameplayTagContainer Result;
		FGSTN_DrillTickExtension::GatherStateTags(&Fragment, Tuning, Result);
		return Result;
	};

	FGameplayTagContainer Ground = Tags(GroundSpin());
	TestTrue(TEXT("ground spin: Spinning and Clockwise only"), Ground.Num() == 2 && Ground.HasTagExact(GSTNSpinDrillTags::State_Spinning)
		&& Ground.HasTagExact(GSTNSpinDrillTags::State_Spinning_Clockwise));
	FGameplayTagContainer AirTags = Tags(AirSpinCcw());
	TestTrue(TEXT("air spin: CounterClockwise and AirSpinSpent"), AirTags.HasTagExact(GSTNSpinDrillTags::State_Spinning_CounterClockwise)
		&& AirTags.HasTagExact(GSTNSpinDrillTags::State_AirSpinSpent) && !AirTags.HasTagExact(GSTNSpinDrillTags::State_Spinning_Clockwise));
	FGSTN_DrillFragment Diving = GroundSpin();
	Diving.Phase = EGSTN_DrillPhase::Diving;
	FGameplayTagContainer DiveTags = Tags(Diving);
	TestTrue(TEXT("dive: Drilling only"), DiveTags.Num() == 1 && DiveTags.HasTagExact(GSTNSpinDrillTags::State_Drilling));
	FGameplayTagContainer Latched = Tags(LatchedAfterAirSpin());
	TestTrue(TEXT("bore: Drilling and DrillLatched, no Spinning"), Latched.HasTagExact(GSTNSpinDrillTags::State_Drilling)
		&& Latched.HasTagExact(GSTNSpinDrillTags::State_DrillLatched) && !Latched.HasTag(GSTNSpinDrillTags::State_Spinning));
	FGameplayTagContainer NoneTags;
	FGSTN_DrillTickExtension::GatherStateTags(nullptr, Tuning, NoneTags);
	TestEqual(TEXT("no fragment, no tags"), NoneTags.Num(), 0);

	// On a real ability system: set from the fragment, cleared when it ends and after a rollback to before the fragment existed
	FRig Rig;
	FGSTN_DrillTickExtension Extension;
	FGSTN_CombatSyncState State;
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DrillFragment>(State.Fragments) = AirSpinCcw();
	RunTick(Extension, State, 105, Rig.ASC);
	TestTrue(TEXT("projected: Spinning.CounterClockwise"), Rig.ASC->HasMatchingGameplayTag(GSTNSpinDrillTags::State_Spinning_CounterClockwise));
	TestFalse(TEXT("not Clockwise"), Rig.ASC->HasMatchingGameplayTag(GSTNSpinDrillTags::State_Spinning_Clockwise));
	// The tag map counts a parent once more for each child it holds, so compare frames instead of a fixed number
	const int32 SpinningAfterOne = Rig.ASC->GetTagCount(GSTNSpinDrillTags::State_Spinning);
	RunTick(Extension, State, 106, Rig.ASC);
	TestEqual(TEXT("set, not added: the leaf stays at one"), Rig.ASC->GetTagCount(GSTNSpinDrillTags::State_Spinning_CounterClockwise), 1);
	TestEqual(TEXT("set, not added: the parent does not grow"), Rig.ASC->GetTagCount(GSTNSpinDrillTags::State_Spinning), SpinningAfterOne);

	FGSTN_CombatSyncState BeforeAnySpin;
	RunTick(Extension, BeforeAnySpin, 104, Rig.ASC);
	TestFalse(TEXT("a rollback to before the fragment clears Spinning"), Rig.ASC->HasMatchingGameplayTag(GSTNSpinDrillTags::State_Spinning));
	TestFalse(TEXT("and AirSpinSpent"), Rig.ASC->HasMatchingGameplayTag(GSTNSpinDrillTags::State_AirSpinSpent));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

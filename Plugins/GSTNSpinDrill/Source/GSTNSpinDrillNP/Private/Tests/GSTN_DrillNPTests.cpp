// Automation tests of the drill fragment and tick extension (plan D3): registration under the feature id, the wire (an absent fragment
// is one bit, an idle one only the list framing, active ones within the documented bytes), the phase machine from scripted inputs,
// rollback (a restored older state re-simulated gives the same frames) and the projection of the Combat.State.* tags.

#include "AbilitySystemComponent.h"
#include "Data/GSTN_SpinDrillTracks.h"
#include "Data/GSTN_SpinTimelines.h"
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
#include "Sim/GSTN_LinkFragment.h"
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
	TestTrue(TEXT("and the latch is cleared, the pop flagged for its frame"), Popped.LatchFrame == INDEX_NONE && Popped.Flags == GSTNDrillFlags::Popped);
	FGSTN_DrillTickExtension::StepAfterClocks(Popped, 319, FGSTN_DrillFrameInputs(), Tuning);
	TestTrue(TEXT("the next frame: the default fragment again"), Popped.IsDefault());

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
	// The spin's timeline runs and the entity is airborne, or the real extension would end the spin and give the air spin back
	State.Core.ActionState = EGSTN_ActionState::InTimeline;
	State.Core.ActiveTimeline = 0;
	State.Core.TimelineStartFrame = AirSpinCcw().PhaseStartFrame;
	State.Core.Jump.Phase = EGSTN_JumpPhase::Falling;
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

namespace GSTNDrillNPTest
{
	/** The real extension (its own GatherInputs) with the started timeline taken from the test instead of a combat component */
	class FTimelineDrillExtension : public FGSTN_DrillTickExtension
	{
	public:
		FGSTN_CombatTimelineData Started;

	protected:
		virtual const FGSTN_CombatTimelineData* ResolveStartedTimeline(const FGSTN_CombatTickContext& Context) const override
		{
			const FGSTN_CombatSyncCore& Core = Context.Out.Core;
			return Core.ActionState == EGSTN_ActionState::InTimeline && Core.TimelineStartFrame == Context.Frame ? &Started : nullptr;
		}
	};

	/** The combat state of a timeline running since StartFrame */
	void RunTimeline(FGSTN_CombatSyncState& State, int32 StartFrame)
	{
		State.Core.ActionState = EGSTN_ActionState::InTimeline;
		State.Core.ActiveTimeline = 0;
		State.Core.TimelineStartFrame = StartFrame;
	}

	void StopTimeline(FGSTN_CombatSyncState& State)
	{
		State.Core.ActionState = EGSTN_ActionState::Actionable;
		State.Core.ActiveTimeline = INDEX_NONE;
		State.Core.TimelineStartFrame = INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPSpinReadTest, "GSTNSpinDrill.NP.Spin.ReadsTheCombatState", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPSpinReadTest::RunTest(const FString& Parameters)
{
	// The pure reads: the end of a spin's timeline, the request of a spin track, the low gravity window
	FGSTN_CombatSyncCore Core;
	FGSTN_DrillFragment Spinning = GroundSpin();
	TestTrue(TEXT("no timeline: the spin ended"), FGSTN_DrillTickExtension::HasSpinTimelineEnded(Spinning, Core));
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.TimelineStartFrame = Spinning.PhaseStartFrame;
	TestFalse(TEXT("its own timeline runs: not ended"), FGSTN_DrillTickExtension::HasSpinTimelineEnded(Spinning, Core));
	Core.TimelineStartFrame = Spinning.PhaseStartFrame + 3;
	TestTrue(TEXT("another timeline replaced it: ended"), FGSTN_DrillTickExtension::HasSpinTimelineEnded(Spinning, Core));
	FGSTN_DrillFragment Diving = Spinning;
	Diving.Phase = EGSTN_DrillPhase::Diving;
	TestFalse(TEXT("only a spin ends this way"), FGSTN_DrillTickExtension::HasSpinTimelineEnded(Diving, Core));

	FGSTN_SpinTrack Track;
	Track.EndFrame = 36;
	Track.Sense = EGSTN_SpinSense::CounterClockwise;
	Track.bAirSpin = true;
	FGSTN_DrillFrameInputs Inputs;
	FGSTN_DrillTickExtension::ReadSpinStart(Track, Inputs);
	TestTrue(TEXT("an air track asks for an air spin, counter-clockwise, 36 frames"), Inputs.Request == EGSTN_DrillRequest::AirSpin && !Inputs.bClockwise && Inputs.SpinFrames == 36);
	Track.bAirSpin = false;
	FGSTN_DrillTickExtension::ReadSpinStart(Track, Inputs);
	TestTrue(TEXT("a ground track asks for a spin"), Inputs.Request == EGSTN_DrillRequest::Spin);

	const FGSTN_DrillFragment Air = AirSpinCcw();
	TestFalse(TEXT("no fragment: no low gravity"), FGSTN_DrillTickExtension::WantsLowGravity(nullptr, 0));
	TestEqual(TEXT("the window is the fragment's"), FGSTN_DrillTickExtension::WantsLowGravity(&Air, Air.PhaseStartFrame), Air.IsLowGravity(Air.PhaseStartFrame));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPSpinFlowTest, "GSTNSpinDrill.NP.Spin.AirSpinOncePerAirtime", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPSpinFlowTest::RunTest(const FString& Parameters)
{
	FScopedFragment Registered;
	FTimelineDrillExtension Extension;
	Extension.Started = FGSTN_SpinTimelines::Make(FGSTN_SpinTimelines::AirParams(), EGSTN_SpinSense::Clockwise, true);
	const int32 SpinFrames = Extension.Started.TotalFrames;

	// Airborne: the jump state says falling, so nothing refreshes
	FGSTN_CombatSyncState State;
	State.Core.Jump.Phase = EGSTN_JumpPhase::Falling;
	RunTick(Extension, State, 9);
	TestNull(TEXT("no spin, no fragment"), FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(State.Fragments));

	// Frame 10: the air spin's timeline starts
	RunTimeline(State, 10);
	RunTick(Extension, State, 10);
	FGSTN_DrillFragment F = ReadFragment(State);
	TestEqual(TEXT("spinning from frame 10"), static_cast<int32>(F.Phase), static_cast<int32>(EGSTN_DrillPhase::Spinning));
	TestEqual(TEXT("the phase starts with the timeline"), F.PhaseStartFrame, 10);
	TestTrue(TEXT("clockwise air spin"), F.IsClockwise() && (F.Flags & GSTNDrillFlags::AirSpin) != 0);
	TestEqual(TEXT("one air spin used"), static_cast<int32>(F.AirSpinsUsed), 1);
	TestEqual(TEXT("low gravity to the end of the spin track"), F.LowGravityUntilFrame, 10 + SpinFrames);

	// It keeps spinning while its timeline runs, and ends with it
	RunTick(Extension, State, 11);
	TestEqual(TEXT("still spinning at frame 11"), static_cast<int32>(ReadFragment(State).Phase), static_cast<int32>(EGSTN_DrillPhase::Spinning));
	StopTimeline(State);
	RunTick(Extension, State, 12);
	F = ReadFragment(State);
	TestEqual(TEXT("the timeline ended: no phase"), static_cast<int32>(F.Phase), static_cast<int32>(EGSTN_DrillPhase::None));
	TestEqual(TEXT("still one air spin used in this airtime"), static_cast<int32>(F.AirSpinsUsed), 1);

	// A second air spin in the same airtime is refused, even if its timeline started (the kit binding is the first gate)
	RunTimeline(State, 20);
	RunTick(Extension, State, 20);
	TestEqual(TEXT("refused: no phase"), static_cast<int32>(ReadFragment(State).Phase), static_cast<int32>(EGSTN_DrillPhase::None));
	StopTimeline(State);

	// A trampoline launch gives it back once
	State.Core.Jump.Phase = EGSTN_JumpPhase::Launched;
	RunTick(Extension, State, 30);
	// RunTick hands the same jump state as In and Out: the launch edge needs the previous frame to differ
	TestEqual(TEXT("still launched is not a new launch"), static_cast<int32>(ReadFragment(State).AirSpinsUsed), 1);

	// Landing after the low gravity window (it ends at 10 + 36) gives it back and the fragment is the default again
	State.Core.Jump.Phase = EGSTN_JumpPhase::None;
	RunTick(Extension, State, 60);
	F = ReadFragment(State);
	TestEqual(TEXT("landing gives the air spin back"), static_cast<int32>(F.AirSpinsUsed), 0);
	TestTrue(TEXT("and the fragment is the default"), F.IsDefault());

	// A ground spin uses nothing and has no low gravity
	FTimelineDrillExtension Ground;
	Ground.Started = FGSTN_SpinTimelines::Make(FGSTN_SpinTimelines::GroundParams(), EGSTN_SpinSense::CounterClockwise, false);
	RunTimeline(State, 70);
	RunTick(Ground, State, 70);
	F = ReadFragment(State);
	TestTrue(TEXT("ground spin, counter-clockwise, no air spin used, no low gravity"), F.IsSpinning() && !F.IsClockwise() && F.AirSpinsUsed == 0
		&& F.LowGravityUntilFrame == INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPDiveTest, "GSTNSpinDrill.NP.Dive.LagAndBounce", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPDiveTest::RunTest(const FString& Parameters)
{
	FScopedFragment Registered;
	FTimelineDrillExtension Extension;
	const FGSTN_CombatTimelineData AirCw = FGSTN_SpinTimelines::Make(FGSTN_SpinTimelines::AirParams(), EGSTN_SpinSense::Clockwise, true);
	const FGSTN_CombatTimelineData Dive = FGSTN_SpinTimelines::MakeDive(FGSTN_DiveTimelineParams());
	const auto Phase = [](const FGSTN_CombatSyncState& State) { return ReadFragment(State).Phase; };

	// An air spin, then crouch on frame 14: the dive's timeline replaces the spin's and the fragment dives
	FGSTN_CombatSyncState State;
	State.Core.Jump.Phase = EGSTN_JumpPhase::Falling;
	Extension.Started = AirCw;
	RunTimeline(State, 10);
	RunTick(Extension, State, 10);
	Extension.Started = Dive;
	RunTimeline(State, 14);
	RunTick(Extension, State, 14);
	FGSTN_DrillFragment F = ReadFragment(State);
	TestTrue(TEXT("diving from frame 14, the spin's sense kept"), F.Phase == EGSTN_DrillPhase::Diving && F.PhaseStartFrame == 14 && F.IsClockwise());
	TestEqual(TEXT("the air spin's low gravity ended with the dive"), F.LowGravityUntilFrame, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("the dive gravity is wanted while diving"), FGSTN_DrillTickExtension::WantsDiveGravity(&F));
	TestEqual(TEXT("the air spin stays spent"), static_cast<int32>(F.AirSpinsUsed), 1);

	// Still in the air: the dive goes on; the ground: the lag, which lasts as long as the dive's timeline
	RunTick(Extension, State, 15);
	TestTrue(TEXT("airborne: still diving"), Phase(State) == EGSTN_DrillPhase::Diving);
	State.Core.Jump.Phase = EGSTN_JumpPhase::None;
	RunTick(Extension, State, 20);
	F = ReadFragment(State);
	TestTrue(TEXT("on the ground: the lag from frame 20"), F.Phase == EGSTN_DrillPhase::Lag && F.PhaseStartFrame == 20);
	TestFalse(TEXT("no dive gravity in the lag"), FGSTN_DrillTickExtension::WantsDiveGravity(&F));
	TestEqual(TEXT("landing gave the air spin back"), static_cast<int32>(F.AirSpinsUsed), 0);
	RunTick(Extension, State, 30);
	TestTrue(TEXT("the lag holds while the dive's timeline runs"), Phase(State) == EGSTN_DrillPhase::Lag);
	StopTimeline(State);
	RunTick(Extension, State, 39);
	TestTrue(TEXT("the dive's timeline ended: the lag is over"), Phase(State) == EGSTN_DrillPhase::None);

	// The pure outcome of a diving frame: a tip hit bounces (before the ground), the ground on the first frame is not a landing yet
	FGSTN_DrillFragment Diving;
	Diving.Phase = EGSTN_DrillPhase::Diving;
	Diving.PhaseStartFrame = 100;
	TestTrue(TEXT("a tip hit bounces, even on the ground"), FGSTN_DrillTickExtension::ReadDiveOutcome(Diving, 103, true, true) == EGSTN_DrillRequest::Bounce);
	TestTrue(TEXT("the ground after the first frame is the lag"), FGSTN_DrillTickExtension::ReadDiveOutcome(Diving, 101, false, true) == EGSTN_DrillRequest::GroundLand);
	TestTrue(TEXT("the ground on the dive's first frame is not (a ground dive lands the frame after)"), FGSTN_DrillTickExtension::ReadDiveOutcome(Diving, 100, false, true) == EGSTN_DrillRequest::None);
	TestTrue(TEXT("not diving: nothing"), FGSTN_DrillTickExtension::ReadDiveOutcome(GroundSpin(), 103, true, true) == EGSTN_DrillRequest::None);

	// The bounce: phase None with the Bounced flag for that frame only, the air spin not given back (7.6)
	const FGSTN_DrillSimTuning Tuning = FGSTN_DrillSimTuning::MakeDefault();
	FGSTN_DrillFragment Bounced = Diving;
	Bounced.AirSpinsUsed = 1;
	FGSTN_DrillFrameInputs Bounce;
	Bounce.Request = EGSTN_DrillRequest::Bounce;
	FGSTN_DrillTickExtension::StepAfterExecute(Bounced, 103, Bounce, Tuning);
	TestTrue(TEXT("bounced: no phase, the flag set"), Bounced.Phase == EGSTN_DrillPhase::None && (Bounced.Flags & GSTNDrillFlags::Bounced) != 0);
	TestEqual(TEXT("the bounce does not refresh the air spin"), static_cast<int32>(Bounced.AirSpinsUsed), 1);
	FGSTN_DrillTickExtension::StepAfterClocks(Bounced, 104, FGSTN_DrillFrameInputs(), Tuning);
	TestEqual(TEXT("the flag is gone the frame after"), static_cast<int32>(Bounced.Flags & GSTNDrillFlags::Bounced), 0);
	TestEqual(TEXT("and the wire takes the flag"), PayloadBytes(Diving) > 0, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPSpinCancelTest, "GSTNSpinDrill.NP.Spin.OppositeCancel", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPSpinCancelTest::RunTest(const FString& Parameters)
{
	FScopedFragment Registered;
	FTimelineDrillExtension Extension;
	const FGSTN_CombatTimelineData AirCw = FGSTN_SpinTimelines::Make(FGSTN_SpinTimelines::AirParams(), EGSTN_SpinSense::Clockwise, true);
	const FGSTN_CombatTimelineData CancelCcw = FGSTN_SpinTimelines::MakeCancel(FGSTN_SpinCancelTimelineParams(), EGSTN_SpinSense::CounterClockwise);
	const FGSTN_CombatTimelineData CancelCw = FGSTN_SpinTimelines::MakeCancel(FGSTN_SpinCancelTimelineParams(), EGSTN_SpinSense::Clockwise);
	const auto Phase = [](const FGSTN_CombatSyncState& State) { return static_cast<int32>(ReadFragment(State).Phase); };
	const int32 Spinning = static_cast<int32>(EGSTN_DrillPhase::Spinning);
	const int32 None = static_cast<int32>(EGSTN_DrillPhase::None);

	FGSTN_CombatSyncState State;
	State.Core.Jump.Phase = EGSTN_JumpPhase::Falling;
	Extension.Started = AirCw;
	RunTimeline(State, 10);
	RunTick(Extension, State, 10);
	TestEqual(TEXT("air spin from frame 10"), Phase(State), Spinning);

	// The phase machine's own refusals, on the fragment (in game the windows already keep these timelines from starting): the same sense
	// never cancels, and the opposite rotation does not before CancelFromFrame (6)
	const FGSTN_DrillSimTuning Tuning = FGSTN_DrillSimTuning::MakeDefault();
	FGSTN_DrillFrameInputs SameSense;
	FGSTN_DrillTickExtension::ReadCancelStart(*FGSTN_SpinCancelTrack::Find(CancelCw), SameSense);
	FGSTN_DrillFragment Same = ReadFragment(State);
	FGSTN_DrillTickExtension::StepAfterExecute(Same, 17, SameSense, Tuning);
	TestEqual(TEXT("a cancel of the same sense is refused"), static_cast<int32>(Same.Phase), Spinning);
	FGSTN_DrillFrameInputs OppositeSense;
	FGSTN_DrillTickExtension::ReadCancelStart(*FGSTN_SpinCancelTrack::Find(CancelCcw), OppositeSense);
	FGSTN_DrillFragment Early = ReadFragment(State);
	FGSTN_DrillTickExtension::StepAfterExecute(Early, 14, OppositeSense, Tuning);
	TestEqual(TEXT("frame 4 of the spin: refused"), static_cast<int32>(Early.Phase), Spinning);

	// Through the extension: the cancel's timeline starts on frame 16, frame 6 of the spin
	Extension.Started = CancelCcw;

	// From frame 6 it stops the spin; the low gravity of the air spin ends with it, the air spin stays spent
	RunTimeline(State, 16);
	RunTick(Extension, State, 16);
	const FGSTN_DrillFragment Cancelled = ReadFragment(State);
	TestEqual(TEXT("frame 6 of the spin: cancelled"), static_cast<int32>(Cancelled.Phase), None);
	TestEqual(TEXT("the low gravity ended with the cancel"), Cancelled.LowGravityUntilFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the air spin stays spent for this airtime"), static_cast<int32>(Cancelled.AirSpinsUsed), 1);

	// While the cancel's recovery runs nothing restarts the spin
	RunTick(Extension, State, 17);
	TestEqual(TEXT("still no phase during the recovery"), Phase(State), None);

	// A tuning that keeps low gravity a little longer keeps it past the cancel by those frames only
	FGSTN_DrillFragment Air;
	FGSTN_DrillSimTuning Longer = FGSTN_DrillSimTuning::MakeDefault();
	Longer.LowGravityExtraFrames = 5;
	FGSTN_DrillFrameInputs Start;
	FGSTN_DrillTickExtension::ReadSpinStart(*FGSTN_SpinTrack::Find(AirCw), Start);
	FGSTN_DrillTickExtension::StepAfterExecute(Air, 100, Start, Longer);
	FGSTN_DrillFrameInputs Stop;
	FGSTN_DrillTickExtension::ReadCancelStart(*FGSTN_SpinCancelTrack::Find(CancelCcw), Stop);
	FGSTN_DrillTickExtension::StepAfterExecute(Air, 110, Stop, Longer);
	TestEqual(TEXT("extra frames: low gravity until the cancel frame + 5"), Air.LowGravityUntilFrame, 115);
	return true;
}

namespace GSTNDrillNPTest
{
	/** A link fragment whose Body channel holds a link of TypeIndex in Role and Phase */
	FGSTN_LinkFragment BodyLink(uint8 TypeIndex, uint8 Role, EGSTN_LinkPhase Phase, uint8 LinkFlags = 0, EGSTN_LinkBreakReason Reason = EGSTN_LinkBreakReason::None)
	{
		FGSTN_LinkFragment Link;
		Link.Links.Body.LinkTypeIndex = TypeIndex;
		Link.Links.Body.Role = Role;
		Link.Links.Body.Phase = Phase;
		Link.Links.Body.Serial = 1;
		Link.Links.Body.Flags = LinkFlags;
		Link.Links.Body.BreakReason = static_cast<uint8>(Reason);
		return Link;
	}

	FGSTN_DrillFragment DivingFrom(int32 Start)
	{
		FGSTN_DrillFragment Fragment;
		Fragment.Phase = EGSTN_DrillPhase::Diving;
		Fragment.PhaseStartFrame = Start;
		return Fragment;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPLatchLinkViewTest, "GSTNSpinDrill.NP.Latch.LinkView", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPLatchLinkViewTest::RunTest(const FString& Parameters)
{
	using namespace GSTNDrillNPTest;
	constexpr uint8 Drill = 3;
	auto View = [](const FGSTN_LinkFragment& Link) { return FGSTN_DrillTickExtension::ReadLinkView(&Link, Drill); };

	const FGSTN_LinkFragment ActiveDrill = BodyLink(Drill, 1, EGSTN_LinkPhase::Active);
	TestFalse(TEXT("no link fragment: nothing"), FGSTN_DrillTickExtension::ReadLinkView(nullptr, Drill).bActive);
	TestFalse(TEXT("drill type not registered (index 0): nothing"), FGSTN_DrillTickExtension::ReadLinkView(&ActiveDrill, 0).bActive);
	TestTrue(TEXT("pending driller"), View(BodyLink(Drill, 1, EGSTN_LinkPhase::Pending, GSTNLinkFlags::Predicted)).bPending);
	TestTrue(TEXT("active driller"), View(ActiveDrill).bActive);
	TestTrue(TEXT("the request of the hit frame (no role yet) is the driller's pending"), View(BodyLink(Drill, 0, EGSTN_LinkPhase::Pending, GSTNLinkFlags::Predicted)).bPending);
	TestFalse(TEXT("a role-less request of another type is not"), View(BodyLink(Drill + 1, 0, EGSTN_LinkPhase::Pending, GSTNLinkFlags::Predicted)).bPending);
	TestFalse(TEXT("another link type is not the drill's"), View(BodyLink(Drill + 1, 1, EGSTN_LinkPhase::Active)).bActive);
	TestFalse(TEXT("the drilled side (role 2) is not the driller"), View(BodyLink(Drill, 2, EGSTN_LinkPhase::Active)).bActive);
	const FGSTN_DrillLinkView Hit = View(BodyLink(Drill, 1, EGSTN_LinkPhase::Active, GSTNLinkFlags::PendingBreak, EGSTN_LinkBreakReason::Hit));
	TestTrue(TEXT("a decided hit break: no longer active, broken by a hit"), !Hit.bActive && Hit.bBrokenByHit);
	const FGSTN_DrillLinkView Exiting = View(BodyLink(Drill, 1, EGSTN_LinkPhase::Exiting, 0, EGSTN_LinkBreakReason::PartnerDeath));
	TestTrue(TEXT("exiting for the target's death: not active, not a hit"), !Exiting.bActive && !Exiting.bBrokenByHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPLatchDiveOutcomeTest, "GSTNSpinDrill.NP.Latch.DiveOutcomeFromLink", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPLatchDiveOutcomeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNDrillNPTest;
	const FGSTN_DrillSimTuning Tuning = FGSTN_DrillSimTuning::MakeDefault();
	FGSTN_DrillLinkView Pending;
	Pending.bPending = true;
	FGSTN_DrillLinkView Active;
	Active.bActive = true;

	// The tip's hit frame: the link tick (120) already marked the request Pending, so the dive waits instead of bouncing
	FGSTN_DrillFragment F = DivingFrom(100);
	TestTrue(TEXT("a tip hit with the request pending: await, no bounce"), FGSTN_DrillTickExtension::ReadDiveOutcome(F, 101, true, false, Pending) == EGSTN_DrillRequest::AwaitLatch);
	FGSTN_DrillFrameInputs In;
	In.Request = EGSTN_DrillRequest::AwaitLatch;
	FGSTN_DrillTickExtension::StepAfterExecute(F, 101, In, Tuning);
	TestTrue(TEXT("awaiting: still diving, the flag set"), F.Phase == EGSTN_DrillPhase::Diving && (F.Flags & GSTNDrillFlags::AwaitingLatch) != 0);

	// Refused (or timed out): the link went back to idle -> the bounce of 7.6, the flag gone with it
	FGSTN_DrillFragment Refused = F;
	TestTrue(TEXT("idle after awaiting: bounce"), FGSTN_DrillTickExtension::ReadDiveOutcome(Refused, 103, false, false) == EGSTN_DrillRequest::Bounce);
	In.Request = EGSTN_DrillRequest::Bounce;
	FGSTN_DrillTickExtension::StepAfterExecute(Refused, 103, In, Tuning);
	TestTrue(TEXT("bounced, no awaiting flag left"), Refused.Phase == EGSTN_DrillPhase::None && Refused.Flags == GSTNDrillFlags::Bounced);

	// Accepted on the dive's first frame: the latch is asked every frame and seats from MinDiveFramesBeforeLatch on
	FGSTN_DrillFragment Early = DivingFrom(100);
	Early.Flags = GSTNDrillFlags::AwaitingLatch;
	TestTrue(TEXT("active: latch"), FGSTN_DrillTickExtension::ReadDiveOutcome(Early, 101, false, true, Active) == EGSTN_DrillRequest::Latch);
	In = FGSTN_DrillFrameInputs();
	In.Request = EGSTN_DrillRequest::Latch;
	In.LatchSeatFrames = 12;
	In.LatchMaxBoreFrames = 90;
	FGSTN_DrillTickExtension::StepAfterExecute(Early, 101, In, Tuning);
	TestTrue(TEXT("too early to seat: still diving"), Early.Phase == EGSTN_DrillPhase::Diving);
	FGSTN_DrillTickExtension::StepAfterExecute(Early, 100 + Tuning.MinDiveFramesBeforeLatch, In, Tuning);
	TestTrue(TEXT("seats the next frame"), Early.Phase == EGSTN_DrillPhase::Seating && Early.LatchFrame == 101 + Tuning.MinDiveFramesBeforeLatch);
	TestEqual(TEXT("the awaiting flag cleared by the latch"), static_cast<int32>(Early.Flags & GSTNDrillFlags::AwaitingLatch), 0);
	TestTrue(TEXT("a plain tip hit (no link asked) still bounces"), FGSTN_DrillTickExtension::ReadDiveOutcome(DivingFrom(100), 103, true, false) == EGSTN_DrillRequest::Bounce);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillNPLatchEndTest, "GSTNSpinDrill.NP.Latch.EndAndRelease", GSTNDrillNPTest::Flags)
bool FGSTN_DrillNPLatchEndTest::RunTest(const FString& Parameters)
{
	using namespace GSTNDrillNPTest;
	const FGSTN_DrillSimTuning Tuning = FGSTN_DrillSimTuning::MakeDefault();
	FGSTN_DrillLinkView Active;
	Active.bActive = true;

	FGSTN_DrillFragment Latched;
	Latched.Phase = EGSTN_DrillPhase::Boring;
	Latched.PhaseStartFrame = 115;
	Latched.LatchFrame = 103;
	Latched.SeatFrames = 12;
	Latched.MaxBoreFrames = 90;

	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.TimelineStartFrame = 98;
	FGSTN_DrillFrameInputs In;
	FGSTN_DrillTickExtension::ReadLatchEnd(Latched, Core, Active, In);
	TestTrue(TEXT("the dive's timeline runs: crouch held, nothing gone"), In.bCrouchHeld && !In.bTargetGone && !In.bDrillerInHitstun);
	FGSTN_DrillFragment Kept = Latched;
	FGSTN_DrillTickExtension::StepAfterExecute(Kept, 120, In, Tuning);
	TestTrue(TEXT("held: keeps boring"), Kept.Phase == EGSTN_DrillPhase::Boring);

	// Crouch released: the dive ability ended its timeline -> the pop, and the drill releases the link it holds
	Core.ActionState = EGSTN_ActionState::Actionable;
	In = FGSTN_DrillFrameInputs();
	FGSTN_DrillTickExtension::ReadLatchEnd(Latched, Core, Active, In);
	TestFalse(TEXT("timeline over: crouch released"), In.bCrouchHeld);
	FGSTN_DrillFragment Popped = Latched;
	FGSTN_DrillTickExtension::StepAfterExecute(Popped, 120, In, Tuning);
	TestTrue(TEXT("released without a jump: the pop (no phase, no latch)"), Popped.Phase == EGSTN_DrillPhase::None && Popped.LatchFrame == INDEX_NONE);
	FGSTN_LinkFragment Link = BodyLink(3, 1, EGSTN_LinkPhase::Active);
	TestTrue(TEXT("the drill releases its link"), FGSTN_DrillTickExtension::ReleaseLinkOnUnlatch(true, Popped, Active, &Link));
	TestTrue(TEXT("as GSTNLink's pending Release break"), Link.Links.Body.HasFlag(GSTNLinkFlags::PendingBreak)
		&& Link.Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::Release);
	TestFalse(TEXT("still latched: no release"), FGSTN_DrillTickExtension::ReleaseLinkOnUnlatch(true, Latched, Active, &Link));

	// The link ended first: a hit on the driller detaches it (no launch); any other break is the target gone (the launch)
	Core.ActionState = EGSTN_ActionState::InTimeline;
	FGSTN_DrillLinkView ByHit;
	ByHit.bBrokenByHit = true;
	In = FGSTN_DrillFrameInputs();
	FGSTN_DrillTickExtension::ReadLatchEnd(Latched, Core, ByHit, In);
	FGSTN_DrillFragment Detached = Latched;
	FGSTN_DrillTickExtension::StepAfterExecute(Detached, 120, In, Tuning);
	TestTrue(TEXT("broken by a hit: detached, no launch"), In.bDrillerInHitstun && Detached.Phase == EGSTN_DrillPhase::None);
	TestFalse(TEXT("nothing to release: the link is no longer active"), FGSTN_DrillTickExtension::ReleaseLinkOnUnlatch(true, Detached, ByHit, &Link));
	In = FGSTN_DrillFrameInputs();
	FGSTN_DrillTickExtension::ReadLatchEnd(Latched, Core, FGSTN_DrillLinkView(), In);
	FGSTN_DrillFragment Launched = Latched;
	FGSTN_DrillTickExtension::StepAfterExecute(Launched, 120, In, Tuning);
	TestTrue(TEXT("link gone otherwise: the target is gone, the launch"), In.bTargetGone && Launched.Phase == EGSTN_DrillPhase::Launching);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

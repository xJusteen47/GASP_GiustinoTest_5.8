// Automation tests of the defence tick extension and the hold window hook (M6 F7, spec 5.2 steps 1, 3, 6 and 7): a held shield stalls the
// timeline and not the combat clock, the stuns veto every command, the state tags are projected from the fragment. No simulation
// component: the clock logic, the hook registry and the extension are driven by hand, as the F2 tests drive them.

#include "GSTN_DefenceNPTestFixture.h"
#include "Data/GSTN_DefenceTracks.h"
#include "GSTN_DefenceTags.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputSample.h"
#include "Sim/GSTN_DefenceTickExtension.h"
#include "Sim/GSTN_DefenceTrackHooks.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_InputSimTypes.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "Timeline/GSTN_TimelinePlayer.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNDefenceNPTest;

namespace
{
	/** The module registered the hold hook for the hold track struct at startup; a test that runs without the module registers its own */
	struct FDefHoldHookScope
	{
		FDefHoldHookScope()
		{
			bOwned = FGSTN_TimelineTrackHookRegistry::Get().Register(FGSTN_HoldWindowTrack::StaticStruct(), &Hook);
		}

		~FDefHoldHookScope()
		{
			if (bOwned)
			{
				FGSTN_TimelineTrackHookRegistry::Get().Unregister(&Hook);
			}
		}

		FGSTN_HoldWindowHook Hook;
		bool bOwned = false;
	};

	/** 30 frames, a hold window on the shield button over timeline frame 1 (one frame, as the shield timeline builds it), actionable from 20 */
	FGSTN_CombatTimelineData DefMakeHoldTimeline()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 20;
		Data.OverrideFrames = 2;
		FGSTN_HoldWindowTrack Hold;
		Hold.StartFrame = 1;
		Hold.EndFrame = 2;
		Hold.HoldButton = GSTNCombatButtonTags::Button_Shield.GetTag();
		Hold.bEndOnZeroShield = true;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_HoldWindowTrack>(Hold));
		return Data;
	}

	FGSTN_CombatSyncCore DefMakeRunning(int32 StartFrame)
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, StartFrame, 3, EGSTN_ActivationSource::Command);
		return Core;
	}

	/** Appends one frame to the history: the shield button (bit 0) held or not */
	void DefSample(FGSTN_InputHistory& History, int32 Frame, bool bHeld)
	{
		const bool bWasHeld = (History.GetHeldButtons() & 1u) != 0;
		FGSTN_InputSample Sample;
		Sample.Frame = Frame;
		Sample.Held = bHeld ? 1u : 0u;
		Sample.Pressed = bHeld && !bWasHeld ? 1u : 0u;
		Sample.Released = !bHeld && bWasHeld ? 1u : 0u;
		History.Append(Sample);
	}

	const FGSTN_StampedWindow* DefFindHoldStamp(const FGSTN_TimelineOutput& Output)
	{
		for (const FGSTN_StampedWindow& Stamp : Output.StampedWindows)
		{
			if (Stamp.Profile == GSTNDefenceStamps::HoldMarker)
			{
				return &Stamp;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTickHoldStallTest, "GSTNDefence.NP.Tick.HoldStallsTimelineNotClock", TestFlags)
bool FGSTN_DefenceTickHoldStallTest::RunTest(const FString& Parameters)
{
	FDefHoldHookScope Scope;
	const FGSTN_CombatTimelineData Data = DefMakeHoldTimeline();
	const FGameplayTag Buttons[] = { GSTNCombatButtonTags::Button_Shield.GetTag() };
	const FGSTN_ButtonMap ButtonMap(Buttons);
	FGSTN_SyncFragmentArray Fragments;
	FGSTN_TrackHookEnvironment Environment;
	Environment.Buttons = &ButtonMap;
	Environment.Fragments = &Fragments;

	// The shield is pressed on 100 and held until 109 (the sample of 109 is the first with the button up)
	FGSTN_CombatSyncCore Core = DefMakeRunning(100);
	DefSample(Core.Input.History, 99, false);
	DefSample(Core.Input.History, 100, true);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output, &Environment);
	const FGSTN_StampedWindow* Start = DefFindHoldStamp(Output);
	TestNotNull(TEXT("the start frame stamps the hold: the raise"), Start);
	if (Start)
	{
		TestEqual(TEXT("the stamp starts at the press frame"), Start->StartFrame, 100);
		TestEqual(TEXT("and has no end while held"), Start->EndFrame, INDEX_NONE);
	}

	int32 ReleaseStampFrame = INDEX_NONE;
	for (int32 Frame = 101; Frame <= 115; ++Frame)
	{
		// The combat tick runs the clock before it samples the frame's input (the history ends at F-1 here)
		Core.Requests.PruneBefore(Frame);
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 1, true, Output, false, nullptr, &Environment);
		DefSample(Core.Input.History, Frame, Frame < 109);
		const FGSTN_StampedWindow* Stamp = DefFindHoldStamp(Output);
		TestNotNull(*FString::Printf(TEXT("frame %d: the hold stamps every tick"), Frame), Stamp);
		if (Frame >= 101 && Frame <= 109)
		{
			// 101 evaluates frame 1 (the hold frame); from 102 the timeline stands ON it (its cancel windows stay open) while the button of the previous frame was down
			TestEqual(*FString::Printf(TEXT("frame %d: the timeline stands on the hold frame"), Frame), Output.Frame, 1);
		}
		if (Stamp && Stamp->EndFrame != INDEX_NONE && ReleaseStampFrame == INDEX_NONE)
		{
			ReleaseStampFrame = Stamp->EndFrame;
		}
	}
	// The sample of 109 has the button up; the hook of frame 110 reads the history of the previous frame and ends the hold there
	TestEqual(TEXT("the hold ended the frame the release was seen"), ReleaseStampFrame, 110);
	TestEqual(TEXT("the timeline counted the stalled frames (102 to 109)"), Core.TimelinePausedFrames, 8);
	TestEqual(TEXT("the combat clock did not pause"), Core.PausedFramesTotal, 0);
	TestEqual(TEXT("so it still counts every frame"), FGSTN_CombatClock::FromFrame(115, Core.PausedFramesTotal), 115);
	TestTrue(TEXT("the timeline goes on after the release"), Output.Frame > 1);

	// A shield stun keeps the hold whatever the button: the fragment of the environment says so
	FGSTN_CombatSyncCore Stunned = DefMakeRunning(200);
	DefSample(Stunned.Input.History, 200, true);
	FGSTN_DefenceFragment Fragment = MakeRaised(200);
	Fragment.ShieldStunEndFrame = 206;
	SetFragment(Fragments, Fragment);
	FGSTN_CombatTimelineLogic::EvaluateStart(Stunned, 200, Data, 1, Output, &Environment);
	for (int32 Frame = 201; Frame <= 208; ++Frame)
	{
		DefSample(Stunned.Input.History, Frame, false);
		Stunned.Requests.PruneBefore(Frame);
		FGSTN_CombatTimelineLogic::AdvanceClock(Stunned, Frame, &Data, 1, true, Output, false, nullptr, &Environment);
		const FGSTN_StampedWindow* Stamp = DefFindHoldStamp(Output);
		if (Frame <= 205)
		{
			TestTrue(*FString::Printf(TEXT("frame %d: stunned, the hold goes on with the button up"), Frame), Stamp && Stamp->EndFrame == INDEX_NONE);
		}
	}
	TestEqual(TEXT("the stun held the timeline on its hold frame from 202 to 205 (201 reached it)"), Stunned.TimelinePausedFrames, 4);

	// The value decayed to nothing (the Empty flag the tick extension sets): the hold ends although the button is down
	FGSTN_CombatSyncCore Empty = DefMakeRunning(300);
	DefSample(Empty.Input.History, 300, true);
	FGSTN_DefenceFragment Drained = MakeRaised(300, 0.0);
	Drained.ShieldFlags |= GSTNShieldFlags::Empty;
	SetFragment(Fragments, Drained);
	FGSTN_CombatTimelineLogic::EvaluateStart(Empty, 300, Data, 1, Output, &Environment);
	DefSample(Empty.Input.History, 301, true);
	FGSTN_CombatTimelineLogic::AdvanceClock(Empty, 301, &Data, 1, true, Output, false, nullptr, &Environment);
	const FGSTN_StampedWindow* EmptyStamp = DefFindHoldStamp(Output);
	TestTrue(TEXT("an empty shield ends the hold on the frame"), EmptyStamp && EmptyStamp->EndFrame == 301);
	TestEqual(TEXT("and nothing stalled"), Empty.TimelinePausedFrames, 0);

	// Without a button map the button reads as up: no stall, the hold ends at once
	Fragments.Reset();
	FGSTN_TrackHookEnvironment NoButtons;
	NoButtons.Fragments = &Fragments;
	FGSTN_CombatSyncCore Blind = DefMakeRunning(400);
	DefSample(Blind.Input.History, 400, true);
	FGSTN_CombatTimelineLogic::EvaluateStart(Blind, 400, Data, 1, Output, &NoButtons);
	DefSample(Blind.Input.History, 401, true);
	FGSTN_CombatTimelineLogic::AdvanceClock(Blind, 401, &Data, 1, true, Output, false, nullptr, &NoButtons);
	TestEqual(TEXT("no button map: no stall"), Blind.TimelinePausedFrames, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTickVetoTest, "GSTNDefence.NP.Tick.VetoWhileStunned", TestFlags)
bool FGSTN_DefenceTickVetoTest::RunTest(const FString& Parameters)
{
	FGSTN_DefenceTickExtension Extension;
	TestEqual(TEXT("the defence feature id"), static_cast<int32>(Extension.GetTypeId()), static_cast<int32>(GSTNFeatureIds::Defence));
	TestEqual(TEXT("after the parts extension"), Extension.GetPriority(), 110);

	FGSTN_CombatSyncState In, Out;
	FGSTN_CombatTickContext Context(120, In, Out);
	TestFalse(TEXT("no fragment: nothing vetoes"), Extension.BlocksActions(Context));

	// A raised shield alone vetoes nothing: the hold is a timeline, its out-of-shield options are cancel windows
	SetFragment(Out.Fragments, MakeRaised(100));
	TestFalse(TEXT("a raised shield is not a veto"), Extension.BlocksActions(Context));

	const auto VetoAt = [&](int32 Frame, TFunctionRef<void(FGSTN_DefenceFragment&)> Change)
	{
		FGSTN_DefenceFragment Fragment = MakeRaised(100);
		Change(Fragment);
		SetFragment(Out.Fragments, Fragment);
		FGSTN_CombatTickContext At(Frame, In, Out);
		return Extension.BlocksActions(At);
	};
	TestTrue(TEXT("shield stun: vetoed before its end"), VetoAt(124, [](FGSTN_DefenceFragment& F) { F.ShieldStunEndFrame = 125; }));
	TestFalse(TEXT("shield stun: free on its end frame"), VetoAt(125, [](FGSTN_DefenceFragment& F) { F.ShieldStunEndFrame = 125; }));
	TestTrue(TEXT("broken: vetoed"), VetoAt(200, [](FGSTN_DefenceFragment& F) { F.ShieldFlags = GSTNShieldFlags::Breaking; F.BreakEndFrame = 250; }));
	TestTrue(TEXT("punished: vetoed"), VetoAt(130, [](FGSTN_DefenceFragment& F) { F.PunishEndFrame = 140; }));
	TestFalse(TEXT("punished: free at the end"), VetoAt(140, [](FGSTN_DefenceFragment& F) { F.PunishEndFrame = 140; }));
	// F8: the downed state is not a veto, it admits only the bindings that require Combat.State.Knockdown (its get-ups)
	TestFalse(TEXT("knocked down: not vetoed"), VetoAt(130, [](FGSTN_DefenceFragment& F) { F.KnockdownEndFrame = 170; }));
	{
		FGSTN_DefenceFragment Downed;
		Downed.KnockdownEndFrame = 170;
		SetFragment(Out.Fragments, Downed);
		FGSTN_CombatTickContext At(130, In, Out);
		TestTrue(TEXT("knocked down: admits only the get-ups"), Extension.AdmitsOnlyBindingsRequiring(At) == GSTNDefenceTags::State_Knockdown.GetTag());
	}
	TestFalse(TEXT("parry recovery is not a veto (the defender may cancel into anything)"), VetoAt(130, [](FGSTN_DefenceFragment& F) { F.ParryRecoveryEndFrame = 134; }));

	// Through the registry, as the component asks: the OR of every extension
	FGSTN_CombatTickExtensionRegistry Registry;
	TestTrue(TEXT("registers"), Registry.Register(&Extension));
	SetFragment(Out.Fragments, MakeRaised(100));
	FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Out.Fragments)->ShieldStunEndFrame = 125;
	TestTrue(TEXT("the registry reports the veto"), Registry.BlocksActions(Context));
	Out.Fragments.Reset();
	TestFalse(TEXT("a rollback to no fragment lifts it"), Registry.BlocksActions(Context));

	// The OnParry cancel condition holds while the running timeline parried, and only then
	FGSTN_DefenceFragment Parried = MakeRaised(100);
	Parried.LastParryFrame = 118;
	SetFragment(Out.Fragments, Parried);
	const uint32 OnParryBit = 1u << static_cast<uint32>(EGSTN_CancelCondition::OnParry);
	TestEqual(TEXT("not in a timeline: no condition"), Registry.HeldCancelConditions(Context), 0u);
	Out.Core = DefMakeRunning(110);
	TestEqual(TEXT("the running timeline parried at 118: OnParry holds"), Registry.HeldCancelConditions(Context), OnParryBit);
	Out.Core = DefMakeRunning(119);
	TestEqual(TEXT("a timeline started after the parry: nothing"), Registry.HeldCancelConditions(Context), 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTickTagsTest, "GSTNDefence.NP.Tick.TagsProjected", TestFlags)
bool FGSTN_DefenceTickTagsTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	FGSTN_DefenceAttach::Apply(Rig.Owner, MakeRules(true), true, FGameplayTagContainer());

	FGSTN_DefenceTickExtension Extension;
	FGSTN_CombatSyncState In, Out;
	FGSTN_CombatTickContext Context(112, In, Out);
	Context.Owner = Rig.Owner;
	Context.AbilitySystem = Rig.ASC;
	const auto Project = [&](int32 Frame)
	{
		FGSTN_CombatTickContext At(Frame, In, Out);
		At.Owner = Rig.Owner;
		At.AbilitySystem = Rig.ASC;
		Extension.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, At);
	};
	const auto Count = [&](const FGameplayTag& Tag) { return Rig.ASC->GetTagCount(Tag); };

	// Raised at 100: Shielding from the frame after the press (one startup frame)
	SetFragment(Out.Fragments, MakeRaised(100));
	Project(100);
	TestEqual(TEXT("on the press frame the bubble does not block yet"), Count(GSTNDefenceTags::State_Shielding), 0);
	Project(112);
	TestEqual(TEXT("raised past startup: Shielding"), Count(GSTNDefenceTags::State_Shielding), 1);
	TestEqual(TEXT("no stun"), Count(GSTNDefenceTags::State_ShieldStun), 0);
	TestEqual(TEXT("no parry window while held (release anchored)"), Count(GSTNDefenceTags::State_ParryWindow), 0);
	Project(113);
	TestEqual(TEXT("projected again: nothing stacks"), Count(GSTNDefenceTags::State_Shielding), 1);

	// Released at 120: the perfect-shield window is open for five frames, the shield is down
	FGSTN_DefenceFragment Released = MakeRaised(100, 92.0);
	Released.ShieldFlags = 0;
	Released.ShieldReleaseFrame = 120;
	Released.ShieldStampFrame = 120;
	SetFragment(Out.Fragments, Released);
	Project(122);
	TestEqual(TEXT("released: not Shielding"), Count(GSTNDefenceTags::State_Shielding), 0);
	TestEqual(TEXT("released: the parry window"), Count(GSTNDefenceTags::State_ParryWindow), 1);
	Project(125);
	TestEqual(TEXT("five frames later the window closed"), Count(GSTNDefenceTags::State_ParryWindow), 0);

	// The stuns and the other states
	FGSTN_DefenceFragment Stunned = MakeRaised(100);
	Stunned.ShieldStunEndFrame = 130;
	SetFragment(Out.Fragments, Stunned);
	Project(125);
	TestEqual(TEXT("shield stun"), Count(GSTNDefenceTags::State_ShieldStun), 1);
	TestEqual(TEXT("still Shielding through the stun"), Count(GSTNDefenceTags::State_Shielding), 1);
	Project(130);
	TestEqual(TEXT("the stun ended"), Count(GSTNDefenceTags::State_ShieldStun), 0);

	FGSTN_DefenceFragment Broken = MakeRaised(100, 0.0);
	Broken.ShieldFlags = GSTNShieldFlags::Breaking;
	Broken.BreakEndFrame = 300;
	SetFragment(Out.Fragments, Broken);
	Project(140);
	TestEqual(TEXT("broken"), Count(GSTNDefenceTags::State_ShieldBroken), 1);
	TestEqual(TEXT("a broken shield is not Shielding"), Count(GSTNDefenceTags::State_Shielding), 0);

	FGSTN_DefenceFragment Other;
	Other.MoveParryStartFrame = 150;
	Other.MoveParryEndFrame = 160;
	Other.ParryRecoveryEndFrame = 158;
	Other.PunishEndFrame = 170;
	Other.KnockdownEndFrame = 180;
	Other.TechEndFrame = 156;
	Other.TechLockoutEndFrame = 190;
	SetFragment(Out.Fragments, Other);
	Project(155);
	TestEqual(TEXT("a move's window: ParryWindow"), Count(GSTNDefenceTags::State_ParryWindow), 1);
	TestEqual(TEXT("parry recovery"), Count(GSTNDefenceTags::State_ParryRecovery), 1);
	TestEqual(TEXT("punished: Parried"), Count(GSTNDefenceTags::State_Parried), 1);
	TestEqual(TEXT("knockdown"), Count(GSTNDefenceTags::State_Knockdown), 1);
	TestEqual(TEXT("teching"), Count(GSTNDefenceTags::State_Teching), 1);
	TestEqual(TEXT("tech lockout"), Count(GSTNDefenceTags::State_Tech_Lockout), 1);
	TestEqual(TEXT("broken is gone with the fragment that said so"), Count(GSTNDefenceTags::State_ShieldBroken), 0);
	Project(160);
	TestEqual(TEXT("the move window closed"), Count(GSTNDefenceTags::State_ParryWindow), 0);
	TestEqual(TEXT("recovery over"), Count(GSTNDefenceTags::State_ParryRecovery), 0);
	TestEqual(TEXT("tech over"), Count(GSTNDefenceTags::State_Teching), 0);

	// A rollback to a state without the fragment: every tag goes, with no memory of the predicted frames
	Out.Fragments.Reset();
	Project(161);
	for (const FGameplayTag& Tag : { GSTNDefenceTags::State_Shielding.GetTag(), GSTNDefenceTags::State_ShieldStun.GetTag(), GSTNDefenceTags::State_ShieldBroken.GetTag(),
		GSTNDefenceTags::State_ParryWindow.GetTag(), GSTNDefenceTags::State_ParryRecovery.GetTag(), GSTNDefenceTags::State_Parried.GetTag(),
		GSTNDefenceTags::State_Knockdown.GetTag(), GSTNDefenceTags::State_Teching.GetTag(), GSTNDefenceTags::State_Tech_Lockout.GetTag() })
	{
		TestEqual(*FString::Printf(TEXT("%s cleared by the restored state"), *Tag.ToString()), Count(Tag), 0);
	}

	// The mode disables the shield: a raised fragment projects no Shielding (nothing of the shield exists), the move window still projects
	FGSTN_DefenceAttach::Apply(Rig.Owner, MakeRules(false), true, FGameplayTagContainer());
	FGSTN_DefenceFragment PvE = MakeRaised(100);
	PvE.MoveParryStartFrame = 110;
	PvE.MoveParryEndFrame = 120;
	SetFragment(Out.Fragments, PvE);
	Project(112);
	TestEqual(TEXT("PvE: no Shielding"), Count(GSTNDefenceTags::State_Shielding), 0);
	TestEqual(TEXT("PvE: the counter move's window"), Count(GSTNDefenceTags::State_ParryWindow), 1);

	// No ability system: the hook does nothing and does not crash
	FGSTN_CombatTickContext Blind(112, In, Out);
	Blind.Owner = Rig.Owner;
	Extension.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Blind);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

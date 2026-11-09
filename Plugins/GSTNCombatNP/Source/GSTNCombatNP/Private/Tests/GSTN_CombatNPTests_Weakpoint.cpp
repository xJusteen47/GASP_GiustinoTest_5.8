// Automation tests of the weak point precision tools (plan precisione-weakpoint 1-3): the hard-surface behaviour of a move, the slide of a swing
// along a hard face, the recoil that waits for the end of the active frames, and the stab-only hurtboxes. Pure like the other attacker tests: a
// hand-built snapshot, the attacker step and the timeline clock over a sync core.

#include "Misc/AutomationTest.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Rules/GSTN_CombatRules.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** Ricochet on the set hurtbox 0 only (the hard shell of the fixtures), a normal hit elsewhere */
	struct FWpShellRicochet : IGSTN_AttackerOutcomeExtension
	{
		virtual FName GetName() const override { return TEXT("WeakpointTestShellRicochet"); }
		virtual EGSTN_AttackerHitOutcome Evaluate(const FGSTN_HitContextView& View) const override
		{
			return View.HurtboxIndex == 0 ? EGSTN_AttackerHitOutcome::Ricochet : EGSTN_AttackerHitOutcome::Normal;
		}
	};

	struct FWpScopedRicochet
	{
		FWpShellRicochet Extension;
		FWpScopedRicochet() { FGSTN_AttackerOutcomeRegistry::Get().Register(&Extension); }
		~FWpScopedRicochet() { FGSTN_AttackerOutcomeRegistry::Get().Unregister(&Extension); }
	};

	/** The attacker is entity 1 at the origin facing +X, its run started on frame 0; the victim is entity 2 */
	struct FWpFixture
	{
		FGSTN_CombatTimelineData Timeline;
		FGSTN_StepSnapshot Snapshot;
		FGSTN_CombatSyncCore Core;
		FGSTN_TimelineFrameResult Scratch;
		TArray<FGSTN_AttackerHit> Hits;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set;

		FWpFixture()
		{
			Rules.Reset(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
			Set.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			Timeline.TotalFrames = 40;
			Timeline.FirstActionableFrame = 30;
			Timeline.OverrideFrames = 0;
			Core.ActionState = EGSTN_ActionState::InTimeline;
			Core.ActiveTimeline = 0;
			Core.TimelineStartFrame = 0;
			Core.Hit.ResetRun();
		}

		/** One sphere hitbox of the radius whose centre goes linearly from From (frame 0) to To (frame Frames) in root space; open on [0, Frames] */
		void AddMovingHitbox(const FVector& From, const FVector& To, int32 Frames, double Radius = 5.0, const FQuat& ShapeRotation = FQuat::Identity)
		{
			FGSTN_HitboxSpec Spec;
			Spec.Shape.Type = EGSTN_ShapeType::Sphere;
			Spec.Shape.Extent = FVector(Radius);
			Spec.Shape.Local = FTransform(ShapeRotation);
			Spec.Data.Damage = 10.f;
			Spec.Data.BaseKnockback = 600.f;
			Spec.Data.KnockbackGrowth = 20.f;
			FGSTN_HitboxKey First;
			First.Frame = 0;
			First.Offset = From;
			FGSTN_HitboxKey Last;
			Last.Frame = Frames;
			Last.Offset = To;
			Spec.Keys = { First, Last };
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = 0;
			Track.EndFrame = Frames + 1;
			Track.Hitboxes = { Spec };
			Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		}

		FGSTN_Hurtbox& AddHurtbox(EGSTN_ShapeType Type, const FVector& Extent, const FVector& Local)
		{
			FGSTN_Hurtbox& Hurtbox = Set->Hurtboxes.AddDefaulted_GetRef();
			Hurtbox.Shape.Type = Type;
			Hurtbox.Shape.Extent = Extent;
			Hurtbox.Shape.Local = FTransform(Local);
			return Hurtbox;
		}

		void Finish(const FVector& VictimLocation)
		{
			FGSTN_SnapshotEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
			Entry.EntityId = 2;
			Entry.Team = FGenericTeamId(2);
			Entry.Transform = FTransform(VictimLocation);
			Entry.Hurtboxes = Set.Get();
			Entry.HurtStates.SetNum(Set->Hurtboxes.Num());
			Entry.BoundRadius = static_cast<float>(UGSTN_CombatWorldSubsystem::ComputeBoundRadius(Set.Get()));
			Snapshot.BuildGrid(UGSTN_CombatWorldSubsystem::DefaultCellSize);
		}

		void Run(int32 Frame)
		{
			FGSTN_AttackerTickInput In;
			In.Frame = Frame;
			In.AttackerId = 1;
			In.AttackerTeam = FGenericTeamId(1);
			In.Timeline = &Timeline;
			In.TimelineFrame = Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames;
			In.Snapshot = &Snapshot;
			In.Rules = Rules.Get();
			In.Root = FTransform::Identity;
			FGSTN_CombatAttacker::Tick(In, Core, Scratch, Hits);
		}

		/** Runs frames [0, Last]; the first frame that hit hurtbox Hurtbox without a ricochet, INDEX_NONE if none */
		int32 RunUntilNormalHit(int32 Last, int32 Hurtbox, int32* OutRicochets = nullptr)
		{
			int32 Found = INDEX_NONE;
			for (int32 Frame = 0; Frame <= Last; ++Frame)
			{
				Run(Frame);
				for (const FGSTN_AttackerHit& Hit : Hits)
				{
					if (OutRicochets && Hit.bRicochet)
					{
						++*OutRicochets;
					}
					if (!Hit.bRicochet && Hit.Entry.HurtboxIndex == Hurtbox && Found == INDEX_NONE)
					{
						Found = Frame;
					}
				}
			}
			return Found;
		}

		/**
		 * A hard wall (box hurtbox 0, face at x = 80, outward normal -X) with a small soft weak point (sphere hurtbox 1) at (75, 60, 0), a few cm
		 * out of the face. The hitbox travels (4, 12, 0) a frame from (60, -60, 0): it meets the face on frame 4 and, sliding, the weak point
		 */
		void MakeWall(const FQuat& BladeRotation = FQuat::Identity, const FVector& To = FVector(100.0, 60.0, 0.0))
		{
			AddMovingHitbox(FVector(60.0, -60.0, 0.0), To, 10, 5.0, BladeRotation);
			AddHurtbox(EGSTN_ShapeType::Box, FVector(20.0, 100.0, 100.0), FVector::ZeroVector);
			AddHurtbox(EGSTN_ShapeType::Sphere, FVector(8.0), FVector(-25.0, 60.0, 0.0));
			Finish(FVector(100.0, 0.0, 0.0));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WeakpointBehaviourDefaultsTest, "GSTNCombat.NP.Weakpoint.HardSurfaceDefaults", GSTN_NP_TEST_FLAGS)
bool FGSTN_WeakpointBehaviourDefaultsTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	TestTrue(TEXT("an unclassified move keeps the immediate recoil"), Data.ResolveHardSurfaceBehaviour() == EGSTN_HardSurfaceBehaviour::ImmediateRecoil);
	Data.StrikeKind = EGSTN_StrikeKind::Swing;
	TestTrue(TEXT("a swing finishes then recoils"), Data.ResolveHardSurfaceBehaviour() == EGSTN_HardSurfaceBehaviour::FinishThenRecoil);
	Data.StrikeKind = EGSTN_StrikeKind::Stab;
	TestTrue(TEXT("a stab recoils at once"), Data.ResolveHardSurfaceBehaviour() == EGSTN_HardSurfaceBehaviour::ImmediateRecoil);
	Data.HardSurfaceBehaviour = EGSTN_HardSurfaceBehaviour::FinishThenRecoil;
	TestTrue(TEXT("the author overrides the kind (an aerial stab)"), Data.ResolveHardSurfaceBehaviour() == EGSTN_HardSurfaceBehaviour::FinishThenRecoil);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WeakpointSlideTest, "GSTNCombat.NP.Weakpoint.SlideReachesWeakPoint", GSTN_NP_TEST_FLAGS)
bool FGSTN_WeakpointSlideTest::RunTest(const FString& Parameters)
{
	FWpScopedRicochet Ricochet;

	// Blade length along Z, travel in the XY plane: the flat faces the wall, case B (slides, no recoil)
	{
		FWpFixture Fixture;
		Fixture.Timeline.StrikeKind = EGSTN_StrikeKind::Swing;
		Fixture.MakeWall();
		int32 Ricochets = 0;
		const int32 Hit = Fixture.RunUntilNormalHit(10, 1, &Ricochets);
		TestEqual(TEXT("one ricochet on the shell"), Ricochets, 1);
		TestTrue(TEXT("the contact is remembered"), Fixture.Core.HardContact.IsSet() && Fixture.Core.HardContact.Hurtbox == 0 && Fixture.Core.HardContact.TimelineFrame == 4);
		TestTrue(TEXT("the face normal is -X"), Fixture.Core.HardContact.GetNormal().Equals(-FVector::XAxisVector, 0.01));
		TestFalse(TEXT("flat along the face: case B"), Fixture.Core.HardContact.bOpposed);
		TestFalse(TEXT("case B: no recoil waits"), Fixture.Core.HardContact.bRecoilPending);
		TestEqual(TEXT("no recoil started at the contact"), Fixture.Core.RecoilStartFrame, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("the slide meets the weak point on frame 9"), Hit, 9);
	}

	// The same swing without the slide passes behind the face and misses the weak point
	{
		FWpFixture Fixture;
		Fixture.Timeline.StrikeKind = EGSTN_StrikeKind::Swing;
		Fixture.Timeline.SweepDeflection.bEnabled = false;
		Fixture.MakeWall();
		TestEqual(TEXT("no slide, no weak point"), Fixture.RunUntilNormalHit(10, 1), static_cast<int32>(INDEX_NONE));
		TestTrue(TEXT("but the contact still holds the shell out"), Fixture.Core.HardContact.IsSet());
	}

	// The blade lies along Y: the face opposes the flat, case A. The recoil waits for the end of the swing
	{
		FWpFixture Fixture;
		Fixture.Timeline.StrikeKind = EGSTN_StrikeKind::Swing;
		Fixture.MakeWall(FQuat::FindBetweenNormals(FVector::ZAxisVector, FVector::YAxisVector));
		Fixture.RunUntilNormalHit(10, 1);
		TestTrue(TEXT("case A"), Fixture.Core.HardContact.bOpposed);
		TestTrue(TEXT("a recoil waits"), Fixture.Core.HardContact.bRecoilPending);
	}

	// Straight into the face: too steep to slide, the blade stops and hits nothing more
	{
		FWpFixture Fixture;
		Fixture.Timeline.StrikeKind = EGSTN_StrikeKind::Swing;
		Fixture.MakeWall(FQuat::Identity, FVector(100.0, -60.0, 0.0));
		int32 Ricochets = 0;
		TestEqual(TEXT("a stopped blade reaches nothing"), Fixture.RunUntilNormalHit(10, 1, &Ricochets), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("one ricochet"), Ricochets, 1);
	}

	// An immediate-recoil move (unclassified) writes no contact: the component starts the recoil on the next frame as before
	{
		FWpFixture Fixture;
		Fixture.MakeWall();
		int32 Ricochets = 0;
		Fixture.RunUntilNormalHit(4, 1, &Ricochets);
		TestEqual(TEXT("the ricochet happens"), Ricochets, 1);
		TestFalse(TEXT("no contact for an immediate recoil"), Fixture.Core.HardContact.IsSet());
		TestTrue(TEXT("and the victim is on the ring as before"), Fixture.Core.Hit.HasHitTarget(2, 0, 0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WeakpointRecoilAfterSwingTest, "GSTNCombat.NP.Weakpoint.RecoilAfterActiveFrames", GSTN_NP_TEST_FLAGS)
bool FGSTN_WeakpointRecoilAfterSwingTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 40;
	Data.FirstActionableFrame = 30;
	Data.OverrideFrames = 0;
	Data.StrikeKind = EGSTN_StrikeKind::Swing;
	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = 2;
	Track.EndFrame = 10;
	Track.Hitboxes.AddDefaulted();
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	Data.bHasRicochetResponse = true;
	Data.RicochetResponse.RecoilFrames = 12;
	TestEqual(TEXT("the active frames end at 10"), Data.GetActiveEndFrame(), 10);

	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.ActiveTimeline = 0;
	Core.TimelineStartFrame = 100;
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 0, Output);
	for (int32 Frame = 101; Frame <= 105; ++Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output);
	}
	// The contact of frame 105 (timeline frame 5), case A
	Core.HardContact.VictimId = 2;
	Core.HardContact.TimelineFrame = 5;
	Core.HardContact.bOpposed = true;
	Core.HardContact.bRecoilPending = true;

	// A hitstop frame on the way does not move the recoil before the active end
	Core.HitstopStartFrame = 106;
	Core.HitstopEndFrame = 107;
	for (int32 Frame = 106; Frame <= 110; ++Frame)
	{
		TestTrue(TEXT("the swing plays on"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output, Core.IsInHitstop(Frame)));
		TestFalse(*FString::Printf(TEXT("frame %d: still the swing"), Frame), Output.bRecoil);
	}
	TestTrue(TEXT("the recoil starts on the first frame past the active end"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, 111, &Data, 0, true, Output));
	TestTrue(TEXT("it is the recoil"), Output.bRecoil && Output.Frame == 0);
	TestEqual(TEXT("recoil frame 0 is this frame"), Core.RecoilStartFrame, 111);
	TestFalse(TEXT("nothing pending any more"), Core.HardContact.bRecoilPending);
	for (int32 Frame = 112; Frame < 123; ++Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output);
	}
	TestFalse(TEXT("the recoil replaces the recovery: it ends the timeline after its frames"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, 123, &Data, 0, true, Output));
	TestTrue(TEXT("actionable"), Core.ActionState == EGSTN_ActionState::Actionable);
	TestFalse(TEXT("the contact is cleared with the timeline"), Core.HardContact.IsSet());

	// The contact goes on the wire and is compared by reconcile
	FGSTN_CombatSyncState State;
	FGSTN_InputSample Sample;
	Sample.Frame = 105;
	State.Core.Input.History.Append(Sample);
	State.Core.ActionState = EGSTN_ActionState::InTimeline;
	State.Core.ActiveTimeline = 1;
	State.Core.TimelineStartFrame = 100;
	State.Core.HardContact.VictimId = 7;
	State.Core.HardContact.TimelineFrame = 5;
	State.Core.HardContact.Hurtbox = 0x81;
	State.Core.HardContact.SetNormal(FVector(-1.0, 0.2, 0.0));
	State.Core.HardContact.bOpposed = true;
	State.Core.HardContact.bRecoilPending = true;
	FBitWriter Writer(1024, true);
	State.NetSerialize(FNetSerializeParams(Writer));
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_CombatSyncState Loaded;
	Loaded.NetSerialize(FNetSerializeParams(Reader));
	TestFalse(TEXT("the state reads back whole"), Reader.IsError() || !Reader.AtEnd());
	TestTrue(TEXT("the contact survives the wire"), Loaded.Core.HardContact == State.Core.HardContact);
	FGSTN_CombatSyncCore Other = State.Core;
	Other.HardContact.bRecoilPending = false;
	TestTrue(TEXT("a different contact asks for a reconcile"), State.Core.ShouldReconcile(Other));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WeakpointStabOnlyTest, "GSTNCombat.NP.Weakpoint.StabOnlyHurtbox", GSTN_NP_TEST_FLAGS)
bool FGSTN_WeakpointStabOnlyTest::RunTest(const FString& Parameters)
{
	auto Make = [](FWpFixture& Fixture, EGSTN_StrikeKind Kind, const FVector& From, const FVector& To)
	{
		Fixture.Timeline.StrikeKind = Kind;
		Fixture.AddMovingHitbox(From, To, 10);
		FGSTN_Hurtbox& Core = Fixture.AddHurtbox(EGSTN_ShapeType::Sphere, FVector(20.0), FVector::ZeroVector);
		Core.bStabOnly = true;
		Fixture.Finish(FVector(100.0, 0.0, 0.0));
	};
	{
		FWpFixture Fixture;
		Make(Fixture, EGSTN_StrikeKind::Stab, FVector(40.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0));
		TestNotEqual(TEXT("a straight stab reaches the deep weak point"), Fixture.RunUntilNormalHit(10, 0), static_cast<int32>(INDEX_NONE));
	}
	{
		FWpFixture Fixture;
		Make(Fixture, EGSTN_StrikeKind::Swing, FVector(40.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0));
		TestEqual(TEXT("a swing on the same path does not"), Fixture.RunUntilNormalHit(10, 0), static_cast<int32>(INDEX_NONE));
	}
	{
		FWpFixture Fixture;
		Make(Fixture, EGSTN_StrikeKind::Stab, FVector(80.0, -40.0, 0.0), FVector(80.0, 40.0, 0.0));
		TestEqual(TEXT("a stab that grazes it at a wide angle does not"), Fixture.RunUntilNormalHit(10, 0), static_cast<int32>(INDEX_NONE));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WeakpointPerGroupTest, "GSTNCombat.NP.Weakpoint.PerHitboxGroup", GSTN_NP_TEST_FLAGS)
bool FGSTN_WeakpointPerGroupTest::RunTest(const FString& Parameters)
{
	// Strike settings per hitbox group (owner 2026-11-09): a move that slashes with one group and thrusts with another
	{
		FGSTN_CombatTimelineData Data;
		Data.StrikeKind = EGSTN_StrikeKind::Swing;
		Data.bHasRicochetResponse = true;
		Data.RicochetResponse.RecoilFrames = 12;
		FGSTN_TimelineTrack_Hitbox Slash;
		Slash.StartFrame = 2;
		Slash.EndFrame = 6;
		Slash.Hitboxes.AddDefaulted();
		FGSTN_TimelineTrack_Hitbox Thrust = Slash;
		Thrust.StartFrame = 8;
		Thrust.EndFrame = 14;
		Thrust.bOverrideStrike = true;
		Thrust.StrikeKind = EGSTN_StrikeKind::Stab;
		Thrust.bHasRicochetResponse = true;
		Thrust.RicochetResponse.RecoilFrames = 5;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Slash));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Thrust));
		TestTrue(TEXT("the slash group inherits the move's swing"), Data.ResolveStrike(0).Kind == EGSTN_StrikeKind::Swing && Data.ResolveStrike(0).Behaviour == EGSTN_HardSurfaceBehaviour::FinishThenRecoil);
		TestTrue(TEXT("the thrust group is its own stab"), Data.ResolveStrike(1).Kind == EGSTN_StrikeKind::Stab && Data.ResolveStrike(1).Behaviour == EGSTN_HardSurfaceBehaviour::ImmediateRecoil);
		TestTrue(TEXT("no group = the move's"), Data.ResolveStrike(INDEX_NONE).Kind == EGSTN_StrikeKind::Swing);
		TestEqual(TEXT("a group's recoil begins after that group"), Data.GetStrikeEndFrame(0), 6);
		TestEqual(TEXT("the move's active end without a group"), Data.GetStrikeEndFrame(INDEX_NONE), 14);
		TestEqual(TEXT("the thrust's own recoil"), Data.GetRicochetResponse(1)->RecoilFrames, 5);
		TestEqual(TEXT("the slash falls to the move's recoil"), Data.GetRicochetResponse(0)->RecoilFrames, 12);

		// FinishThenRecoil on the slash group: the recoil replaces the rest of the move from the end of that group, with the move's response
		FGSTN_CombatSyncCore Core;
		Core.ActionState = EGSTN_ActionState::InTimeline;
		Core.ActiveTimeline = 0;
		Core.TimelineStartFrame = 100;
		Data.TotalFrames = 40;
		Data.FirstActionableFrame = 30;
		FGSTN_TimelineOutput Output;
		Output.Reserve(Data);
		FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 0, Output);
		for (int32 Frame = 101; Frame <= 103; ++Frame)
		{
			FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output);
		}
		Core.HardContact.VictimId = 2;
		Core.HardContact.TimelineFrame = 3;
		Core.HardContact.bOpposed = true;
		Core.HardContact.bRecoilPending = true;
		Core.StrikeTrack = 0;
		for (int32 Frame = 104; Frame <= 105; ++Frame)
		{
			FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output);
			TestFalse(*FString::Printf(TEXT("frame %d: still the slash"), Frame), Output.bRecoil);
		}
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, 106, &Data, 0, true, Output);
		TestTrue(TEXT("the recoil starts past the slash group, before the thrust"), Output.bRecoil && Core.RecoilStartFrame == 106);
		TestEqual(TEXT("with the slash's (the move's) recoil"), Output.TotalFrames, 12);

		// The group goes on the wire with the recoil and is compared by reconcile
		FGSTN_CombatSyncState State;
		FGSTN_InputSample Sample;
		Sample.Frame = 106;
		State.Core.Input.History.Append(Sample);
		State.Core.ActionState = EGSTN_ActionState::InTimeline;
		State.Core.ActiveTimeline = 1;
		State.Core.TimelineStartFrame = 100;
		State.Core.RecoilStartFrame = 106;
		State.Core.StrikeTrack = 1;
		FBitWriter Writer(1024, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_CombatSyncState Loaded;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		TestFalse(TEXT("the state reads back whole"), Reader.IsError() || !Reader.AtEnd());
		TestEqual(TEXT("the group survives the wire"), static_cast<int32>(Loaded.Core.StrikeTrack), 1);
		FGSTN_CombatSyncCore Other = State.Core;
		Other.StrikeTrack = 0;
		TestTrue(TEXT("a different group asks for a reconcile"), State.Core.ShouldReconcile(Other));
	}

	// A punch: a plain sphere on the root, no weapon and no blade, in an unclassified move. Its group marked Swing slides and recoils like a sword
	{
		FWpScopedRicochet Ricochet;
		FWpFixture Fixture;
		Fixture.MakeWall(FQuat::FindBetweenNormals(FVector::ZAxisVector, FVector::YAxisVector));
		FGSTN_TimelineTrack_Hitbox& Group = Fixture.Timeline.Tracks[0].GetMutable<FGSTN_TimelineTrack_Hitbox>();
		Group.bOverrideStrike = true;
		Group.StrikeKind = EGSTN_StrikeKind::Swing;
		Fixture.RunUntilNormalHit(10, 1);
		TestTrue(TEXT("the punch keeps its swing: a hard contact, not an immediate recoil"), Fixture.Core.HardContact.IsSet());
		TestEqual(TEXT("written by its group"), static_cast<int32>(Fixture.Core.StrikeTrack), 0);
		TestTrue(TEXT("case A: a recoil waits for the end of the swing"), Fixture.Core.HardContact.bRecoilPending);
	}

	// The last fallback: the project default of the group's strike kind, after the weapon's and the kit's
	{
		FGSTN_RicochetResponse Weapon;
		Weapon.RecoilFrames = 8;
		UGSTN_CombatTableSet* Set = NewObject<UGSTN_CombatTableSet>(GetTransientPackage());
		Set->WeaponRicochet = { FGSTN_RicochetResponse(), Weapon };
		Set->KitRicochet = FGSTN_RicochetResponse();
		Set->SwingRicochet.RecoilFrames = 20;
		Set->StabRicochet.RecoilFrames = 9;
		TestEqual(TEXT("a swing with nothing else gets the swing default"), Set->GetRicochetFallback(0, EGSTN_StrikeKind::Swing)->RecoilFrames, 20);
		TestEqual(TEXT("a stab gets the stab default"), Set->GetRicochetFallback(0, EGSTN_StrikeKind::Stab)->RecoilFrames, 9);
		TestNull(TEXT("an unclassified group has no project default"), Set->GetRicochetFallback(0, EGSTN_StrikeKind::Unspecified));
		TestEqual(TEXT("the weapon's own still wins"), Set->GetRicochetFallback(1, EGSTN_StrikeKind::Swing)->RecoilFrames, 8);
	}
	return true;
}

#endif

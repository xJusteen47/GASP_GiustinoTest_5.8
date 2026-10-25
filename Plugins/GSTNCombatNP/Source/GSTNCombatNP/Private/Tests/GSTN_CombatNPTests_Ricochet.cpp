// Automation tests of the defensive half of the hit milestone (M4 H12): the window of the base dodges (data of the ability classes, honoured for the
// affected hurtbox group only) and the ricochet recoil (the timeline ends into the recoil of the response, which is the locked part; the rest is
// the cancellable tail). Pure: sync state in, sync state out, like the other timeline tests. The PIE scenarios dodge_feet, air_dodge_feet,
// ricochet and kit_escape_in_hitstun check the same with two machines.

#include "Abilities/GSTN_GA_Dash.h"
#include "GSTN_CombatCoreTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	const FGSTN_TimelineTrack_HurtboxModifier* FindHurtboxTrack(const FGSTN_CombatTimelineData& Data)
	{
		for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
		{
			const FGSTN_TimelineTrack* Base = Track.GetPtr<FGSTN_TimelineTrack>();
			if (Base && Base->GetKind() == EGSTN_TimelineTrackKind::HurtboxModifier)
			{
				return static_cast<const FGSTN_TimelineTrack_HurtboxModifier*>(Base);
			}
		}
		return nullptr;
	}

	FGSTN_Hurtbox MakeHurtbox(const TCHAR* Name, const TCHAR* Tag)
	{
		FGSTN_Hurtbox Box;
		Box.Name = Name;
		Box.Tags.AddTag(FGameplayTag::RequestGameplayTag(Tag));
		return Box;
	}

	EGSTN_HurtState StateAt(const FGSTN_CombatTimelineData& Data, int32 Frame, const FGSTN_Hurtbox& Box)
	{
		FGSTN_TimelineFrameResult Result;
		Result.Reserve(Data);
		FGSTN_TimelinePlayer::EvaluateHits(Data, Frame, Result);
		return FGSTN_TimelinePlayer::ResolveHurtState(Result, Box).State;
	}

	/** A 60 frame attack that authors a 20 frame recoil (so the recoil and the timeline end on different frames) */
	FGSTN_CombatTimelineData MakeAttack(bool bWithResponse = true)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 60;
		Data.FirstActionableFrame = 50;
		Data.OverrideFrames = 0;
		Data.bHasRicochetResponse = bWithResponse;
		Data.RicochetResponse.RecoilFrames = 20;
		return Data;
	}

	FGSTN_CombatSyncCore MakeRunning(int32 StartFrame)
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, StartFrame, 2, EGSTN_ActivationSource::Command);
		return Core;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPDodgeFeetWindowTest, "GSTNCombat.NP.Hits.Dodge.FeetWindowFromData", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPDodgeFeetWindowTest::RunTest(const FString& Parameters)
{
	const FGSTN_Hurtbox Feet = MakeHurtbox(TEXT("feet"), TEXT("Combat.Hurtbox.Feet"));
	const FGSTN_Hurtbox Torso = MakeHurtbox(TEXT("torso"), TEXT("Combat.Hurtbox.Torso"));
	const UGSTN_GA_MoveTimeline* Dodges[] = { GetDefault<UGSTN_GA_GroundDodge>(), GetDefault<UGSTN_GA_AirDodge>() };
	for (const UGSTN_GA_MoveTimeline* Dodge : Dodges)
	{
		const FString Name = Dodge->GetName();
		const FGSTN_CombatTimelineData& Data = Dodge->GetTimelineData();
		const FGSTN_TimelineTrack_HurtboxModifier* Window = FindHurtboxTrack(Data);
		if (!TestNotNull(*(Name + TEXT(" carries a hurtbox window")), Window))
		{
			continue;
		}
		TestEqual(*(Name + TEXT(" window is intangible")), Window->Kind, EGSTN_HurtboxModifierKind::Intangible);
		TestTrue(*(Name + TEXT(" window addresses the feet by tag")), Window->AffectedTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Feet"))));
		TestEqual(*(Name + TEXT(" window starts at take-off")), Window->StartFrame, 0);
		TestTrue(*(Name + TEXT(" window ends before the timeline does")), Window->EndFrame > 0 && Window->EndFrame < Data.TotalFrames);

		// Only the feet, only the initial part (S section 12 4b)
		TestEqual(*(Name + TEXT(" feet intangible at take-off")), StateAt(Data, 0, Feet), EGSTN_HurtState::Intangible);
		TestEqual(*(Name + TEXT(" feet intangible on the last window frame")), StateAt(Data, Window->EndFrame - 1, Feet), EGSTN_HurtState::Intangible);
		TestEqual(*(Name + TEXT(" feet hittable after the window")), StateAt(Data, Window->EndFrame, Feet), EGSTN_HurtState::Normal);
		for (const int32 Frame : { 0, Window->EndFrame - 1, Window->EndFrame, Data.TotalFrames - 1 })
		{
			TestEqual(*FString::Printf(TEXT("%s torso hittable at frame %d"), *Name, Frame), StateAt(Data, Frame, Torso), EGSTN_HurtState::Normal);
		}

		// The window is data: a designer who moves its end moves the hittable frame, and nothing else decides it
		FGSTN_CombatTimelineData Reshaped = Data;
		for (TInstancedStruct<FGSTN_TimelineTrack>& Track : Reshaped.Tracks)
		{
			if (FGSTN_TimelineTrack* Base = Track.GetMutablePtr<FGSTN_TimelineTrack>(); Base && Base->GetKind() == EGSTN_TimelineTrackKind::HurtboxModifier)
			{
				Base->EndFrame = Window->EndFrame + 5;
			}
		}
		TestEqual(*(Name + TEXT(" reshaped window keeps the feet intangible five frames longer")), StateAt(Reshaped, Window->EndFrame + 4, Feet), EGSTN_HurtState::Intangible);
		TestEqual(*(Name + TEXT(" and ends where the data says")), StateAt(Reshaped, Window->EndFrame + 5, Feet), EGSTN_HurtState::Normal);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPRicochetEndsTimelineTest, "GSTNCombat.NP.Hits.Ricochet.EndsTimelineIntoRecoil", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPRicochetEndsTimelineTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatTimelineData Data = MakeAttack();
	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 0, Output);
	for (int32 Frame = 101; Frame <= 110; ++Frame)
	{
		TestTrue(TEXT("the timeline runs until the bounce"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output));
		TestFalse(TEXT("and plays its own frames"), Output.bRecoil);
	}

	// The attacker side decided a ricochet on frame 110
	TestTrue(TEXT("the recoil starts"), FGSTN_CombatTimelineLogic::StartRecoil(Core, 110, &Data, nullptr));
	TestEqual(TEXT("recoil frame 0 is the next frame"), Core.RecoilStartFrame, 111);
	TestFalse(TEXT("a second bounce does not restart it"), FGSTN_CombatTimelineLogic::StartRecoil(Core, 111, &Data, nullptr));
	TestEqual(TEXT("the first start stays"), Core.RecoilStartFrame, 111);

	int32 StartsNow = 0;
	for (int32 Frame = 111; Frame < 131; ++Frame)
	{
		TestTrue(*FString::Printf(TEXT("the recoil runs on frame %d"), Frame), FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output));
		TestTrue(TEXT("and it is the recoil that is evaluated"), Output.bRecoil);
		TestEqual(TEXT("with the recoil's length"), Output.TotalFrames, 20);
		TestEqual(TEXT("at its own frame"), Output.Frame, Frame - 111);
		StartsNow += Output.bRecoilStartsNow ? 1 : 0;
		TestTrue(TEXT("the combatant stays in its timeline"), Core.ActionState == EGSTN_ActionState::InTimeline);
	}
	TestEqual(TEXT("the recoil started once"), StartsNow, 1);

	// The timeline had 30 frames to go: the recoil ended it
	TestFalse(TEXT("the recoil ends the timeline on its last frame"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, 131, &Data, 0, true, Output));
	TestTrue(TEXT("back to actionable"), Core.ActionState == EGSTN_ActionState::Actionable);
	TestEqual(TEXT("and the recoil is forgotten"), Core.RecoilStartFrame, static_cast<int32>(INDEX_NONE));

	// No response, no recoil: the bounce changes nothing about the attacker
	const FGSTN_CombatTimelineData Plain = MakeAttack(false);
	FGSTN_CombatSyncCore Other = MakeRunning(100);
	TestFalse(TEXT("an attack with no response and no fallback does not recoil"), FGSTN_CombatTimelineLogic::StartRecoil(Other, 110, &Plain, nullptr));
	TestEqual(TEXT("nothing was written"), Other.RecoilStartFrame, static_cast<int32>(INDEX_NONE));
	FGSTN_RicochetResponse Fallback;
	Fallback.RecoilFrames = 12;
	TestTrue(TEXT("the fallback recoils it"), FGSTN_CombatTimelineLogic::StartRecoil(Other, 110, &Plain, &Fallback));
	FGSTN_CombatTimelineLogic::AdvanceClock(Other, 111, &Plain, 0, true, Output, false, &Fallback);
	TestEqual(TEXT("with the fallback's length"), Output.TotalFrames, 12);
	FGSTN_CombatSyncCore Idle;
	TestFalse(TEXT("a combatant that is not in a timeline cannot recoil"), FGSTN_CombatTimelineLogic::StartRecoil(Idle, 5, &Data, nullptr));

	// The start frame is part of the state that goes on the wire and is compared by reconcile
	FGSTN_CombatSyncState State;
	FGSTN_InputSample Sample;
	Sample.Frame = 111;
	State.Core.Input.History.Append(Sample);
	State.Core.ActionState = EGSTN_ActionState::InTimeline;
	State.Core.ActiveTimeline = 2;
	State.Core.TimelineStartFrame = 100;
	State.Core.RecoilStartFrame = 111;
	FBitWriter Writer(1024, true);
	State.NetSerialize(FNetSerializeParams(Writer));
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_CombatSyncState Loaded;
	Loaded.NetSerialize(FNetSerializeParams(Reader));
	TestFalse(TEXT("the state reads back whole"), Reader.IsError() || !Reader.AtEnd());
	TestEqual(TEXT("the recoil start survives the wire"), Loaded.Core.RecoilStartFrame, 111);
	FGSTN_CombatSyncCore Without = State.Core;
	Without.RecoilStartFrame = INDEX_NONE;
	TestTrue(TEXT("a different recoil start asks for a reconcile"), State.Core.ShouldReconcile(Without));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPRicochetTailTest, "GSTNCombat.NP.Hits.Ricochet.RecoilTailCancellable", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPRicochetTailTest::RunTest(const FString& Parameters)
{
	// RecoilFrames is the locked part (S 7: the attacker is punishable only then); what the montage has left is the cancellable tail
	const FGSTN_CombatTimelineData Data = MakeAttack();
	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 0, Output);
	FGSTN_CombatTimelineLogic::AdvanceClock(Core, 101, &Data, 0, true, Output);
	FGSTN_CombatTimelineLogic::StartRecoil(Core, 101, &Data, nullptr);

	const FGameplayTag Command = GSTNCommandTags::Special;
	for (int32 Frame = 102; Frame < 122; ++Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output);
		TestFalse(*FString::Printf(TEXT("frame %d of the recoil is not a tail"), Frame), Output.bTailCancel);
		TestFalse(*FString::Printf(TEXT("a command is refused on locked frame %d"), Frame),
			FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, Command, 9, 0));
	}
	FGSTN_CombatTimelineLogic::AdvanceClock(Core, 122, &Data, 0, true, Output);
	TestTrue(TEXT("the first frame after the locked part is actionable"), Core.ActionState == EGSTN_ActionState::Actionable);
	TestTrue(TEXT("and any command is allowed"), FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, Command, 0, 0));

	// A hitstop frame pauses the recoil like it pauses a timeline: the locked part ends one frame later per paused frame
	FGSTN_CombatSyncCore Paused = MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Paused, 100, Data, 0, Output);
	FGSTN_CombatTimelineLogic::StartRecoil(Paused, 100, &Data, nullptr);
	int32 EndFrame = INDEX_NONE;
	for (int32 Frame = 101; Frame < 140 && EndFrame == INDEX_NONE; ++Frame)
	{
		const bool bHitstop = Frame == 105 || Frame == 106;
		if (!FGSTN_CombatTimelineLogic::AdvanceClock(Paused, Frame, &Data, 0, true, Output, bHitstop))
		{
			EndFrame = Frame;
		}
		else if (bHitstop)
		{
			TestEqual(TEXT("the recoil frame stands still in hitstop"), Output.Frame, 3);
		}
	}
	TestEqual(TEXT("two paused frames, two frames later"), EndFrame, 101 + 20 + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPRicochetFallbackTest, "GSTNCombat.NP.Hits.Ricochet.FallbackOrder", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPRicochetFallbackTest::RunTest(const FString& Parameters)
{
	// Per attack, then per weapon, then per kit (C2/C5)
	FGSTN_RicochetResponse Weapon;
	Weapon.RecoilFrames = 8;
	FGSTN_RicochetResponse Kit;
	Kit.RecoilFrames = 16;
	UGSTN_CombatTableSet* Set = NewObject<UGSTN_CombatTableSet>(GetTransientPackage());
	Set->WeaponRicochet = { FGSTN_RicochetResponse(), Weapon };
	Set->KitRicochet = Kit;
	TestEqual(TEXT("a weapon with no response falls to the kit's"), Set->GetRicochetFallback(0)->RecoilFrames, 16);
	TestEqual(TEXT("a weapon's own wins over the kit's"), Set->GetRicochetFallback(1)->RecoilFrames, 8);
	TestEqual(TEXT("a weapon the loadout does not have falls to the kit's"), Set->GetRicochetFallback(5)->RecoilFrames, 16);
	Set->KitRicochet = FGSTN_RicochetResponse();
	TestNull(TEXT("nothing authored, nothing to fall to"), Set->GetRicochetFallback(0));

	const FGSTN_CombatTimelineData Own = MakeAttack();
	const FGSTN_CombatTimelineData None = MakeAttack(false);
	TestEqual(TEXT("the attack's own response wins over every fallback"), FGSTN_CombatTimelineLogic::ResolveRicochet(&Own, &Weapon)->RecoilFrames, 20);
	TestEqual(TEXT("without one the fallback answers"), FGSTN_CombatTimelineLogic::ResolveRicochet(&None, &Weapon)->RecoilFrames, 8);
	TestNull(TEXT("and with neither there is none"), FGSTN_CombatTimelineLogic::ResolveRicochet(&None, nullptr));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Automation tests of the activation gate in a hit reaction (M4 H13b): the tag Combat.Ability.IgnoreHitstunOnActivation and the burst flag
// UGSTN_GA_Base::bEndReactionOnActivation. Pure: ability class defaults in, flags out; sync state in, sync state out. The PIE scenarios
// kit_escape_in_hitstun and burst_ends_reaction check the same with two machines.

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_CombatTags.h"
#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Tests/GSTN_ReviewTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPGateIgnoreHitstunTest, "GSTNCombat.NP.Hits.Gate.IgnoreHitstunOnActivation", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPGateIgnoreHitstunTest::RunTest(const FString& Parameters)
{
	UGSTN_TestMontageTimelineAbility* Plain = NewObject<UGSTN_TestMontageTimelineAbility>(GetTransientPackage());
	TestEqual(TEXT("a plain ability has no reaction flag"), UGSTN_CombatTableSet::MakeReactionFlags(Plain), static_cast<uint8>(0));
	TestFalse(TEXT("the burst flag is off by default"), Plain->bEndReactionOnActivation);

	UGSTN_TestMontageTimelineAbility* Tagged = NewObject<UGSTN_TestMontageTimelineAbility>(GetTransientPackage());
	Tagged->EditorGetAssetTags().AddTag(GSTNCombatTags::Ability_IgnoreHitstunOnActivation);
	TestEqual(TEXT("the tag opens the gate and nothing else"), UGSTN_CombatTableSet::MakeReactionFlags(Tagged), UGSTN_CombatTableSet::ReactionIgnoresHitstun);

	TestEqual(TEXT("no class, no flags"), UGSTN_CombatTableSet::MakeReactionFlags(static_cast<const UClass*>(nullptr)), static_cast<uint8>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPGateEndReactionTest, "GSTNCombat.NP.Hits.Gate.EndReactionOnActivation", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPGateEndReactionTest::RunTest(const FString& Parameters)
{
	UGSTN_TestMontageTimelineAbility* Burst = NewObject<UGSTN_TestMontageTimelineAbility>(GetTransientPackage());
	Burst->bEndReactionOnActivation = true;
	TestEqual(TEXT("the flag is read from the ability"), UGSTN_CombatTableSet::MakeReactionFlags(Burst), UGSTN_CombatTableSet::ReactionEndsReaction);

	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::Tumble;
	Core.Hit.HitstunEndFrame = 140;
	Core.Hit.ImpactEndFrame = 150;
	Core.HitstopStartFrame = 95;
	Core.HitstopEndFrame = 105;
	Core.Hit.SetPendingReaction(EGSTN_HitReaction::Launch);
	Core.Hit.SetPendingLaunch(FVector(100.0, 0.0, 200.0));

	FGSTN_CombatVictim::EndReaction(Core, 100);
	TestEqual(TEXT("the reaction is over"), Core.ActionState, EGSTN_ActionState::Actionable);
	TestFalse(TEXT("the pending launch is dropped"), Core.Hit.HasPendingReaction());
	TestEqual(TEXT("the hitstun ends this frame"), Core.Hit.HitstunEndFrame, 100);
	TestFalse(TEXT("the window closes at this frame"), Core.IsInHitstop(100));
	TestTrue(TEXT("and its past stays"), Core.HitstopStartFrame == 95 && Core.HitstopEndFrame == 100);
	TestFalse(TEXT("the command lock is gone"), FGSTN_CombatVictim::IsActionLocked(Core));

	// Idempotent, and a KO is the one state a burst does not leave
	FGSTN_CombatSyncCore Again = Core;
	FGSTN_CombatVictim::EndReaction(Again, 101);
	TestTrue(TEXT("a second burst changes nothing"), Again.Hit.HitstunEndFrame == 100 && Again.ActionState == EGSTN_ActionState::Actionable);
	FGSTN_CombatSyncCore Out;
	Out.ActionState = EGSTN_ActionState::KO;
	FGSTN_CombatVictim::EndReaction(Out, 100);
	TestEqual(TEXT("a KO stays a KO"), Out.ActionState, EGSTN_ActionState::KO);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

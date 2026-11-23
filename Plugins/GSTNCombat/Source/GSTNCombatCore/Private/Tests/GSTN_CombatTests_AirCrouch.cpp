// Automation tests of the air crouch rule (owner 2026-11-16): a pure function of the frame's facts, so every machine projects the same state.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "GSTN_CombatCoreTags.h"
#include "Rules/GSTN_AirCrouch.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirCrouchHoldsTest, "GSTNCombat.Core.AirCrouch.Holds", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirCrouchHoldsTest::RunTest(const FString& Parameters)
{
	const FGameplayTagContainer NoTags;
	FGSTN_AirCrouchKitExtension AirCrouch;
	TestTrue(TEXT("airborne with the button held"), AirCrouch.Holds(true, true, false, false, NoTags));
	TestFalse(TEXT("on the ground it is the ground crouch"), AirCrouch.Holds(false, true, false, false, NoTags));
	TestFalse(TEXT("released"), AirCrouch.Holds(true, false, false, false, NoTags));
	TestFalse(TEXT("a hit reaction or a KO blocks it"), AirCrouch.Holds(true, true, false, true, NoTags));
	TestFalse(TEXT("a move owns the pose by default"), AirCrouch.Holds(true, true, true, false, NoTags));

	AirCrouch.bKeepDuringMoves = true;
	TestTrue(TEXT("kept through moves when asked"), AirCrouch.Holds(true, true, true, false, NoTags));

	AirCrouch.bEnabled = false;
	TestFalse(TEXT("off in the kit"), AirCrouch.Holds(true, true, false, false, NoTags));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirCrouchDataTest, "GSTNCombat.Core.AirCrouch.Data", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirCrouchDataTest::RunTest(const FString& Parameters)
{
	FGSTN_AirCrouchKitExtension AirCrouch;
	TestEqual(TEXT("no button authored = the standard crouch button"), AirCrouch.ResolveHoldButton(), GSTNCombatButtonTags::Button_Crouch.GetTag());
	AirCrouch.HoldButton = GSTNCombatButtonTags::Button_Shield.GetTag();
	TestEqual(TEXT("an authored button wins"), AirCrouch.ResolveHoldButton(), GSTNCombatButtonTags::Button_Shield.GetTag());

	// A designer block: the tag on the owner stops it, an unrelated tag does not
	AirCrouch.BlockedTags.AddTag(GSTNCombatTags::State_Hitstop);
	FGameplayTagContainer Owner;
	Owner.AddTag(GSTNCombatTags::State_Busy);
	TestTrue(TEXT("unrelated owner tag"), AirCrouch.Holds(true, true, false, false, Owner));
	Owner.AddTag(GSTNCombatTags::State_Hitstop);
	TestFalse(TEXT("blocked tag on the owner"), AirCrouch.Holds(true, true, false, false, Owner));

	TestEqual(TEXT("the state tag keeps its name"), GSTNCombatTags::State_AirCrouch.GetTag().ToString(), FString(TEXT("Combat.State.AirCrouch")));
	return true;
}

#endif

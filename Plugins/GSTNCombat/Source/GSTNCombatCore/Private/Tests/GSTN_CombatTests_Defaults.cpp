#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_InputTags.h"
#include "Input/GSTN_StandardSlots.h"
#include "Misc/DataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNCombatTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StandardSlotsSetAssetTest, "GSTNCombat.Core.StandardSlots.SlotSetAsset", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StandardSlotsSetAssetTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	const TStrongObjectPtr<UGSTN_InputSlotSet> Set(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
	UGSTN_StandardSlots::FillSlotSet(*Set);
	TestEqual(TEXT("the standard set has 16 slots"), Set->Slots.Num(), 16);
	{
		FDataValidationContext Context;
		TestTrue(TEXT("the standard set is valid"), Set->IsDataValid(Context) == EDataValidationResult::Valid);
		TestEqual(TEXT("with no error"), Context.GetNumErrors(), 0);
		TestEqual(TEXT("and no warning"), Context.GetNumWarnings(), 0);
	}

	// A deliberate conflict: a copy of a slot under another command.
	FGSTN_InputSlotEntry Copy = Set->Slots[0];
	Copy.Inline.Command = GSTNCommandTags::Attack;
	Set->Slots.Add(Copy);
	{
		FDataValidationContext Context;
		TestEqual(TEXT("a copy of a slot is invalid"), Set->IsDataValid(Context), EDataValidationResult::Invalid);
		TestTrue(TEXT("with an error"), Context.GetNumErrors() > 0);
	}

	// A slot shared as an asset is used in place of the inline one.
	const FGameplayTag SharedCommand = Set->Slots[4].Inline.Command;
	const TStrongObjectPtr<UGSTN_InputSlotAsset> Asset(NewObject<UGSTN_InputSlotAsset>(GetTransientPackage()));
	Asset->Spec = Set->Slots[4].Inline;
	FGSTN_InputSlotEntry Shared;
	Shared.Asset = Asset.Get();
	TestEqual(TEXT("an entry with an asset reads the asset"), Shared.GetSpec().Command, SharedCommand);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CombatNativeTagsTest, "GSTNCombat.Core.NativeTags", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CombatNativeTagsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("attack button tag"), GSTNCombatButtonTags::Button_Attack.GetTag().IsValid());
	TestEqual(TEXT("attack button tag name"), GSTNCombatButtonTags::Button_Attack.GetTag().GetTagName(), FName(TEXT("Input.Button.Attack")));
	TestTrue(TEXT("buttons are under the input plugin's root"), GSTNCombatButtonTags::Button_Jump.GetTag().MatchesTag(GSTNInputTags::Button.GetTag()));
	TestTrue(TEXT("strong button tag"), GSTNCombatButtonTags::Button_Strong.GetTag().IsValid());
	TestTrue(TEXT("crouch button tag"), GSTNCombatButtonTags::Button_Crouch.GetTag().IsValid());

	TestEqual(TEXT("command tag name"), GSTNCommandTags::Strong_CrouchDirectional.GetTag().GetTagName(), FName(TEXT("Command.Strong.CrouchDirectional")));
	TestTrue(TEXT("attack neutral is under the attack root"),
		GSTNCommandTags::Attack_Neutral.GetTag().MatchesTag(GSTNCommandTags::Attack.GetTag()));
	TestTrue(TEXT("commands are under the input plugin's root"), GSTNCommandTags::Attack_Neutral.GetTag().MatchesTag(GSTNInputTags::Command.GetTag()));
	TestFalse(TEXT("a context tag is not a command"), GSTNInputTags::Context_Airborne.GetTag().MatchesTag(GSTNCommandTags::Attack.GetTag()));
	TestTrue(TEXT("combo command"), GSTNCommandTags::Combo_AttackSpecial_CrouchNeutral.GetTag().MatchesTag(GSTNCommandTags::Combo.GetTag()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CombatSettingsDefaultsTest, "GSTNCombat.Core.SettingsDefaults", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CombatSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	// Reads the project's settings (Config/DefaultGame.ini): only the structural defaults are checked, the numbers are designer tuning.
	const UGSTN_InputSettings& Settings = UGSTN_InputSettings::Get();
	TestTrue(TEXT("attack tier is configured"), Settings.GetCommandTier(GSTNCommandTags::Attack_Neutral) > 0);
	TestTrue(TEXT("special beats strong beats attack"),
		Settings.GetCommandTier(GSTNCommandTags::Special_Neutral) > Settings.GetCommandTier(GSTNCommandTags::Strong_Neutral)
		&& Settings.GetCommandTier(GSTNCommandTags::Strong_Neutral) > Settings.GetCommandTier(GSTNCommandTags::Attack_Neutral));
	TestTrue(TEXT("combo beats special"),
		Settings.GetCommandTier(GSTNCommandTags::Combo_AttackSpecial_Neutral) > Settings.GetCommandTier(GSTNCommandTags::Special_Neutral));
	TestTrue(TEXT("attack buttons are exclusive"), Settings.ExclusiveButtons.Contains(GSTNCombatButtonTags::Button_Attack.GetTag()));
	TestFalse(TEXT("jump is not exclusive"), Settings.ExclusiveButtons.Contains(GSTNCombatButtonTags::Button_Jump.GetTag()));
	TestTrue(TEXT("dead zone below aim zone"), Settings.StickDeadZone < Settings.AimZone);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#include "Tests/GSTN_TestUtils.h"
#include "InputAction.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatInputConfig.h"
#include "Input/GSTN_CombatInputTags.h"
#include "Misc/DataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DirectionMaskTest, "GSTNCombat.Core.Input.Types.DirectionMasks", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DirectionMaskTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;

	TestEqual(TEXT("Neutral is bit 0"), static_cast<int32>(Neutral), 1);
	TestEqual(TEXT("AnyTilted is every direction but Neutral"), static_cast<int32>(AnyTilted), static_cast<int32>(All & ~Neutral));
	TestEqual(TEXT("All has one bit per direction"), FMath::CountBits(All), GSTN_NumStickDirections);

	for (int32 Direction = 0; Direction < GSTN_NumStickDirections; ++Direction)
	{
		const EGSTN_StickDirection Value = static_cast<EGSTN_StickDirection>(Direction);
		TestTrue(TEXT("every direction is in All"), Contains(All, Value));
		TestEqual(TEXT("Neutral is the only direction outside AnyTilted"), Contains(AnyTilted, Value), Direction != 0);
	}

	TestTrue(TEXT("UpLeft is forward"), Contains(FacingForward, EGSTN_StickDirection::UpLeft));
	TestFalse(TEXT("Left is not forward"), Contains(FacingForward, EGSTN_StickDirection::Left));
	TestTrue(TEXT("DownRight is back"), Contains(FacingBack, EGSTN_StickDirection::DownRight));
	TestEqual(TEXT("forward and back do not overlap"), static_cast<int32>(FacingForward & FacingBack), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NativeTagsTest, "GSTNCombat.Core.Input.Types.NativeTags", GSTN_CORE_TEST_FLAGS)
bool FGSTN_NativeTagsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("attack button tag"), GSTNInputTags::Button_Attack.GetTag().IsValid());
	TestEqual(TEXT("attack button tag name"), GSTNInputTags::Button_Attack.GetTag().GetTagName(), FName(TEXT("Input.Combat.Button.Attack")));
	TestTrue(TEXT("strong button tag"), GSTNInputTags::Button_Strong.GetTag().IsValid());
	TestTrue(TEXT("crouch button tag"), GSTNInputTags::Button_Crouch.GetTag().IsValid());

	TestEqual(TEXT("command tag name"), GSTNCommandTags::Strong_CrouchDirectional.GetTag().GetTagName(), FName(TEXT("Command.Strong.CrouchDirectional")));
	TestEqual(TEXT("grounded context"), GSTNCommandTags::Context_Grounded.GetTag().GetTagName(), FName(TEXT("Combat.Context.Grounded")));
	TestEqual(TEXT("airborne context"), GSTNCommandTags::Context_Airborne.GetTag().GetTagName(), FName(TEXT("Combat.Context.Airborne")));
	TestEqual(TEXT("on wall context"), GSTNCommandTags::Context_OnWall.GetTag().GetTagName(), FName(TEXT("Combat.Context.OnWall")));
	TestTrue(TEXT("attack neutral is under the attack root"),
		GSTNCommandTags::Attack_Neutral.GetTag().MatchesTag(GSTNCommandTags::Attack.GetTag()));
	TestFalse(TEXT("a context tag is not a command"), GSTNCommandTags::Context_Airborne.GetTag().MatchesTag(GSTNCommandTags::Attack.GetTag()));
	TestTrue(TEXT("combo command"), GSTNCommandTags::Combo_AttackSpecial_CrouchNeutral.GetTag().MatchesTag(GSTNCommandTags::Combo.GetTag()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SettingsTiersTest, "GSTNCombat.Core.Input.Settings.CommandTiers", GSTN_CORE_TEST_FLAGS)
bool FGSTN_SettingsTiersTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = GSTNTest::MakeSettings();
	Settings->CommandTiers.Empty();
	Settings->CommandTiers.Add(GSTNCommandTags::Attack, 100);
	Settings->CommandTiers.Add(GSTNCommandTags::Special, 300);
	Settings->CommandTiers.Add(GSTNCommandTags::Attack_CrouchDirectional, 250);

	TestEqual(TEXT("a root has its own tier"), Settings->GetCommandTier(GSTNCommandTags::Attack), 100);
	TestEqual(TEXT("a leaf takes the tier of its root"), Settings->GetCommandTier(GSTNCommandTags::Attack_Neutral), 100);
	TestEqual(TEXT("special leaf"), Settings->GetCommandTier(GSTNCommandTags::Special_Directional), 300);
	TestEqual(TEXT("the nearest listed ancestor wins"), Settings->GetCommandTier(GSTNCommandTags::Attack_CrouchDirectional), 250);
	TestEqual(TEXT("an unlisted root is tier 0"), Settings->GetCommandTier(GSTNCommandTags::Strong_Neutral), 0);
	TestEqual(TEXT("an unrelated tag is tier 0"), Settings->GetCommandTier(GSTNCommandTags::Context_Airborne), 0);
	TestEqual(TEXT("an invalid tag is tier 0"), Settings->GetCommandTier(FGameplayTag()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SettingsDefaultsTest, "GSTNCombat.Core.Input.Settings.Defaults", GSTN_CORE_TEST_FLAGS)
bool FGSTN_SettingsDefaultsTest::RunTest(const FString& Parameters)
{
	// Reads the project's settings: only the structural defaults are checked, the numbers are designer tuning.
	const UGSTN_CombatInputSettings& Settings = UGSTN_CombatInputSettings::Get();
	TestTrue(TEXT("attack tier is configured"), Settings.GetCommandTier(GSTNCommandTags::Attack_Neutral) > 0);
	TestTrue(TEXT("special beats strong beats attack"),
		Settings.GetCommandTier(GSTNCommandTags::Special_Neutral) > Settings.GetCommandTier(GSTNCommandTags::Strong_Neutral)
		&& Settings.GetCommandTier(GSTNCommandTags::Strong_Neutral) > Settings.GetCommandTier(GSTNCommandTags::Attack_Neutral));
	TestTrue(TEXT("combo beats special"),
		Settings.GetCommandTier(GSTNCommandTags::Combo_AttackSpecial_Neutral) > Settings.GetCommandTier(GSTNCommandTags::Special_Neutral));
	TestTrue(TEXT("attack buttons are exclusive"), Settings.ExclusiveButtons.Contains(GSTNInputTags::Button_Attack.GetTag()));
	TestFalse(TEXT("jump is not exclusive"), Settings.ExclusiveButtons.Contains(GSTNInputTags::Button_Jump.GetTag()));
	TestTrue(TEXT("dead zone below aim zone"), Settings.StickDeadZone < Settings.AimZone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputConfigTest, "GSTNCombat.Core.Input.Config.Buttons", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputConfigTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputConfig> Config(NewObject<UGSTN_CombatInputConfig>(GetTransientPackage()));
	UInputAction* AttackAction = NewObject<UInputAction>(GetTransientPackage());
	UInputAction* SpecialAction = NewObject<UInputAction>(GetTransientPackage());
	const TStrongObjectPtr<UInputAction> KeepAttack(AttackAction);
	const TStrongObjectPtr<UInputAction> KeepSpecial(SpecialAction);

	Config->Buttons.Add({ GSTNInputTags::Button_Attack.GetTag(), AttackAction });
	Config->Buttons.Add({ GSTNInputTags::Button_Special.GetTag(), SpecialAction });

	TestEqual(TEXT("index is the array position"), Config->FindButtonIndex(GSTNInputTags::Button_Special.GetTag()), 1);
	TestEqual(TEXT("mask is the bit of the index"), Config->GetButtonMask(GSTNInputTags::Button_Special.GetTag()), 2u);
	TestEqual(TEXT("unknown button"), Config->FindButtonIndex(GSTNInputTags::Button_Jump.GetTag()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("unknown button mask"), Config->GetButtonMask(GSTNInputTags::Button_Jump.GetTag()), 0u);
	TestEqual(TEXT("invalid tag"), Config->FindButtonIndex(FGameplayTag()), static_cast<int32>(INDEX_NONE));

#if WITH_EDITOR
	{
		FDataValidationContext Context;
		TestTrue(TEXT("a good config is valid"), Config->IsDataValid(Context) != EDataValidationResult::Invalid);
		TestEqual(TEXT("a good config has no error"), Context.GetNumErrors(), 0);
	}
	{
		Config->Buttons.Add({ GSTNInputTags::Button_Attack.GetTag(), SpecialAction });
		FDataValidationContext Context;
		TestEqual(TEXT("duplicate tag and action"), Config->IsDataValid(Context), EDataValidationResult::Invalid);
		TestEqual(TEXT("both duplicates are reported"), Context.GetNumErrors(), 2);
		Config->Buttons.Pop();
	}
	{
		Config->Buttons.Add(FGSTN_CombatButtonDef());
		FDataValidationContext Context;
		TestEqual(TEXT("a button without tag"), Config->IsDataValid(Context), EDataValidationResult::Invalid);
		Config->Buttons.Pop();
	}
	{
		Config->Buttons.Reset();
		for (int32 Index = 0; Index <= UGSTN_CombatInputConfig::MaxButtons; ++Index)
		{
			Config->Buttons.Add({ FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Combat.Button.Attack"))), nullptr });
		}
		FDataValidationContext Context;
		TestEqual(TEXT("more buttons than bits"), Config->IsDataValid(Context), EDataValidationResult::Invalid);
	}
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

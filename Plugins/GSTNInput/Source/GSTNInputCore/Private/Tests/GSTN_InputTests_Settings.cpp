#include "Tests/GSTN_TestUtils.h"
#include "InputAction.h"
#include "Input/GSTN_InputConfig.h"
#include "Misc/DataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DirectionMaskTest, "GSTNInput.Core.Input.Types.DirectionMasks", GSTN_CORE_TEST_FLAGS)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SettingsTiersTest, "GSTNInput.Core.Input.Settings.CommandTiers", GSTN_CORE_TEST_FLAGS)
bool FGSTN_SettingsTiersTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNTest::MakeSettings();
	Settings->CommandTiers.Empty();
	Settings->CommandTiers.Add(GSTNTestTags::Attack, 100);
	Settings->CommandTiers.Add(GSTNTestTags::Special, 300);
	Settings->CommandTiers.Add(GSTNTestTags::Attack_CrouchDirectional, 250);

	TestEqual(TEXT("a root has its own tier"), Settings->GetCommandTier(GSTNTestTags::Attack), 100);
	TestEqual(TEXT("a leaf takes the tier of its root"), Settings->GetCommandTier(GSTNTestTags::Attack_Neutral), 100);
	TestEqual(TEXT("special leaf"), Settings->GetCommandTier(GSTNTestTags::Special_Directional), 300);
	TestEqual(TEXT("the nearest listed ancestor wins"), Settings->GetCommandTier(GSTNTestTags::Attack_CrouchDirectional), 250);
	TestEqual(TEXT("an unlisted root is tier 0"), Settings->GetCommandTier(GSTNTestTags::Strong_Neutral), 0);
	TestEqual(TEXT("an unrelated tag is tier 0"), Settings->GetCommandTier(GSTNInputTags::Context_Airborne), 0);
	TestEqual(TEXT("an invalid tag is tier 0"), Settings->GetCommandTier(FGameplayTag()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputConfigTest, "GSTNInput.Core.Input.Config.Buttons", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputConfigTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputConfig> Config(NewObject<UGSTN_InputConfig>(GetTransientPackage()));
	UInputAction* AttackAction = NewObject<UInputAction>(GetTransientPackage());
	UInputAction* SpecialAction = NewObject<UInputAction>(GetTransientPackage());
	const TStrongObjectPtr<UInputAction> KeepAttack(AttackAction);
	const TStrongObjectPtr<UInputAction> KeepSpecial(SpecialAction);

	Config->Buttons.Add({ GSTNTestTags::Button_Attack.GetTag(), TSoftObjectPtr<UInputAction>(AttackAction) });
	Config->Buttons.Add({ GSTNTestTags::Button_Special.GetTag(), TSoftObjectPtr<UInputAction>(SpecialAction) });

	TestEqual(TEXT("index is the array position"), Config->FindButtonIndex(GSTNTestTags::Button_Special.GetTag()), 1);
	TestEqual(TEXT("mask is the bit of the index"), Config->GetButtonMask(GSTNTestTags::Button_Special.GetTag()), 2u);
	TestEqual(TEXT("unknown button"), Config->FindButtonIndex(GSTNTestTags::Button_Jump.GetTag()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("unknown button mask"), Config->GetButtonMask(GSTNTestTags::Button_Jump.GetTag()), 0u);
	TestEqual(TEXT("invalid tag"), Config->FindButtonIndex(FGameplayTag()), static_cast<int32>(INDEX_NONE));

	// Soft actions: nothing is resolved until the host asks, then the getters hand out the loaded ones
	TestNull(TEXT("actions are not resolved before ResolveActions"), Config->GetButtonAction(0));
	Config->MoveAction = TSoftObjectPtr<UInputAction>(SpecialAction);
	Config->ResolveActions();
	TestEqual(TEXT("button 0 resolves to its action"), Config->GetButtonAction(0), AttackAction);
	TestEqual(TEXT("button 1 resolves to its action"), Config->GetButtonAction(1), SpecialAction);
	TestNull(TEXT("a button past the end has no action"), Config->GetButtonAction(5));
	TestEqual(TEXT("the move action resolves"), Config->GetMoveAction(), SpecialAction);

#if WITH_EDITOR
	{
		FDataValidationContext Context;
		TestTrue(TEXT("a good config is valid"), Config->IsDataValid(Context) != EDataValidationResult::Invalid);
		TestEqual(TEXT("a good config has no error"), Context.GetNumErrors(), 0);
	}
	{
		Config->Buttons.Add({ GSTNTestTags::Button_Attack.GetTag(), TSoftObjectPtr<UInputAction>(SpecialAction) });
		FDataValidationContext Context;
		TestEqual(TEXT("duplicate tag and action"), Config->IsDataValid(Context), EDataValidationResult::Invalid);
		TestEqual(TEXT("both duplicates are reported"), Context.GetNumErrors(), 2);
		Config->Buttons.Pop();
	}
	{
		Config->Buttons.Add(FGSTN_InputButtonDef());
		FDataValidationContext Context;
		TestEqual(TEXT("a button without tag"), Config->IsDataValid(Context), EDataValidationResult::Invalid);
		Config->Buttons.Pop();
	}
	{
		Config->Buttons.Reset();
		for (int32 Index = 0; Index <= UGSTN_InputConfig::MaxButtons; ++Index)
		{
			Config->Buttons.Add({ GSTNTestTags::Button_Attack.GetTag(), TSoftObjectPtr<UInputAction>() });
		}
		FDataValidationContext Context;
		TestEqual(TEXT("more buttons than bits"), Config->IsDataValid(Context), EDataValidationResult::Invalid);
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SettingsContributionTest, "GSTNInput.Core.Input.Settings.Contributions", GSTN_CORE_TEST_FLAGS)
bool FGSTN_SettingsContributionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNTest::MakeSettings();
	Settings->CommandTiers.Empty();
	Settings->ExclusiveButtons.Empty();
	TestEqual(TEXT("no tier before a contribution"), Settings->GetCommandTier(GSTNTestTags::Special_Directional), 0);
	TestFalse(TEXT("no exclusive button before a contribution"), Settings->IsExclusiveButton(GSTNTestTags::Button_Attack.GetTag()));

	TMap<FGameplayTag, int32> Tiers;
	Tiers.Add(GSTNTestTags::Special, 300);
	Settings->SetContribution(TEXT("TestPlugin"), Tiers, { GSTNTestTags::Button_Attack.GetTag() });
	TestEqual(TEXT("a contributed root gives its leaves the tier"), Settings->GetCommandTier(GSTNTestTags::Special_Directional), 300);
	TestTrue(TEXT("a contributed exclusive button"), Settings->IsExclusiveButton(GSTNTestTags::Button_Attack.GetTag()));
	TestFalse(TEXT("only the contributed buttons"), Settings->IsExclusiveButton(GSTNTestTags::Button_Jump.GetTag()));

	// The settings' own list wins over a contribution on the same tag, and the nearest ancestor wins across both
	Settings->CommandTiers.Add(GSTNTestTags::Special, 10);
	TestEqual(TEXT("the settings' own tier wins on the same tag"), Settings->GetCommandTier(GSTNTestTags::Special_Directional), 10);

	// A second contributor adds to the first and the highest tier of a tag wins
	TMap<FGameplayTag, int32> OtherTiers;
	OtherTiers.Add(GSTNTestTags::Attack, 100);
	Settings->SetContribution(TEXT("OtherPlugin"), OtherTiers, {});
	TestEqual(TEXT("a second contributor adds its tiers"), Settings->GetCommandTier(GSTNTestTags::Attack_Neutral), 100);
	TestTrue(TEXT("the first contributor is kept"), Settings->IsExclusiveButton(GSTNTestTags::Button_Attack.GetTag()));

	Settings->SetContribution(TEXT("TestPlugin"), TMap<FGameplayTag, int32>(), {});
	TestFalse(TEXT("a contribution is replaced, not merged"), Settings->IsExclusiveButton(GSTNTestTags::Button_Attack.GetTag()));
	Settings->RemoveContribution(TEXT("OtherPlugin"));
	TestEqual(TEXT("a removed contribution is gone"), Settings->GetCommandTier(GSTNTestTags::Attack_Neutral), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

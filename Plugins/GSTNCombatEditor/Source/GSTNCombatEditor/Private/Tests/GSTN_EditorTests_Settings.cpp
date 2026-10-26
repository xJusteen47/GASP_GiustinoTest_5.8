#include "GSTN_CombatTags.h"
#include "Settings/GSTN_CombatEditorSettings.h"

#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNEditorSettingsTest
{
	/** The struct names of the M3 track kinds, as the track colour table addresses them */
	const TCHAR* const TrackStructNames[] = {
		TEXT("GSTN_TimelineTrack_CancelWindow"), TEXT("GSTN_TimelineTrack_Movement"), TEXT("GSTN_TimelineTrack_Tags"),
		TEXT("GSTN_TimelineTrack_Event"), TEXT("GSTN_TimelineTrack_WeaponSwitch"), TEXT("GSTN_TimelineTrack_Hitbox"),
		TEXT("GSTN_TimelineTrack_HurtboxModifier") };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EditorSettingsDefaultsTest, "GSTNCombat.Editor.Settings.DefaultsFromPluginIni",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_EditorSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UGSTN_CombatEditorSettings& Settings = UGSTN_CombatEditorSettings::Get();

	// Values that only the plugin's Config/DefaultGSTNCombatEditor.ini sets (the C++ defaults differ or are empty)
	TestFalse(TEXT("baking on save is off by default"), Settings.bBakeOnSave);
	TestTrue(TEXT("auto-key is on by default"), Settings.bAutoKeyDefault);
	TestEqual(TEXT("fallback tick rate"), Settings.FallbackTickRateHz, 62.5f);
	TestTrue(TEXT("debugger key"), Settings.DebuggerKey == FKey(TEXT("NumPadFive")));
	TestTrue(TEXT("mirror forward axis is +X"), Settings.MirrorForwardAxis == EGSTN_MirrorForwardAxis::PositiveX);

	// A track colour per M3 kind, distinct from the generic one, and the generic colour for anything else
	for (const TCHAR* StructName : GSTNEditorSettingsTest::TrackStructNames)
	{
		TestFalse(FString::Printf(TEXT("%s has its own colour"), StructName),
			Settings.GetTrackColor(StructName) == Settings.GenericTrackColor);
	}
	TestTrue(TEXT("an unknown track struct uses the generic colour"),
		Settings.GetTrackColor(TEXT("GSTN_TimelineTrack_NotInTheTable")) == Settings.GenericTrackColor);

	// The three bone naming conventions the plan names
	TArray<FString> Pairs;
	for (const FGSTN_MirrorNamePair& Pair : Settings.MirrorNamePairs)
	{
		Pairs.Add(Pair.Left + TEXT("/") + Pair.Right);
	}
	TestTrue(TEXT("_l/_r pair"), Pairs.Contains(TEXT("_l/_r")));
	TestTrue(TEXT("Left/Right pair"), Pairs.Contains(TEXT("Left/Right")));
	TestTrue(TEXT(".L/.R pair"), Pairs.Contains(TEXT(".L/.R")));

	// The zone mapping resolves to the six native tags of GSTNCombatGAS, so a bone name suggests a zone
	const TArray<TPair<const TCHAR*, FGameplayTag>> Expected = {
		{ TEXT("head"), GSTNCombatTags::Hurtbox_Head },
		{ TEXT("spine_03"), GSTNCombatTags::Hurtbox_Torso },
		{ TEXT("upperarm_l"), GSTNCombatTags::Hurtbox_Arm },
		{ TEXT("calf_r"), GSTNCombatTags::Hurtbox_Leg },
		{ TEXT("hand_r"), GSTNCombatTags::Hurtbox_Hand },
		{ TEXT("foot_l"), GSTNCombatTags::Hurtbox_Foot } };
	for (const TPair<const TCHAR*, FGameplayTag>& Case : Expected)
	{
		TestTrue(FString::Printf(TEXT("zone suggested for %s"), Case.Key),
			Settings.SuggestHurtboxZone(Case.Key) == Case.Value);
	}
	TestFalse(TEXT("an unmapped bone suggests nothing"), Settings.SuggestHurtboxZone(TEXT("prop_socket")).IsValid());
	for (const FGSTN_HurtboxZoneRule& Rule : Settings.HurtboxZoneRules)
	{
		TestTrue(FString::Printf(TEXT("the rule for '%s' names a registered tag"), *Rule.BoneNameContains), Rule.Zone.IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EditorTickRateTest, "GSTNCombat.Editor.Settings.TickRateFromNetworkPrediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_EditorTickRateTest::RunTest(const FString& Parameters)
{
	const float Fallback = 42.f;

	// Unusable input always gives the fallback, never zero or a crash
	TestEqual(TEXT("null object"), UGSTN_CombatEditorSettings::ReadFixedTickRate(nullptr, Fallback), Fallback);
	TestEqual(TEXT("an object without the property"),
		UGSTN_CombatEditorSettings::ReadFixedTickRate(GetDefault<UGSTN_CombatEditorSettings>(), Fallback), Fallback);

	// The real Network Prediction settings object (the fork), reached by name as the editor does: no world involved
	const UClass* SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/NetworkPrediction.NetworkPredictionSettingsObject"));
	if (!TestNotNull(TEXT("the Network Prediction settings class is loaded"), SettingsClass))
	{
		return true;
	}
	const float FromCdo = UGSTN_CombatEditorSettings::ReadFixedTickRate(SettingsClass->GetDefaultObject(), Fallback);
	TestTrue(TEXT("the CDO gives a value, not the fallback"), FromCdo != Fallback);
	TestTrue(TEXT("the rate is positive"), FromCdo > 0.f);

	// An independent route to the same number: the config files the CDO was loaded from
	FString SettingsText;
	float FromConfig = 0.f;
	if (GConfig->GetString(*SettingsClass->GetPathName(), TEXT("Settings"), SettingsText, SettingsClass->ClassConfigName.ToString())
		|| GConfig->GetString(*SettingsClass->GetPathName(), TEXT("Settings"), SettingsText, GGameIni))
	{
		if (FParse::Value(*SettingsText, TEXT("FixedTickFrameRate="), FromConfig))
		{
			TestEqual(TEXT("the CDO agrees with the config it was loaded from"), FromCdo, FromConfig);
		}
	}

	// The helper the editor uses returns the same value (62.5 Hz in this project, set in Config/DefaultNetworkPrediction.ini)
	TestEqual(TEXT("GetSimulationTickRate reads the same object"), UGSTN_CombatEditorSettings::GetSimulationTickRate(), FromCdo);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

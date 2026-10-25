#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// The editor-only BakeInfo of a timeline (E8 of the M5 plan): authoring data that must not reach a simulation, a serializer of the network
// or a cooked build. A cooked build cannot be made inside the editor, so the test reads what UHT recorded: the member exists only with
// editor data and carries the editor-only property flag, which the cooker uses to strip it.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BakeInfoNotInNetSerializeTest, "GSTNCombat.Core.TimelineData.BakeInfoNotInNetSerialize", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BakeInfoNotInNetSerializeTest::RunTest(const FString& Parameters)
{
	// No custom serializer: the timeline data is a plain struct, so there is no NetSerialize that could carry the bake info
	TestFalse(TEXT("the timeline data has no NetSerialize"), TStructOpsTypeTraits<FGSTN_CombatTimelineData>::WithNetSerializer);
	TestFalse(TEXT("the timeline data has no NetDeltaSerialize"), TStructOpsTypeTraits<FGSTN_CombatTimelineData>::WithNetDeltaSerializer);

	const FProperty* BakeInfo = FGSTN_CombatTimelineData::StaticStruct()->FindPropertyByName(TEXT("BakeInfo"));
#if WITH_EDITORONLY_DATA
	if (TestNotNull(TEXT("BakeInfo exists with editor data"), BakeInfo))
	{
		TestTrue(TEXT("BakeInfo is an editor-only property (stripped from cooked data)"), BakeInfo->IsEditorOnlyProperty());
	}
	// BakedSockets is the runtime data and stays
	const FProperty* Baked = FGSTN_CombatTimelineData::StaticStruct()->FindPropertyByName(TEXT("BakedSockets"));
	if (TestNotNull(TEXT("BakedSockets exists"), Baked))
	{
		TestFalse(TEXT("BakedSockets is not editor-only: the runtime reads it"), Baked->IsEditorOnlyProperty());
	}
#else
	TestNull(TEXT("BakeInfo does not exist without editor data"), BakeInfo);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BakeInfoDefaultsAreNotBakedTest, "GSTNCombat.Core.TimelineData.BakeInfoDefaultIsNeverBaked", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BakeInfoDefaultsAreNotBakedTest::RunTest(const FString& Parameters)
{
	FGSTN_BakeInfo Info;
	TestFalse(TEXT("a default info says never baked"), Info.IsSet());
	Info.When = FDateTime(2026, 10, 25);
	TestTrue(TEXT("a time makes it set"), Info.IsSet());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Automation tests of the combat rules asset and its activation pipeline. Pure: no world, no simulation.

#include "GSTN_RulesTestTypes.h"
#include "GSTN_CombatTestUtils.h"
#include "Misc/PackageName.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_CombatStep.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSoftClassPtr<UGSTN_CombatStep> Step(UClass* Class)
	{
		return TSoftClassPtr<UGSTN_CombatStep>(Class);
	}

	TStrongObjectPtr<UGSTN_CombatRules> MakeRules(std::initializer_list<TSoftClassPtr<UGSTN_CombatStep>> Steps)
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
		Rules->Pipeline = TArray<TSoftClassPtr<UGSTN_CombatStep>>(Steps);
		Rules->ResolvePipeline();
		return Rules;
	}

	bool LogIs(std::initializer_list<int32> Expected)
	{
		return GSTNRulesTest::RunLog() == TArray<int32>(Expected);
	}

	FGSTN_CombatStepResult Run(const UGSTN_CombatRules& Rules)
	{
		GSTNRulesTest::RunLog().Reset();
		FGSTN_CombatStepView View;
		View.Frame = 10;
		return Rules.RunPipeline(View);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RulesPipelineOrderTest, "GSTNCombat.Core.Rules.PipelineOrder", GSTN_CORE_TEST_FLAGS)

bool FGSTN_RulesPipelineOrderTest::RunTest(const FString& Parameters)
{
	// The default rules carry the one built-in step, which lets everything through
	{
		TStrongObjectPtr<UGSTN_CombatRules> Defaults(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
		TestEqual(TEXT("one built-in step by default"), Defaults->Pipeline.Num(), 1);
		Defaults->ResolvePipeline();
		TestEqual(TEXT("the built-in step resolves"), Defaults->GetResolvedPipeline().Num(), 1);
		TestTrue(TEXT("the built-in step continues"), Run(*Defaults).Verdict == EGSTN_CombatStepVerdict::Continue);
	}

	// The order is the order of the list, whatever the order of declaration of the classes
	{
		const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules({ Step(UGSTN_TestStepOne::StaticClass()), Step(UGSTN_TestStepTwo::StaticClass()), Step(UGSTN_TestStepThree::StaticClass()) });
		TestTrue(TEXT("verdict"), Run(*Rules).Verdict == EGSTN_CombatStepVerdict::Continue);
		TestTrue(TEXT("1, 2, 3"), LogIs({ 1, 2, 3 }));
	}
	{
		const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules({ Step(UGSTN_TestStepThree::StaticClass()), Step(UGSTN_TestStepOne::StaticClass()), Step(UGSTN_TestStepTwo::StaticClass()) });
		Run(*Rules);
		TestTrue(TEXT("3, 1, 2"), LogIs({ 3, 1, 2 }));
	}

	// The first rejection ends the pipeline and is its result
	{
		const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules({ Step(UGSTN_TestStepOne::StaticClass()), Step(UGSTN_TestStepReject::StaticClass()), Step(UGSTN_TestStepThree::StaticClass()) });
		TestTrue(TEXT("verdict"), Run(*Rules).Verdict == EGSTN_CombatStepVerdict::Reject);
		TestTrue(TEXT("the step after the rejection does not run"), LogIs({ 1, 9 }));
	}

	// An empty pipeline lets everything through
	{
		const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules({});
		TestTrue(TEXT("verdict"), Run(*Rules).Verdict == EGSTN_CombatStepVerdict::Continue);
		TestTrue(TEXT("nothing ran"), GSTNRulesTest::RunLog().IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RulesUnknownStepSkippedTest, "GSTNCombat.Core.Rules.UnknownStepSkipped", GSTN_CORE_TEST_FLAGS)

bool FGSTN_RulesUnknownStepSkippedTest::RunTest(const FString& Parameters)
{
	// A class of a plugin that is not loaded: the package does not exist
	const FString MissingPackage = FPackageName::GetModuleScriptPackageName(FStringView(TEXT("GSTNNoSuchModule")));
	AddExpectedError(TEXT("GSTNNoSuchModule"), EAutomationExpectedErrorFlags::Contains, -1);
	const TSoftClassPtr<UGSTN_CombatStep> Missing{ FSoftObjectPath(FTopLevelAssetPath(FName(*MissingPackage), FName(TEXT("GSTN_NoSuchStep")))) };

	const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules({
		Step(UGSTN_TestStepOne::StaticClass()),
		TSoftClassPtr<UGSTN_CombatStep>(),
		Missing,
		Step(UGSTN_CombatStep::StaticClass()),
		Step(UGSTN_TestStepTwo::StaticClass()) });

	TestEqual(TEXT("the list keeps its five entries"), Rules->Pipeline.Num(), 5);
	TestEqual(TEXT("only the two usable steps are resolved"), Rules->GetResolvedPipeline().Num(), 2);

	TestTrue(TEXT("verdict"), Run(*Rules).Verdict == EGSTN_CombatStepVerdict::Continue);
	TestTrue(TEXT("the others run in order"), LogIs({ 1, 2 }));

	// Resolving again after the list changed rebuilds it
	Rules->Pipeline.RemoveAt(0);
	Rules->ResolvePipeline();
	Run(*Rules);
	TestTrue(TEXT("rebuilt"), LogIs({ 2 }));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

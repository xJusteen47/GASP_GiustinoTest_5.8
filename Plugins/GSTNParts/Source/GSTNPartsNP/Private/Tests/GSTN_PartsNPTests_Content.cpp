// Automation tests of the content helpers the setup script uses (P7): idempotent, no simulation involved.

#include "GSTN_PartsNPTestFixture.h"
#include "Data/GSTN_PartsContentLibrary.h"
#include "Data/GSTN_PartsKitExtension.h"
#include "Hits/GSTN_HitSteps.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sim/GSTN_PartsKitLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPartsNPTest;

namespace
{
	int32 IndexOfStep(const UGSTN_CombatRules& Rules, const UClass* Class)
	{
		return Rules.HitPipeline.IndexOfByPredicate([Class](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return Step.ToSoftObjectPath() == FSoftObjectPath(Class); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsContentRulesTest, "GSTNParts.NP.Content.ConvertRulesToParts", GSTNPartsNPTest::Flags)
bool FGSTN_PartsContentRulesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	TestTrue(TEXT("the standard rules have ApplyDamage"), IndexOfStep(*Rules, UGSTN_HitStep_ApplyDamage::StaticClass()) != INDEX_NONE);
	const int32 Before = Rules->HitPipeline.Num();

	TestTrue(TEXT("converted"), UGSTN_PartsContentLibrary::ConvertRulesToParts(Rules.Get()));
	const int32 Resolve = IndexOfStep(*Rules, UGSTN_HitStep_PartResolve::StaticClass());
	TestTrue(TEXT("PartResolve is in the pipeline"), Resolve != INDEX_NONE);
	TestTrue(TEXT("right before ComputeKnockback"), Resolve + 1 == IndexOfStep(*Rules, UGSTN_HitStep_ComputeKnockback::StaticClass()));
	TestEqual(TEXT("ApplyDamage is gone"), IndexOfStep(*Rules, UGSTN_HitStep_ApplyDamage::StaticClass()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and HealthWeight"), IndexOfStep(*Rules, UGSTN_HitStep_HealthWeight::StaticClass()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("two steps out, one in"), Rules->HitPipeline.Num(), Before - 1);

	const TArray<TSoftClassPtr<UGSTN_HitResolutionStep>> Once = Rules->HitPipeline;
	UGSTN_PartsContentLibrary::ConvertRulesToParts(Rules.Get());
	TestTrue(TEXT("a second conversion changes nothing"), Rules->HitPipeline == Once);
	TestFalse(TEXT("a null rules asset is refused"), UGSTN_PartsContentLibrary::ConvertRulesToParts(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsContentEffectsTest, "GSTNParts.NP.Content.BreakEffectsAreIdempotent", GSTNPartsNPTest::Flags)
bool FGSTN_PartsContentEffectsTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	const int32 Before = Fixture.Set->Parts[Head].OnBreak.Num();
	TestTrue(TEXT("a group mask is set"), UGSTN_PartsContentLibrary::SetBreakDisableHitGroups(Fixture.Set.Get(), Head, 0x6));
	TestTrue(TEXT("again with another mask"), UGSTN_PartsContentLibrary::SetBreakDisableHitGroups(Fixture.Set.Get(), Head, 0x8));
	TestTrue(TEXT("a main damage effect"), UGSTN_PartsContentLibrary::SetBreakMainDamage(Fixture.Set.Get(), Head, 10.f, false));
	TestTrue(TEXT("replaced"), UGSTN_PartsContentLibrary::SetBreakMainDamage(Fixture.Set.Get(), Head, 25.f, true));
	TestEqual(TEXT("two effects, not four"), Fixture.Set->Parts[Head].OnBreak.Num(), Before + 2);
	const FGSTN_PartBreakEffect_DisableHitGroups* Groups = nullptr;
	const FGSTN_PartBreakEffect_MainDamage* Damage = nullptr;
	for (const FInstancedStruct& Entry : Fixture.Set->Parts[Head].OnBreak)
	{
		Groups = Groups ? Groups : Entry.GetPtr<FGSTN_PartBreakEffect_DisableHitGroups>();
		Damage = Damage ? Damage : Entry.GetPtr<FGSTN_PartBreakEffect_MainDamage>();
	}
	TestEqual(TEXT("the last mask wins"), Groups ? Groups->GroupMask : 0, static_cast<uint8>(0x8));
	TestTrue(TEXT("the last damage wins"), Damage && Damage->AddPercent == 25.f && Damage->bKill);
	TestFalse(TEXT("a bad index is refused"), UGSTN_PartsContentLibrary::SetBreakMainDamage(Fixture.Set.Get(), 99, 1.f, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsContentKitTest, "GSTNParts.NP.Content.KitPartSet", GSTNPartsNPTest::Flags)
bool FGSTN_PartsContentKitTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
	TestTrue(TEXT("the set is added"), UGSTN_PartsKitLibrary::SetKitPartSet(Kit.Get(), Fixture.Set.Get()));
	TestTrue(TEXT("twice"), UGSTN_PartsKitLibrary::SetKitPartSet(Kit.Get(), Fixture.Set.Get()));
	TestEqual(TEXT("one extension entry"), Kit->Extensions.ExtensionData.Num(), 1);
	TestTrue(TEXT("the kit names the set"), FGSTN_PartsAttach::ResolveKitSet(Kit->Extensions) == Fixture.Set.Get());
	TestTrue(TEXT("null removes it"), UGSTN_PartsKitLibrary::SetKitPartSet(Kit.Get(), nullptr));
	TestEqual(TEXT("no entry"), Kit->Extensions.ExtensionData.Num(), 0);
	TestFalse(TEXT("a null kit is refused"), UGSTN_PartsKitLibrary::SetKitPartSet(nullptr, Fixture.Set.Get()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

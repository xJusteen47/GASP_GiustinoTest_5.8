// Automation test of the defence plugin's own test content (plan F13): the assets Tools/GAS/setup_defence_link_content.py writes under
// /GSTNDefence/Test load in the editor, validate, and carry the numbers the harness and the docs quote (m6 spec 11).

#include "Abilities/GSTN_DefenceAbilities.h"
#include "Abilities/GSTN_GA_Shield.h"
#include "Data/GSTN_DefenceRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace GSTNDefenceContentTest
{
	/** The plugin's content root: plugin-owned, so the test needs no project content */
	const TCHAR* const Root = TEXT("/GSTNDefence/Test/");

	/** The ability Blueprints of the sample content and the native class each one derives from */
	struct FAbilitySpec
	{
		const TCHAR* Name;
		UClass* Parent;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceContentSampleAssetsValidTest, "GSTNDefence.Content.SampleAssetsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_DefenceContentSampleAssetsValidTest::RunTest(const FString& Parameters)
{
	using namespace GSTNDefenceContentTest;

	// The two rules assets: PvP with the short test numbers, PvE with the shield off and the rest equal
	struct FRulesSpec
	{
		const TCHAR* Name;
		bool bShieldEnabled;
	};
	for (const FRulesSpec& Spec : { FRulesSpec{ TEXT("DA_DefenceRules_Test"), true }, FRulesSpec{ TEXT("DA_DefenceRules_TestPvE"), false } })
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s"), Root, Spec.Name, Spec.Name);
		const UGSTN_DefenceRules* Rules = LoadObject<UGSTN_DefenceRules>(nullptr, *Path);
		if (!TestNotNull(FString::Printf(TEXT("%s loads"), Spec.Name), Rules))
		{
			continue;
		}
		FDataValidationContext Context;
		TestEqual(FString::Printf(TEXT("%s validates"), Spec.Name), static_cast<int32>(Rules->IsDataValid(Context)),
			static_cast<int32>(EDataValidationResult::Valid));
		TestTrue(FString::Printf(TEXT("%s shield flag"), Spec.Name), Rules->bShieldEnabled == Spec.bShieldEnabled);
		TestEqual(FString::Printf(TEXT("%s shield 100"), Spec.Name), Rules->Shield.MaxShield, 100.f);
		TestEqual(FString::Printf(TEXT("%s drain 0.4 a frame"), Spec.Name), Rules->Shield.DrainPerFrame, 0.4f);
		TestEqual(FString::Printf(TEXT("%s perfect shield window 5"), Spec.Name), Rules->Parry.WindowFrames, 5);
		TestTrue(FString::Printf(TEXT("%s perfect shield on the release"), Spec.Name), Rules->Parry.Anchor == EGSTN_ParryAnchor::Release);
		TestEqual(FString::Printf(TEXT("%s tech window 20"), Spec.Name), Rules->Tech.WindowFrames, 20);
		TestEqual(FString::Printf(TEXT("%s tech lockout 40"), Spec.Name), Rules->Tech.LockoutFrames, 40);
		TestEqual(FString::Printf(TEXT("%s parry profiles Shield and Counter"), Spec.Name), Rules->ParryProfiles.Num(), 2);
		if (Rules->ParryProfiles.Num() == 2)
		{
			TestTrue(TEXT("profile 1 is the counter's"), Rules->ParryProfiles[1].Name == FName(TEXT("Counter")));
		}
	}

	// The abilities: thin Blueprint children of the plugin's native classes (the frame data is the native class's), so a kit names content
	const FAbilitySpec Abilities[] = {
		{ TEXT("GA_Shield_Test"), UGSTN_GA_Shield::StaticClass() },
		{ TEXT("GA_CounterTest"), UGSTN_GA_CounterSample::StaticClass() },
		{ TEXT("GA_TechInPlace_Test"), UGSTN_GA_Tech::StaticClass() },
		{ TEXT("GA_TechRoll_Test"), UGSTN_GA_Tech::StaticClass() },
		{ TEXT("GA_TechWall_Test"), UGSTN_GA_Tech::StaticClass() },
		{ TEXT("GA_GroundBounce_Test"), UGSTN_GA_GroundBounce::StaticClass() },
		{ TEXT("GA_GetupRoll_Test"), UGSTN_GA_GetupRoll::StaticClass() },
		{ TEXT("GA_GetupNeutral_Test"), UGSTN_GA_GetupNeutral::StaticClass() },
	};
	for (const FAbilitySpec& Spec : Abilities)
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s_C"), Root, Spec.Name, Spec.Name);
		UClass* Class = LoadClass<UObject>(nullptr, *Path);
		if (!TestNotNull(FString::Printf(TEXT("%s loads"), Spec.Name), Class))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s derives from %s"), Spec.Name, *Spec.Parent->GetName()), Class->IsChildOf(Spec.Parent));
		FDataValidationContext Context;
		TestNotEqual(FString::Printf(TEXT("%s is not invalid"), Spec.Name), static_cast<int32>(Class->GetDefaultObject()->IsDataValid(Context)),
			static_cast<int32>(EDataValidationResult::Invalid));
	}
	return true;
}

#endif

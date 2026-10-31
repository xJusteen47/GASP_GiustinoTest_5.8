// Automation test of the link plugin's own test content (plan F13): the assets Tools/GAS/setup_defence_link_content.py writes under
// /GSTNLink/Test load in the editor, validate, are found by the registry, and carry the values the harness and the docs quote (m6 spec 11).

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkTags.h"
#include "Harness/GSTN_LinkTestAbilities.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Rules/GSTN_LinkRule_Latch.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace GSTNLinkContentTest
{
	/** The plugin's content root: plugin-owned, so the test needs no project content */
	const TCHAR* const Root = TEXT("/GSTNLink/Test/");

	/** The grab's mash in the test content: shorter than the rule's default so the harness waits less (m6 spec 11, "grab base 90 frames") */
	constexpr int32 MashBaseFrames = 90;

	const UGSTN_LinkTypeDefinition* LoadType(FAutomationTestBase& Test, const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s"), Root, Name, Name);
		const UGSTN_LinkTypeDefinition* Definition = LoadObject<UGSTN_LinkTypeDefinition>(nullptr, *Path);
		if (Test.TestNotNull(FString::Printf(TEXT("%s loads"), Name), Definition))
		{
			FDataValidationContext Context;
			Test.TestEqual(FString::Printf(TEXT("%s validates"), Name), static_cast<int32>(Definition->IsDataValid(Context)),
				static_cast<int32>(EDataValidationResult::Valid));
		}
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkContentSampleAssetsValidTest, "GSTNLink.Content.SampleAssetsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_LinkContentSampleAssetsValidTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLinkContentTest;

	// The grab: its tag, the grabbed parked, the latch tried first, the grab rule with the test mash
	if (const UGSTN_LinkTypeDefinition* Grab = LoadType(*this, TEXT("DA_LinkType_Grab_Test")))
	{
		TestTrue(TEXT("The grab type's tag"), Grab->LinkType == GSTNLinkTags::Type_Grab.GetTag());
		TestEqual(TEXT("The grab parks the grabbed (role 2)"), Grab->GetParkedRoleIndex(), 2);
		TestTrue(TEXT("The grab lists the latch as its alternative"), Grab->Alternatives.Contains(GSTNLinkTags::Type_Latch.GetTag()));
		const UGSTN_LinkRule_Grab* Rule = nullptr;
		for (const TObjectPtr<UGSTN_LinkRule>& Entry : Grab->Rules)
		{
			Rule = Rule ? Rule : Cast<UGSTN_LinkRule_Grab>(Entry.Get());
		}
		if (TestNotNull(TEXT("The grab type carries a grab rule"), Rule))
		{
			TestEqual(TEXT("The grab rule's mash base frames"), Rule->Rules.Mash.BaseFrames, MashBaseFrames);
		}
	}

	// The latch: the latcher parked and acting, the latched one acting with its release as the shake-off
	if (const UGSTN_LinkTypeDefinition* Latch = LoadType(*this, TEXT("DA_LinkType_Latch_Test")))
	{
		TestTrue(TEXT("The latch type's tag"), Latch->LinkType == GSTNLinkTags::Type_Latch.GetTag());
		TestEqual(TEXT("The latch parks the latcher (role 1)"), Latch->GetParkedRoleIndex(), 1);
		TestTrue(TEXT("The latched role keeps acting"), Latch->GetRoleDef(2).bActsWhileLinked);
		TestTrue(TEXT("The latch type carries a latch rule"),
			Latch->Rules.ContainsByPredicate([](const TObjectPtr<UGSTN_LinkRule>& Entry) { return Entry && Entry->IsA<UGSTN_LinkRule_Latch>(); }));
	}

	// The registry finds both by their primary asset type, as the game does
	if (const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get())
	{
		TestNotNull(TEXT("The registry knows the grab"), Registry->FindDefinition(GSTNLinkTags::Type_Grab.GetTag()));
		TestNotNull(TEXT("The registry knows the latch"), Registry->FindDefinition(GSTNLinkTags::Type_Latch.GetTag()));
	}
	else
	{
		AddError(TEXT("No link registry"));
	}

	// The moves: thin Blueprint children of the harness classes (the frame data is the native class's), one asset per command
	struct FAbilitySpec
	{
		const TCHAR* Name;
		UClass* Parent;
	};
	const FAbilitySpec Abilities[] = {
		{ TEXT("GA_Grab_Test"), UGSTN_GA_LinkTestGrab::StaticClass() },
		{ TEXT("GA_Pummel_Test"), UGSTN_GA_LinkTestPummel::StaticClass() },
		{ TEXT("GA_Release_Test"), UGSTN_GA_LinkTestRelease::StaticClass() },
		{ TEXT("GA_ThrowForward_Test"), UGSTN_GA_LinkTestThrow::StaticClass() },
		{ TEXT("GA_ThrowBack_Test"), UGSTN_GA_LinkTestThrow::StaticClass() },
		{ TEXT("GA_ThrowUp_Test"), UGSTN_GA_LinkTestThrow::StaticClass() },
		{ TEXT("GA_ThrowDown_Test"), UGSTN_GA_LinkTestThrow::StaticClass() },
		{ TEXT("GA_LatchAttack_Test"), UGSTN_GA_LinkTestLatchAttack::StaticClass() },
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

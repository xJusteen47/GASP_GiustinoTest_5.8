// Piggyback, editor part (editor only; the runtime never depends on it).

#include "GSTN_PairMoveAbilityValidator.h"
#include "GSTN_PairMoveDefinition.h"
#include "GSTN_PairMoveFactories.h"
#include "GSTN_PairMoveTestContent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool AnyContains(const TArray<FString>& Messages, const TCHAR* Text)
	{
		return Messages.ContainsByPredicate([Text](const FString& Message) { return Message.Contains(Text); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackEditor_FreePairMoveId, "GSTNPiggyback.Editor.FactoryPicksFreeId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGSTN_PiggybackEditor_FreePairMoveId::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("no definitions: 1"), UGSTN_PairMoveDefinitionFactory::FindFreePairMoveId({}), 1);
	TestEqual(TEXT("the first gap"), UGSTN_PairMoveDefinitionFactory::FindFreePairMoveId({ 1, 2, 4 }), 3);
	TestEqual(TEXT("after a run"), UGSTN_PairMoveDefinitionFactory::FindFreePairMoveId({ 1, 2, 3 }), 4);
	TestEqual(TEXT("a hole at 1"), UGSTN_PairMoveDefinitionFactory::FindFreePairMoveId({ 2, 3 }), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackEditor_AbilityValidator, "GSTNPiggyback.Editor.PairMoveAbilityValidator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGSTN_PiggybackEditor_AbilityValidator::RunTest(const FString& Parameters)
{
	const UClass* Ability = UGSTN_GA_Test_CarrierThrowRider::StaticClass();

	TArray<FString> Errors;
	TArray<FString> Warnings;
	UGSTN_PairMoveAbilityValidator::CheckPairMoveAbility(Ability, nullptr, Errors, Warnings);
	TestEqual(TEXT("no definition: no error"), Errors.Num(), 0);
	TestEqual(TEXT("no definition: one warning"), Warnings.Num(), 1);

	UGSTN_PairMoveDefinition* Other = NewObject<UGSTN_PairMoveDefinition>(GetTransientPackage());
	Other->InitiatorAbility = UGSTN_GA_Test_RiderBoostJump::StaticClass();
	Errors.Reset();
	Warnings.Reset();
	UGSTN_PairMoveAbilityValidator::CheckPairMoveAbility(Ability, Other, Errors, Warnings);
	TestTrue(TEXT("a definition that plays another ability is an error"), AnyContains(Errors, TEXT("plays that ability")));

	UGSTN_PairMoveDefinition* Own = NewObject<UGSTN_PairMoveDefinition>(GetTransientPackage());
	Own->InitiatorAbility = UGSTN_GA_Test_CarrierThrowRider::StaticClass();
	Errors.Reset();
	Warnings.Reset();
	UGSTN_PairMoveAbilityValidator::CheckPairMoveAbility(Ability, Own, Errors, Warnings);
	TestFalse(TEXT("its own definition: no mismatch"), AnyContains(Errors, TEXT("plays that ability")));
	TestTrue(TEXT("the definition's own findings come along, prefixed"), Errors.Num() == 0 || AnyContains(Errors, TEXT("Pair move ")));

	// A child of the initiator (per-kit timing) is accepted
	Own->InitiatorAbility = UGSTN_GA_PairMove::StaticClass();
	Errors.Reset();
	UGSTN_PairMoveAbilityValidator::CheckPairMoveAbility(Ability, Own, Errors, Warnings);
	TestFalse(TEXT("a child of the initiator: no mismatch"), AnyContains(Errors, TEXT("plays that ability")));
	return true;
}

#endif

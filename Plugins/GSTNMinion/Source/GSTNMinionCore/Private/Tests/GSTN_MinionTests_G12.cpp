// Automation tests of M8 G12a in the Core: the store capacity default and the cross-asset validation rules. Pure: no world.

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_MinionValidation.h"
#include "Data/GSTN_MinionWaveDefinition.h"
#include "GameplayTagsManager.h"
#include "GSTN_MinionTestUtils.h"
#include "HAL/IConsoleManager.h"
#include "Store/GSTN_MinionStore.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionCapacity160Test, "GSTNMinion.Core.G12.CapacityDefault160", GSTNMinionTest::Flags)
bool FGSTN_MinionCapacity160Test::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("The default capacity is 160"), GSTNMinion::DefaultCapacity, 160);
	TestTrue(TEXT("A single wave still fits the default store with room for others"), GSTNMinion::MaxWaveTotal < GSTNMinion::DefaultCapacity);

	// The variable carries the default unless something set it (an ini, another test): only then is it comparable
	IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Minion.Max"));
	if (TestNotNull(TEXT("gstn.Minion.Max exists"), Var) && (Var->GetFlags() & ECVF_SetByMask) == ECVF_SetByConstructor)
	{
		TestEqual(TEXT("The variable defaults to the constant"), Var->GetInt(), GSTNMinion::DefaultCapacity);
	}

	// The G11a population case: 150 minions fit
	FGSTN_MinionStore Store;
	Store.Reset(GSTNMinion::DefaultCapacity);
	int32 Allocated = 0;
	for (int32 Index = 0; Index < 150; ++Index)
	{
		Allocated += Store.Allocate().IsNull() ? 0 : 1;
	}
	TestEqual(TEXT("150 minions are allocated"), Allocated, 150);
	return true;
}

namespace
{
	UGSTN_MinionDefinition* MakeType(const TCHAR* Name, const FGameplayTag& Tag)
	{
		UGSTN_MinionDefinition* Definition = NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), FName(Name), RF_Transient);
		Definition->TypeTag = Tag;
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionRegistryCheckTest, "GSTNMinion.Core.G12.RegistryCheck", GSTNMinionTest::Flags)
bool FGSTN_MinionRegistryCheckTest::RunTest(const FString& Parameters)
{
	// Tags need not be registered for the rule: it compares them, so two distinct requested-or-empty tags are enough. Use the base tag
	// requested without error so no tag has to exist in the ini
	const FGameplayTag TagA = FGameplayTag::RequestGameplayTag(FName(TEXT("Minion.Type")), false);
	UGSTN_MinionDefinition* A = MakeType(TEXT("G12TypeA"), TagA);
	UGSTN_MinionDefinition* B = MakeType(TEXT("G12TypeB"), FGameplayTag());
	UGSTN_MinionDefinition* C = MakeType(TEXT("G12TypeC"), FGameplayTag());

	TArray<FString> Errors;
	GSTNMinionValidation::CheckRegistry(TArray<const UGSTN_MinionDefinition*>{ A, B, C }, Errors);
	TestEqual(TEXT("Distinct definitions are fine"), Errors.Num(), 0);

	Errors.Reset();
	GSTNMinionValidation::CheckRegistry(TArray<const UGSTN_MinionDefinition*>{ A, B, A }, Errors);
	TestEqual(TEXT("A duplicate entry is one error"), Errors.Num(), 1);

	Errors.Reset();
	GSTNMinionValidation::CheckRegistry(TArray<const UGSTN_MinionDefinition*>{ A, nullptr, B }, Errors);
	TestEqual(TEXT("A null entry is one error"), Errors.Num(), 1);

	if (TagA.IsValid())
	{
		UGSTN_MinionDefinition* SameTag = MakeType(TEXT("G12TypeSameTag"), TagA);
		Errors.Reset();
		GSTNMinionValidation::CheckRegistry(TArray<const UGSTN_MinionDefinition*>{ A, SameTag }, Errors);
		TestEqual(TEXT("Two assets with one type tag are one error"), Errors.Num(), 1);
	}

	TArray<const UGSTN_MinionDefinition*> TooMany;
	for (int32 Index = 0; Index <= GSTNMinionValidation::MaxRegistryTypes; ++Index)
	{
		TooMany.Add(MakeType(*FString::Printf(TEXT("G12Many%d"), Index), FGameplayTag()));
	}
	Errors.Reset();
	GSTNMinionValidation::CheckRegistry(TooMany, Errors);
	TestEqual(TEXT("More than 256 types is one error"), Errors.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionWaveCapacityCheckTest, "GSTNMinion.Core.G12.WaveOverCapacity", GSTNMinionTest::Flags)
bool FGSTN_MinionWaveCapacityCheckTest::RunTest(const FString& Parameters)
{
	UGSTN_MinionDefinition* Definition = MakeType(TEXT("G12WaveType"), FGameplayTag());
	UGSTN_MinionWaveDefinition* Wave = NewObject<UGSTN_MinionWaveDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FGSTN_MinionWaveEntry& Entry = Wave->Entries.AddDefaulted_GetRef();
		Entry.Definition = Definition;
		Entry.Count = 20;
	}

	TArray<FString> Errors;
	GSTNMinionValidation::CheckWaveCrossAsset(*Wave, 160, Errors);
	TestEqual(TEXT("60 in a store of 160 is fine, and three entries of one type are not a duplicate"), Errors.Num(), 0);

	Errors.Reset();
	GSTNMinionValidation::CheckWaveCrossAsset(*Wave, 40, Errors);
	TestEqual(TEXT("60 in a store of 40 is one error"), Errors.Num(), 1);

	// A path that does not resolve
	AddExpectedError(TEXT("G12Missing"), EAutomationExpectedErrorFlags::Contains, 0);
	FGSTN_MinionWaveEntry& Missing = Wave->Entries.AddDefaulted_GetRef();
	Missing.Definition = TSoftObjectPtr<UGSTN_MinionDefinition>(FSoftObjectPath(TEXT("/Game/DoesNotExist/G12Missing.G12Missing")));
	Errors.Reset();
	GSTNMinionValidation::CheckWaveCrossAsset(*Wave, 160, Errors);
	TestEqual(TEXT("An unresolved definition path is one error"), Errors.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

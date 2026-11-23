// Automation tests of the wave definition (M8 G9a): validation and the preload list. Pure: no world.

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_MinionWaveDefinition.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "GSTN_MinionTestUtils.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UGSTN_MinionDefinition* MakeDefinition()
	{
		return NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	UGSTN_MinionWaveDefinition* MakeWave(UGSTN_MinionDefinition* Definition, int32 Count)
	{
		UGSTN_MinionWaveDefinition* Wave = NewObject<UGSTN_MinionWaveDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		FGSTN_MinionWaveEntry& Entry = Wave->Entries.AddDefaulted_GetRef();
		Entry.Definition = Definition;
		Entry.Count = Count;
		return Wave;
	}
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionWaveValidTest, "GSTNMinion.Core.Wave.Validation", GSTNMinionTest::Flags)
bool FGSTN_MinionWaveValidTest::RunTest(const FString& Parameters)
{
	UGSTN_MinionDefinition* Definition = MakeDefinition();

	const auto Validate = [](const UGSTN_MinionWaveDefinition* Wave, int32& OutErrors)
	{
		FDataValidationContext Context;
		const EDataValidationResult Result = Wave->IsDataValid(Context);
		OutErrors = Context.GetNumErrors();
		return Result;
	};

	int32 Errors = 0;
	{
		UGSTN_MinionWaveDefinition* Wave = MakeWave(Definition, 8);
		TestTrue(TEXT("A good wave is valid"), Validate(Wave, Errors) == EDataValidationResult::Valid);
		TestEqual(TEXT("with no error"), Errors, 0);
		TestEqual(TEXT("and its total is the count"), Wave->GetTotalCount(), 8);
	}
	{
		UGSTN_MinionWaveDefinition* Wave = MakeWave(Definition, 8);
		Wave->Entries[0].Definition.Reset();
		TestTrue(TEXT("An entry with no definition is invalid"), Validate(Wave, Errors) == EDataValidationResult::Invalid);
		TestTrue(TEXT("with an error"), Errors > 0);
	}
	{
		UGSTN_MinionWaveDefinition* Wave = MakeWave(Definition, 0);
		TestTrue(TEXT("A zero count is invalid"), Validate(Wave, Errors) == EDataValidationResult::Invalid);
	}
	{
		UGSTN_MinionWaveDefinition* Wave = MakeWave(Definition, GSTNMinion::MaxWaveEntryCount + 1);
		TestTrue(TEXT("A count above 64 is invalid"), Validate(Wave, Errors) == EDataValidationResult::Invalid);
	}
	{
		UGSTN_MinionWaveDefinition* Wave = MakeWave(Definition, 64);
		FGSTN_MinionWaveEntry Second = Wave->Entries[0];
		Wave->Entries.Add(Second);
		TestTrue(TEXT("Exactly 128 in total is valid"), Validate(Wave, Errors) == EDataValidationResult::Valid);
		Wave->Entries.Add(Second);
		TestEqual(TEXT("192 counted"), Wave->GetTotalCount(), 192);
		TestTrue(TEXT("A total above 128 is invalid"), Validate(Wave, Errors) == EDataValidationResult::Invalid);
	}
	{
		UGSTN_MinionWaveDefinition* Wave = NewObject<UGSTN_MinionWaveDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		TestTrue(TEXT("An empty wave is invalid"), Validate(Wave, Errors) == EDataValidationResult::Invalid);
	}
	{
		UGSTN_MinionWaveDefinition* Wave = MakeWave(Definition, 2);
		Wave->Entries[0].DelayFrames = -1;
		TestTrue(TEXT("A negative delay is invalid"), Validate(Wave, Errors) == EDataValidationResult::Invalid);
	}
	return true;
}

#endif // WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionWavePreloadTest, "GSTNMinion.Core.Wave.PreloadList", GSTNMinionTest::Flags)
bool FGSTN_MinionWavePreloadTest::RunTest(const FString& Parameters)
{
	UGSTN_MinionDefinition* Definition = MakeDefinition();
	const FSoftObjectPath BrainPath(TEXT("/Game/GSTNWaveTest/ST_FakeBrain.ST_FakeBrain"));
	const FSoftObjectPath FighterPath(TEXT("/Game/GSTNWaveTest/BP_FakeFighter.BP_FakeFighter_C"));
	const FSoftObjectPath PuppetPath(TEXT("/Game/GSTNWaveTest/BP_FakePuppet.BP_FakePuppet_C"));
	const FSoftObjectPath MeshPath(TEXT("/Game/GSTNWaveTest/SK_Fake.SK_Fake"));
	Definition->Brain = TSoftObjectPtr<UObject>(BrainPath);
	Definition->PromotionFighterClass = TSoftClassPtr<AActor>(FighterPath);
	Definition->PuppetClass = TSoftClassPtr<AActor>(PuppetPath);
	Definition->PuppetMesh = TSoftObjectPtr<USkeletalMesh>(MeshPath);

	UGSTN_MinionWaveDefinition* Wave = MakeWave(Definition, 3);
	// A second entry of the same type must not list it twice
	{ const FGSTN_MinionWaveEntry Copy = Wave->Entries[0]; Wave->Entries.Add(Copy); }

	TArray<FSoftObjectPath> Paths;
	Wave->GetDefinitionPaths(Paths);
	TestEqual(TEXT("One definition path for two entries of the same type"), Paths.Num(), 1);
	TestTrue(TEXT("and it is the definition"), Paths.Contains(FSoftObjectPath(Definition)));

	Paths.Reset();
	Wave->GetAssetsToPreload(Paths);
	TestTrue(TEXT("The definition is listed"), Paths.Contains(FSoftObjectPath(Definition)));
	TestTrue(TEXT("and its Brain tree"), Paths.Contains(BrainPath));
	TestTrue(TEXT("and its promotion fighter class"), Paths.Contains(FighterPath));
	TestTrue(TEXT("and its puppet class"), Paths.Contains(PuppetPath));
	TestTrue(TEXT("and its puppet mesh"), Paths.Contains(MeshPath));
	TestEqual(TEXT("with nothing else (empty references are skipped, no path twice)"), Paths.Num(), 5);

	Paths.Reset();
	Wave->GetAssetsToPreload(Paths, true);
	TestFalse(TEXT("A client list leaves out the Brain tree"), Paths.Contains(BrainPath));
	TestTrue(TEXT("but keeps the puppet"), Paths.Contains(PuppetPath));
	TestEqual(TEXT("four paths"), Paths.Num(), 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilityToolset.h"

#include "Abilities/GameplayAbility.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilityBlueprint.h"
#include "Misc/AutomationTest.h"
#include "ToolsetRegistry/Tests/TestAssetSandbox.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGameplayAbilityToolsetSpec,
	"AIAssistant.Toolsets.GASToolsets.GameplayAbilityToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TUniquePtr<FTestAssetSandbox> Sandbox;

	// Captures the error raised by a tool call, or empty if none was raised.
	FString CaptureError(TFunction<void()> Body)
	{
		UE::ToolsetRegistry::FToolCallExceptionHandler ExceptionHandler;
		ExceptionHandler.CaptureErrorsIn(MoveTemp(Body));
		return ExceptionHandler.GetException();
	}
END_DEFINE_SPEC(FGameplayAbilityToolsetSpec)

void FGameplayAbilityToolsetSpec::Define()
{
	// Shared by every test, so each test creates its abilities under names no other test uses.
	BeforeAll([this]()
	{
		Sandbox = MakeUnique<FTestAssetSandbox>(TEXT("GameplayAbilityToolsetSpecTemp"));
	});

	AfterAll([this]()
	{
		Sandbox.Reset();
	});

	Describe(TEXT("CreateGameplayAbility"), [this]()
	{
		It(TEXT("creates a Gameplay Ability Blueprint whose only ubergraph is the Gameplay Ability Graph"), [this]()
		{
			UBlueprint* Blueprint = UGameplayAbilityToolset::CreateGameplayAbility(
				Sandbox->Folder(), TEXT("GA_Created"), UGameplayAbility::StaticClass());
			TestNotNull(TEXT("Created"), Blueprint);
			if (!Blueprint)
			{
				return;
			}
			TestTrue(TEXT("Is a UGameplayAbilityBlueprint"), Blueprint->IsA<UGameplayAbilityBlueprint>());
			TestEqual(TEXT("Parent class"), Blueprint->ParentClass.Get(), UGameplayAbility::StaticClass());

			if (TestEqual(TEXT("Ubergraph count"), Blueprint->UbergraphPages.Num(), 1))
			{
				TestEqual(TEXT("Ubergraph name"), Blueprint->UbergraphPages[0]->GetName(), FString(TEXT("Gameplay Ability Graph")));
			}
		});

		It(TEXT("accepts a Blueprint ability as the parent"), [this]()
		{
			UBlueprint* Parent = UGameplayAbilityToolset::CreateGameplayAbility(
				Sandbox->Folder(), TEXT("GA_Parent"), UGameplayAbility::StaticClass());
			TestNotNull(TEXT("Parent created"), Parent);
			if (!Parent)
			{
				return;
			}
			UClass* ParentClass = Parent->GeneratedClass;
			TestNotNull(TEXT("Parent class"), ParentClass);
			if (!ParentClass)
			{
				return;
			}
			UBlueprint* Child = UGameplayAbilityToolset::CreateGameplayAbility(
				Sandbox->Folder(), TEXT("GA_Child"), ParentClass);
			TestNotNull(TEXT("Child created"), Child);
			if (Child)
			{
				TestEqual(TEXT("Parent class"), Child->ParentClass.Get(), ParentClass);
			}
		});

		It(TEXT("raises for an empty asset name"), [this]()
		{
			const FString Error = CaptureError([&]()
			{
				UGameplayAbilityToolset::CreateGameplayAbility(Sandbox->Folder(), FString(), UGameplayAbility::StaticClass());
			});
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("AssetName")));
		});

		It(TEXT("raises for a null parent class"), [this]()
		{
			const FString Error = CaptureError([&]()
			{
				UGameplayAbilityToolset::CreateGameplayAbility(Sandbox->Folder(), TEXT("GA_NullParent"), nullptr);
			});
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("ParentClass")));
		});

		It(TEXT("raises for a parent class that is not a gameplay ability"), [this]()
		{
			const FString Error = CaptureError([&]()
			{
				UGameplayAbilityToolset::CreateGameplayAbility(Sandbox->Folder(), TEXT("GA_BadParent"), AActor::StaticClass());
			});
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("ParentClass")));
		});

		It(TEXT("raises for a folder outside the project's asset roots"), [this]()
		{
			const FString Error = CaptureError([&]()
			{
				UGameplayAbilityToolset::CreateGameplayAbility(
					TEXT("/Engine/Transient"), TEXT("GA_BadFolder"), UGameplayAbility::StaticClass());
			});
			TestFalse(TEXT("Raises"), Error.IsEmpty());
		});

		It(TEXT("raises when the asset already exists"), [this]()
		{
			TestNotNull(TEXT("Created"), UGameplayAbilityToolset::CreateGameplayAbility(
				Sandbox->Folder(), TEXT("GA_Duplicate"), UGameplayAbility::StaticClass()));

			const FString Error = CaptureError([&]()
			{
				UGameplayAbilityToolset::CreateGameplayAbility(Sandbox->Folder(), TEXT("GA_Duplicate"), UGameplayAbility::StaticClass());
			});
			TestTrue(TEXT("Duplicate raises"), Error.Contains(TEXT("already exists")));
		});
	});
}

#endif  // WITH_DEV_AUTOMATION_TESTS

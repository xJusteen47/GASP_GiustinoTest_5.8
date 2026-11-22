// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCueToolset.h"

#include "AbilitySystemGlobals.h"
#include "Algo/IsSorted.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayCueManager.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueSet.h"
#include "GameplayTagsManager.h"
#include "Misc/AutomationTest.h"
#include "Selection.h"
#include "ToolsetRegistry/Tests/TestAssetSandbox.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGameplayCueToolsetSpec,
	"AIAssistant.Toolsets.GASToolsets.GameplayCueToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	// Registered in memory only, so no tag INI is touched. They stay registered for the editor session.
	const FString TestRoot = TEXT("GameplayCue.GASToolsetsTest");
	const FString AlphaTag = TEXT("GameplayCue.GASToolsetsTest.Alpha");
	const FString BetaTag = TEXT("GameplayCue.GASToolsetsTest.Beta");
	const FString BetaChildTag = TEXT("GameplayCue.GASToolsetsTest.Beta.Child");
	const FString MissingTag = TEXT("GameplayCue.GASToolsetsTest.NotRegistered");

	TUniquePtr<FTestAssetSandbox> Sandbox;
	FString SandboxCuePath;
	UWorld* TestWorld = nullptr;
	AActor* TestActor = nullptr;
	TArray<TWeakObjectPtr<AActor>> SavedSelection;

	// Captures the error raised by a tool call, or empty if none was raised.
	FString CaptureError(TFunction<void()> Body)
	{
		UE::ToolsetRegistry::FToolCallExceptionHandler ExceptionHandler;
		ExceptionHandler.CaptureErrorsIn(MoveTemp(Body));
		return ExceptionHandler.GetException();
	}

	// Registers a notify for Tag directly in the editor cue set. ClassPath is the notify's generated class.
	void AddCue(const FString& Tag, const FString& ClassPath)
	{
		// The cue manager only builds the editor cue set when the editor is initialized
		UGameplayCueSet* EditorCueSet = UAbilitySystemGlobals::Get().GetGameplayCueManager()->GetEditorCueSet();
		TestNotNull(TEXT("Editor cue set"), EditorCueSet);
		if (EditorCueSet)
		{
			const FGameplayTag CueTag = FGameplayTag::RequestGameplayTag(FName(*Tag));
			EditorCueSet->AddCues({ FGameplayCueReferencePair(CueTag, FSoftObjectPath(ClassPath)) });
		}
	}

	// Notifies the tool created are registered in the runtime and editor cue sets, and deleting their
	// assets does not reliably unregister them, so clear every test tag from both.
	void ClearTestCues()
	{
		FGameplayTagContainer TestCueTags;
		for (const FString& Tag : { AlphaTag, BetaTag, BetaChildTag })
		{
			TestCueTags.AddTag(FGameplayTag::RequestGameplayTag(FName(*Tag)));
		}
		for (UGameplayCueSet* CueSet : UAbilitySystemGlobals::Get().GetGameplayCueManager()->GetGlobalCueSets())
		{
			CueSet->RemoveCuesByTags(TestCueTags);
		}
	}

	// Whether the cue manager treats Path as a gameplay cue notify path. Its valid paths can be overridden
	// by a project's manager, so adding the sandbox to the global paths doesn't always make it valid.
	bool IsCuePath(const FString& Path)
	{
		// Rejecting a folder logs warnings: the folder, then the valid paths one per line (indented).
		// Both this check and the tool's own check do that, and neither is a test failure.
		AddExpectedMessage(TEXT("Invalid GameplayCuePath|Valid Paths|\\s\\s/"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, -1);

		TGuardValue<bool> NoDialogGuard(GIsRunningUnattendedScript, true);
		return UAbilitySystemGlobals::Get().GetGameplayCueManager()->VerifyNotifyAssetIsInValidPath(Path);
	}

	// For tests that create notifies in the sandbox: notes the skip and returns false if it isn't a cue path.
	bool RequireSandboxCuePath()
	{
		const bool bIsCuePath = IsCuePath(SandboxCuePath / TEXT("GCN_Probe"));
		if (!bIsCuePath)
		{
			AddInfo(TEXT("Skipped: the cue manager does not treat the test folder as a gameplay cue notify path."));
		}
		return bIsCuePath;
	}

	// Spawns an actor into a throwaway world, so the user's open level isn't touched, and makes it the
	// only selected actor. Returns false, having failed the test, if that doesn't work.
	bool SelectNewTestActor()
	{
		TestWorld = UWorld::CreateWorld(EWorldType::Inactive, /*bInformEngineOfWorld=*/ false);
		TestNotNull(TEXT("Test world"), TestWorld);
		if (!TestWorld)
		{
			return false;
		}
		TestActor = TestWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
		TestNotNull(TEXT("TestActor"), TestActor);
		if (!TestActor)
		{
			return false;
		}
		// Added to the selection directly, as GEditor->SelectActor can refuse actors outside a level
		GEditor->SelectNone(false, true, false);
		GEditor->GetSelectedActors()->Select(TestActor);
		return true;
	}

	// Creates a notify for Tag in the sandbox and returns its object path, or empty on failure.
	FString CreateNotify(const FString& Tag, const FString& Name, bool bIsActor)
	{
		return UGameplayCueToolset::CreateCueNotifyAsset(Tag, Sandbox->Folder(), Name, bIsActor);
	}
END_DEFINE_SPEC(FGameplayCueToolsetSpec)

void FGameplayCueToolsetSpec::Define()
{
	// Shared by every test, so each test creates its notifies under names no other test uses.
	// The name must not contain a configured cue path (such as the "/Game" fallback), or the
	// manager would treat the folder as valid even without the path added in BeforeEach.
	BeforeAll([this]()
	{
		Sandbox = MakeUnique<FTestAssetSandbox>(TEXT("GASToolsetsCueSpecTemp"));
		SandboxCuePath = Sandbox->Folder();
	});

	AfterAll([this]()
	{
		Sandbox.Reset();
		ClearTestCues();
	});

	BeforeEach([this]()
	{
		UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
		TagManager.AddNativeGameplayTag(FName(*AlphaTag), TEXT("GASToolsets test"));
		TagManager.AddNativeGameplayTag(FName(*BetaChildTag), TEXT("GASToolsets test"));

		// The sandbox must be a gameplay cue notify path, or the cue manager rejects notifies created in it.
		UAbilitySystemGlobals::Get().AddGameplayCueNotifyPath(SandboxCuePath);

		// The tests change the editor's actor selection, so put it back afterward
		SavedSelection.Reset();
		for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				SavedSelection.Add(Actor);
			}
		}
	});

	AfterEach([this]()
	{
		GEditor->SelectNone(false, true, false);
		TestActor = nullptr;
		if (TestWorld)
		{
			TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/ false);
			TestWorld = nullptr;
		}
		for (const TWeakObjectPtr<AActor>& Actor : SavedSelection)
		{
			if (AActor* SelectedActor = Actor.Get())
			{
				GEditor->SelectActor(SelectedActor, true, false);
			}
		}
		SavedSelection.Reset();

		UAbilitySystemGlobals::Get().RemoveGameplayCueNotifyPath(SandboxCuePath);
		ClearTestCues();
	});

	Describe(TEXT("ListCues"), [this]()
	{
		It(TEXT("lists every descendant of the given parent tag, sorted"), [this]()
		{
			const TArray<FString> Cues = UGameplayCueToolset::ListCues(TestRoot);
			TestEqual(TEXT("Cues"), Cues, TArray<FString>({ AlphaTag, BetaTag, BetaChildTag }));
			TestTrue(TEXT("Sorted"), Algo::IsSorted(Cues));
		});

		It(TEXT("lists from the GameplayCue root when the parent is empty"), [this]()
		{
			const TArray<FString> Cues = UGameplayCueToolset::ListCues(FString());
			TestTrue(TEXT("Includes Alpha"), Cues.Contains(AlphaTag));
			TestTrue(TEXT("Includes Beta.Child"), Cues.Contains(BetaChildTag));
			TestTrue(TEXT("Sorted"), Algo::IsSorted(Cues));
		});

		It(TEXT("returns empty for a leaf tag"), [this]()
		{
			TestTrue(TEXT("Empty"), UGameplayCueToolset::ListCues(AlphaTag).IsEmpty());
		});

		It(TEXT("raises when the parent tag does not exist"), [this]()
		{
			const FString Error = CaptureError([&]() { UGameplayCueToolset::ListCues(MissingTag); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("does not exist")));
		});
	});

	Describe(TEXT("CreateCueNotifyAsset"), [this]()
	{
		It(TEXT("creates a static notify bound to the tag and registers it with the cue manager"), [this]()
		{
			if (!RequireSandboxCuePath())
			{
				return;
			}
			const FString Path = CreateNotify(AlphaTag, TEXT("GCN_Static"), false);
			if (!TestFalse(TEXT("Returned a path"), Path.IsEmpty()))
			{
				return;
			}
			UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
			TestNotNull(TEXT("Loads as a Blueprint"), Blueprint);
			if (!Blueprint)
			{
				return;
			}
			UClass* GeneratedClass = Blueprint->GeneratedClass;
			TestNotNull(TEXT("Generated class"), GeneratedClass);
			if (!GeneratedClass)
			{
				return;
			}
			const UGameplayCueNotify_Static* CDO = Cast<UGameplayCueNotify_Static>(GeneratedClass->GetDefaultObject());
			TestNotNull(TEXT("Static notify CDO"), CDO);
			if (CDO)
			{
				TestEqual(TEXT("Cue tag"), CDO->GameplayCueTag.ToString(), AlphaTag);
				TestEqual(TEXT("Cue name"), CDO->GameplayCueName.ToString(), AlphaTag);
			}
			TestEqual(TEXT("Registered notify"), UGameplayCueToolset::GetCueInfo(AlphaTag).NotifyAssetPath, Path);
		});

		It(TEXT("creates an actor notify bound to the tag and registers it with the cue manager"), [this]()
		{
			if (!RequireSandboxCuePath())
			{
				return;
			}
			const FString Path = CreateNotify(AlphaTag, TEXT("GCN_Actor"), true);
			if (!TestFalse(TEXT("Returned a path"), Path.IsEmpty()))
			{
				return;
			}
			UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
			TestNotNull(TEXT("Loads as a Blueprint"), Blueprint);
			if (!Blueprint)
			{
				return;
			}
			UClass* GeneratedClass = Blueprint->GeneratedClass;
			TestNotNull(TEXT("Generated class"), GeneratedClass);
			if (!GeneratedClass)
			{
				return;
			}
			const AGameplayCueNotify_Actor* CDO = Cast<AGameplayCueNotify_Actor>(GeneratedClass->GetDefaultObject());
			TestNotNull(TEXT("Actor notify CDO"), CDO);
			if (CDO)
			{
				TestEqual(TEXT("Cue tag"), CDO->GameplayCueTag.ToString(), AlphaTag);
				TestEqual(TEXT("Cue name"), CDO->GameplayCueName.ToString(), AlphaTag);
			}
			TestEqual(TEXT("Registered notify"), UGameplayCueToolset::GetCueInfo(AlphaTag).NotifyAssetPath, Path);
		});

		It(TEXT("raises for an empty cue tag"), [this]()
		{
			const FString Error = CaptureError([&]() { CreateNotify(FString(), TEXT("GCN_NoTag"), false); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("CueTag")));
		});

		It(TEXT("raises for an empty package path"), [this]()
		{
			const FString Error = CaptureError([&]()
			{
				UGameplayCueToolset::CreateCueNotifyAsset(AlphaTag, FString(), TEXT("GCN_NoPath"), false);
			});
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("PackagePath")));
		});

		It(TEXT("raises for an empty asset name"), [this]()
		{
			const FString Error = CaptureError([&]() { CreateNotify(AlphaTag, FString(), false); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("AssetName")));
		});

		It(TEXT("raises when the cue tag does not exist"), [this]()
		{
			const FString Error = CaptureError([&]() { CreateNotify(MissingTag, TEXT("GCN_MissingTag"), false); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("does not exist")));
		});

		It(TEXT("raises for a folder outside the project's asset roots"), [this]()
		{
			const FString Error = CaptureError([&]()
			{
				UGameplayCueToolset::CreateCueNotifyAsset(AlphaTag, TEXT("/Engine/Transient"), TEXT("GCN_BadFolder"), false);
			});
			TestFalse(TEXT("Raises"), Error.IsEmpty());
		});

		It(TEXT("raises for a folder that is not a gameplay cue notify path"), [this]()
		{
			UAbilitySystemGlobals::Get().RemoveGameplayCueNotifyPath(SandboxCuePath);

			// The manager accepts any folder containing a configured cue path, so a broad project
			// config makes every folder valid and there is nothing to reject.
			if (IsCuePath(SandboxCuePath / TEXT("GCN_BadCuePath")))
			{
				AddInfo(TEXT("Skipped: the project's gameplay cue notify paths include the test folder."));
				return;
			}

			const FString Error = CaptureError([&]() { CreateNotify(AlphaTag, TEXT("GCN_BadCuePath"), false); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("not in a gameplay cue notify path")));
			TestNull(TEXT("Asset not created"), FindObject<UObject>(nullptr, *(SandboxCuePath / TEXT("GCN_BadCuePath.GCN_BadCuePath"))));
		});

		It(TEXT("raises when the tag already has a notify"), [this]()
		{
			if (!RequireSandboxCuePath())
			{
				return;
			}
			AddCue(AlphaTag, TEXT("/Game/GASToolsetsTest/GCN_Existing.GCN_Existing_C"));

			const FString Error = CaptureError([&]() { CreateNotify(AlphaTag, TEXT("GCN_SecondNotify"), false); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("already has a notify")));
			TestTrue(TEXT("Names the existing notify"), Error.Contains(TEXT("/Game/GASToolsetsTest/GCN_Existing.GCN_Existing")));
			TestNull(TEXT("Asset not created"), FindObject<UObject>(nullptr, *(SandboxCuePath / TEXT("GCN_SecondNotify.GCN_SecondNotify"))));
		});

		It(TEXT("raises when the asset already exists"), [this]()
		{
			if (!RequireSandboxCuePath())
			{
				return;
			}
			TestFalse(TEXT("First create succeeds"), CreateNotify(AlphaTag, TEXT("GCN_Duplicate"), false).IsEmpty());
			// A different tag, so this reaches the existing-asset check rather than the existing-notify one
			const FString Error = CaptureError([&]() { CreateNotify(BetaChildTag, TEXT("GCN_Duplicate"), false); });
			TestTrue(TEXT("Duplicate raises"), Error.Contains(TEXT("already exists")));
		});
	});

	Describe(TEXT("GetCueInfo"), [this]()
	{
		It(TEXT("reports no notify for a tag that has none"), [this]()
		{
			const FGameplayCueInfo Info = UGameplayCueToolset::GetCueInfo(BetaChildTag);
			TestEqual(TEXT("Tag"), Info.Tag, BetaChildTag);
			TestTrue(TEXT("No notify path"), Info.NotifyAssetPath.IsEmpty());
		});

		It(TEXT("returns the asset path of the registered notify"), [this]()
		{
			AddCue(AlphaTag, TEXT("/Game/GASToolsetsTest/GCN_Fake.GCN_Fake_C"));
			const FGameplayCueInfo Info = UGameplayCueToolset::GetCueInfo(AlphaTag);
			TestEqual(TEXT("Tag"), Info.Tag, AlphaTag);
			TestEqual(TEXT("Notify path"), Info.NotifyAssetPath, FString(TEXT("/Game/GASToolsetsTest/GCN_Fake.GCN_Fake")));
		});

		It(TEXT("does not report a parent tag's notify for a child tag"), [this]()
		{
			AddCue(BetaTag, TEXT("/Game/GASToolsetsTest/GCN_Parent.GCN_Parent_C"));
			TestFalse(TEXT("Parent has its notify"), UGameplayCueToolset::GetCueInfo(BetaTag).NotifyAssetPath.IsEmpty());
			TestTrue(TEXT("Child has none"), UGameplayCueToolset::GetCueInfo(BetaChildTag).NotifyAssetPath.IsEmpty());
		});

		It(TEXT("raises when the tag does not exist"), [this]()
		{
			const FString Error = CaptureError([&]() { UGameplayCueToolset::GetCueInfo(MissingTag); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("does not exist")));
		});
	});

	Describe(TEXT("FindCueTagsWithoutNotifies"), [this]()
	{
		It(TEXT("lists tags with no notify of their own, and drops a tag once it has one"), [this]()
		{
			const TArray<FString> Before = UGameplayCueToolset::FindCueTagsWithoutNotifies();
			TestTrue(TEXT("Alpha listed before"), Before.Contains(AlphaTag));
			TestTrue(TEXT("Beta.Child listed before"), Before.Contains(BetaChildTag));
			TestTrue(TEXT("Sorted"), Algo::IsSorted(Before));

			AddCue(AlphaTag, TEXT("/Game/GASToolsetsTest/GCN_Fake.GCN_Fake_C"));

			const TArray<FString> After = UGameplayCueToolset::FindCueTagsWithoutNotifies();
			TestFalse(TEXT("Alpha dropped after"), After.Contains(AlphaTag));
			TestTrue(TEXT("Beta.Child still listed"), After.Contains(BetaChildTag));
		});

		It(TEXT("still lists a child tag whose parent has a notify"), [this]()
		{
			AddCue(BetaTag, TEXT("/Game/GASToolsetsTest/GCN_Parent.GCN_Parent_C"));
			const TArray<FString> Tags = UGameplayCueToolset::FindCueTagsWithoutNotifies();
			TestFalse(TEXT("Beta dropped"), Tags.Contains(BetaTag));
			TestTrue(TEXT("Beta.Child listed"), Tags.Contains(BetaChildTag));
		});
	});

	Describe(TEXT("ExecuteCueOnSelectedActor"), [this]()
	{
		It(TEXT("executes the cue on the selected actor"), [this]()
		{
			if (!SelectNewTestActor())
			{
				return;
			}

			TestTrue(TEXT("Executed"), UGameplayCueToolset::ExecuteCueOnSelectedActor(
				AlphaTag, 0.5f, FVector::ZeroVector, FVector::UpVector));
		});

		It(TEXT("raises when no actor is selected"), [this]()
		{
			GEditor->SelectNone(false, true, false);
			const FString Error = CaptureError([&]()
			{
				UGameplayCueToolset::ExecuteCueOnSelectedActor(AlphaTag, 1.f, FVector::ZeroVector, FVector::UpVector);
			});
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("No actor selected")));
		});

		It(TEXT("raises when the cue tag does not exist"), [this]()
		{
			if (!SelectNewTestActor())
			{
				return;
			}

			const FString Error = CaptureError([&]()
			{
				UGameplayCueToolset::ExecuteCueOnSelectedActor(MissingTag, 1.f, FVector::ZeroVector, FVector::UpVector);
			});
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("does not exist")));
		});
	});

	// AddCueTag and RemoveCueTag write the project's tag INI on success, so only their
	// argument and precondition errors are exercised here.
	Describe(TEXT("AddCueTag"), [this]()
	{
		It(TEXT("raises for an empty tag"), [this]()
		{
			const FString Error = CaptureError([&]() { UGameplayCueToolset::AddCueTag(FString(), FString()); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("CueTag")));
		});

		It(TEXT("raises for a tag outside the GameplayCue root"), [this]()
		{
			const FString Error = CaptureError([&]() { UGameplayCueToolset::AddCueTag(TEXT("Other.Tag"), FString()); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("must begin with")));
		});
	});

	Describe(TEXT("RemoveCueTag"), [this]()
	{
		It(TEXT("raises when the tag does not exist"), [this]()
		{
			const FString Error = CaptureError([&]() { UGameplayCueToolset::RemoveCueTag(MissingTag); });
			TestTrue(TEXT("Raises"), Error.Contains(TEXT("does not exist")));
		});
	});
}

#endif  // WITH_DEV_AUTOMATION_TESTS

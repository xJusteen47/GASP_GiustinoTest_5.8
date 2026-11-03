// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Bake/GSTN_BakeAll.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "FileHelpers.h"
#include "GSTNCombatEditorModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Preview/GSTN_PreviewSubject.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_CombatTimeline.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	IConsoleCommand* BakeAllCommand = nullptr;
}

FText GSTNBakeAll::FResult::Summary() const
{
	return FText::Format(LOCTEXT("BakeAllSummary", "Bake all: {0} baked, {1} failed, {2} already up to date or not needed, {3} skipped."),
		FText::AsNumber(Baked), FText::AsNumber(Failed), FText::AsNumber(UpToDate), FText::AsNumber(Skipped));
}

USkeletalMesh* GSTNBakeAll::ResolveMesh(const UGSTN_CombatTimeline& Timeline)
{
	// 1. What the designer previews this timeline with (kept per asset by the timeline editor)
	FGSTN_PreviewSubject Subject;
	if (UGSTN_CombatEditorPreviewState::Find(FSoftObjectPath(&Timeline), Subject))
	{
		if (USkeletalMesh* Mesh = GSTNPreviewSubject::Resolve(Subject).Mesh)
		{
			return Mesh;
		}
	}

#if WITH_EDITORONLY_DATA
	// 2. The mesh of the last bake: re-baking with the mesh it was baked from changes nothing but what changed
	if (Timeline.Data.BakeInfo.IsSet())
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Timeline.Data.BakeInfo.Mesh.TryLoad()))
		{
			return Mesh;
		}
	}
#endif

	// 3. The settings' default preview subject
	const UGSTN_CombatEditorSettings& Settings = UGSTN_CombatEditorSettings::Get();
	if (!Settings.DefaultPreviewKit.IsNull() || !Settings.DefaultPreviewSkin.IsNull())
	{
		FGSTN_PreviewSubject Default;
		Default.Kind = EGSTN_PreviewSubjectKind::KitSkin;
		Default.Kit = Settings.DefaultPreviewKit;
		Default.Skin = Settings.DefaultPreviewSkin;
		return GSTNPreviewSubject::Resolve(Default).Mesh;
	}
	return nullptr;
}

void GSTNBakeAll::CollectAssets(const FOptions& Options, TArray<FAssetData>& OutAssets)
{
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	TArray<FAssetData> All;
	Registry.GetAssetsByClass(UGSTN_CombatTimeline::StaticClass()->GetClassPathName(), All, true);
	for (const FAssetData& Asset : All)
	{
		const FString Package = Asset.PackageName.ToString();
		if (Package.StartsWith(TEXT("/Engine/")) || Package.StartsWith(TEXT("/Temp/")))
		{
			continue;
		}
		if (!Options.PathPrefix.IsEmpty() && !Package.StartsWith(Options.PathPrefix))
		{
			continue;
		}
		OutAssets.Add(Asset);
	}
	OutAssets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath()); });
}

GSTNBakeAll::FResult GSTNBakeAll::Run(const FOptions& Options)
{
	FResult Result;
	TArray<FAssetData> Assets;
	CollectAssets(Options, Assets);

	FScopedSlowTask Progress(Assets.Num(), LOCTEXT("BakeAllProgress", "Baking sockets"), Options.bShowProgress);
	if (Options.bShowProgress)
	{
		Progress.MakeDialog(true);
	}

	const float TickRate = UGSTN_CombatEditorSettings::GetSimulationTickRate();
	TArray<UPackage*> ToSave;
	for (const FAssetData& AssetData : Assets)
	{
		if (Options.bShowProgress)
		{
			if (Progress.ShouldCancel())
			{
				break;
			}
			Progress.EnterProgressFrame(1.f, FText::FromString(AssetData.AssetName.ToString()));
		}

		FEntry& Entry = Result.Entries.AddDefaulted_GetRef();
		Entry.Asset = AssetData;
		UGSTN_CombatTimeline* Timeline = Cast<UGSTN_CombatTimeline>(AssetData.GetAsset());
		if (Timeline == nullptr)
		{
			Entry.Note = LOCTEXT("BakeAllNoLoad", "The asset could not be loaded.");
			++Result.Skipped;
			continue;
		}

		USkeletalMesh* Mesh = ResolveMesh(*Timeline);
		FGSTN_BakeInputs Inputs;
		Inputs.Mesh = Mesh;
		Inputs.TickRateHz = TickRate;
		if (!Timeline->Data.Montage.IsNull())
		{
			Inputs.Montage = Timeline->Data.Montage.LoadSynchronous();
		}
		const FGSTN_BakeStatusResult Status = GSTNBake::GetStatus(Timeline->Data, Inputs);
		Entry.Before = Status.Status;

		if (Status.Status == EGSTN_BakeStatus::NotNeeded || Status.Status == EGSTN_BakeStatus::Fresh)
		{
			Entry.Note = Status.Reason;
			++Result.UpToDate;
			continue;
		}
		if (Mesh == nullptr)
		{
			Entry.Note = LOCTEXT("BakeAllNoMesh", "No mesh to bake from: open the timeline, choose a preview subject, and bake there.");
			++Result.Skipped;
			continue;
		}

		const FGSTN_BakeResult Baked = FGSTN_SocketBaker::BakeStandalone(Timeline->Data, Mesh, TickRate);
		if (!Baked.bBaked)
		{
			Entry.Note = Baked.DescribeProblems();
			++Result.Failed;
			UE_LOG(LogGSTNCombatEditor, Warning, TEXT("Bake all: %s was not baked. %s"), *AssetData.GetObjectPathString(), *Entry.Note.ToString());
			continue;
		}
		Timeline->Modify();
		FGSTN_SocketBaker::Apply(Timeline->Data, Baked);
		Timeline->MarkPackageDirty();
		ToSave.AddUnique(Timeline->GetOutermost());
		Entry.bBaked = true;
		++Result.Baked;
	}

	if (Options.bSave && !ToSave.IsEmpty())
	{
		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	}
	return Result;
}

void GSTNBakeAll::RegisterConsoleCommand()
{
	BakeAllCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("gstn.Combat.BakeAll"),
		TEXT("Bakes the sockets of every combat timeline asset whose bake is missing or stale. Arguments: save (save the packages that were baked), and a package path prefix such as /Game/Combat. Without save the assets are left modified for review."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			FOptions Options;
			Options.bShowProgress = true;
			for (const FString& Arg : Args)
			{
				if (Arg.Equals(TEXT("save"), ESearchCase::IgnoreCase))
				{
					Options.bSave = true;
				}
				else
				{
					Options.PathPrefix = Arg;
				}
			}
			const FResult Result = Run(Options);
			for (const FEntry& Entry : Result.Entries)
			{
				if (Entry.bBaked || !Entry.Note.IsEmpty())
				{
					UE_LOG(LogGSTNCombatEditor, Log, TEXT("Bake all: %s: %s"), *Entry.Asset.GetObjectPathString(), Entry.bBaked ? TEXT("baked") : *Entry.Note.ToString());
				}
			}
			UE_LOG(LogGSTNCombatEditor, Log, TEXT("%s"), *Result.Summary().ToString());
		}), ECVF_Default);
}

void GSTNBakeAll::UnregisterConsoleCommand()
{
	if (BakeAllCommand != nullptr)
	{
		IConsoleManager::Get().UnregisterConsoleObject(BakeAllCommand);
		BakeAllCommand = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE

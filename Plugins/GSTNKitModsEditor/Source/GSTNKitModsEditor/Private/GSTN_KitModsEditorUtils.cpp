// Editor tooling of kit mods (M11 L8). Editor only.

#include "GSTN_KitModsEditorUtils.h"

#include "Editor.h"
#include "GSTN_KitModDefinition.h"
#include "GSTN_KitModsEditorRules.h"
#include "GSTN_KitModsEditorSettings.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNKitModsEditor, Log, All);

float UGSTN_KitModsEditorSettings::GetReachMeshTolerance()
{
	const UGSTN_KitModsEditorSettings* Settings = GetDefault<UGSTN_KitModsEditorSettings>();
	return Settings ? Settings->ReachMeshTolerance : 0.25f;
}

namespace
{
	TArray<const UGSTN_KitModDefinition*> ToConst(const TArray<UGSTN_KitModDefinition*>& Mods)
	{
		TArray<const UGSTN_KitModDefinition*> Out;
		for (const UGSTN_KitModDefinition* Mod : Mods)
		{
			if (Mod)
			{
				Out.Add(Mod);
			}
		}
		return Out;
	}

	/** Two moves of one command (kit and weapon) bake from different bases, so the preview name says which source it is */
	FString SourceSuffix(const FGSTN_ModMovePreview& Move)
	{
		return Move.WeaponIndex == INDEX_NONE ? FString(TEXT("Kit")) : FString::Printf(TEXT("W%d"), Move.WeaponIndex);
	}
}

TArray<FGameplayTag> UGSTN_KitModsEditorUtils::GetModdedCommands(UGSTN_PrimaryKitDefinition* Kit, const TArray<UGSTN_KitModDefinition*>& Mods)
{
	TArray<FGameplayTag> Commands;
	if (!Kit)
	{
		return Commands;
	}
	FGSTN_KitModPreview Preview;
	FGSTN_KitModsEditorRules::PreviewModsOnKit(*Kit, ToConst(Mods), Preview);
	for (const FGSTN_ModMovePreview& Move : Preview.Moves)
	{
		Commands.AddUnique(Move.Command);
	}
	return Commands;
}

UGSTN_CombatTimeline* UGSTN_KitModsEditorUtils::MakePreviewTimeline(UGSTN_PrimaryKitDefinition* Kit, const TArray<UGSTN_KitModDefinition*>& Mods, FGameplayTag Command,
	FString& OutMessage)
{
	if (!Kit)
	{
		OutMessage = TEXT("No kit");
		return nullptr;
	}
	FGSTN_KitModPreview Preview;
	FGSTN_KitModsEditorRules::PreviewModsOnKit(*Kit, ToConst(Mods), Preview);
	const FGSTN_ModMovePreview* Move = Preview.Moves.FindByPredicate([&Command](const FGSTN_ModMovePreview& Entry) { return Entry.Command == Command; });
	if (!Move)
	{
		OutMessage = FString::Printf(TEXT("The mods change no move on %s of kit %s"), *Command.ToString(), *Kit->GetName());
		for (const FString& Warning : Preview.Warnings)
		{
			OutMessage += TEXT("\n") + Warning;
		}
		return nullptr;
	}

	// A real (non transient) object in a /Temp package: the asset editor opens assets only, and /Temp is never saved nor validated
	const FString Name = FString::Printf(TEXT("%s_%s"), *Move->DerivedName.ToString(), *SourceSuffix(*Move));
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/GSTNModPreview/%s"), *Name));
	UGSTN_CombatTimeline* Timeline = FindObject<UGSTN_CombatTimeline>(Package, *Name);
	if (!Timeline)
	{
		Timeline = NewObject<UGSTN_CombatTimeline>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
	}
	Timeline->Data = Move->Derived;
	Timeline->PostEditChange();
	Package->SetDirtyFlag(false);

	OutMessage = FString::Printf(TEXT("Preview of %s on kit %s: time x%.3f, reach x%.3f%s (base %s)"), *Command.ToString(), *Kit->GetName(),
		Move->Modifier.TimeScale, Move->Modifier.ReachScale, Move->bClamped ? TEXT(", cut back by the kit's limits") : TEXT(""), *Move->BasePath.ToString());
	return Timeline;
}

bool UGSTN_KitModsEditorUtils::OpenPreview(UGSTN_PrimaryKitDefinition* Kit, const TArray<UGSTN_KitModDefinition*>& Mods, FGameplayTag Command, FString& OutMessage)
{
	UGSTN_CombatTimeline* Timeline = MakePreviewTimeline(Kit, Mods, Command, OutMessage);
	if (!Timeline || !GEditor)
	{
		return false;
	}
	UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	return Editors && Editors->OpenEditorForAsset(Timeline);
}

void UGSTN_KitModsEditorUtils::RunConsoleCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 3)
	{
		UE_LOG(LogGSTNKitModsEditor, Warning, TEXT("Usage: GSTN.KitMods.Preview KitPath ModPath[+ModPath...] CommandTag"));
		return;
	}
	UGSTN_PrimaryKitDefinition* Kit = LoadObject<UGSTN_PrimaryKitDefinition>(nullptr, *Args[0]);
	TArray<FString> ModPaths;
	Args[1].ParseIntoArray(ModPaths, TEXT("+"));
	TArray<UGSTN_KitModDefinition*> Mods;
	for (const FString& Path : ModPaths)
	{
		if (UGSTN_KitModDefinition* Mod = LoadObject<UGSTN_KitModDefinition>(nullptr, *Path))
		{
			Mods.Add(Mod);
		}
		else
		{
			UE_LOG(LogGSTNKitModsEditor, Warning, TEXT("GSTN.KitMods.Preview: %s is not a kit mod"), *Path);
		}
	}
	const FGameplayTag Command = FGameplayTag::RequestGameplayTag(FName(*Args[2]), false);
	FString Message;
	const bool bOpened = OpenPreview(Kit, Mods, Command, Message);
	UE_LOG(LogGSTNKitModsEditor, Display, TEXT("GSTN.KitMods.Preview: %s%s"), *Message, bOpened ? TEXT("") : TEXT(" (not opened)"));
}

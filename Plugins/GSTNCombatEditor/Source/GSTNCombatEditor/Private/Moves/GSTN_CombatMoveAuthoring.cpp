// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Moves/GSTN_CombatMoveAuthoring.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameplayAbilityBlueprint.h"
#include "Graph/GSTN_MoveGraphAuthoring.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Misc/PackageName.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/Package.h"

namespace GSTNMoveAuthoring
{
	// The starter strike: the defaults of the examples' light attack, a starting point to retune, not a balance pass
	constexpr float StrikeRadius = 50.f;
	constexpr float StrikeReachFrom = 80.f;
	constexpr float StrikeReachTo = 160.f;
	constexpr float StrikeDamage = 6.f;
	constexpr float StrikeBaseKnockback = 500.f;
	constexpr float StrikeAngleDeg = 30.f;

	/** The move events a graph move starts with, laid out in a column (name, y). On Move Frame is left out: it is opt-in (bTickMoveInBlueprint) */
	const TPair<const TCHAR*, int32> MoveEvents[] = {
		{ TEXT("K2_OnMoveStarted"), 0 },
		{ TEXT("K2_OnMoveKey"), 200 },
		{ TEXT("K2_OnMoveHitConfirmed"), 400 },
		{ TEXT("K2_OnMoveEnded"), 600 },
	};

	FString ClassName(const UClass* Class)
	{
		return Class ? Class->GetName() : FString(TEXT("(none)"));
	}
}

FGSTN_NewCombatMoveOptions::FGSTN_NewCombatMoveOptions()
	: ParentClass(UGSTN_GA_BaseCombat::StaticClass())
{
}

TArray<FString> UGSTN_CombatMoveAuthoring::SanitizeOptions(FGSTN_NewCombatMoveOptions& Options)
{
	TArray<FString> Changes;
	auto Fix = [&Changes](int32& Value, int32 Min, int32 Max, const TCHAR* What)
	{
		const int32 Fixed = FMath::Clamp(Value, Min, FMath::Max(Min, Max));
		if (Fixed != Value)
		{
			Changes.Add(FString::Printf(TEXT("%s %d -> %d"), What, Value, Fixed));
			Value = Fixed;
		}
	};
	Fix(Options.TotalFrames, 1, MAX_int32, TEXT("total frames"));
	Fix(Options.ActiveStart, 0, Options.TotalFrames - 1, TEXT("active start"));
	Fix(Options.ActiveEnd, Options.ActiveStart, Options.TotalFrames - 1, TEXT("active end"));
	Fix(Options.FirstActionableFrame, Options.ActiveEnd + 1, Options.TotalFrames, TEXT("first actionable frame"));
	if (Options.MontagePlayRate < 0.01f)
	{
		Changes.Add(FString::Printf(TEXT("montage play rate %.3f -> 1"), Options.MontagePlayRate));
		Options.MontagePlayRate = 1.f;
	}
	if (!Options.ParentClass)
	{
		Changes.Add(TEXT("parent class (none) -> GSTN_GA_BaseCombat"));
		Options.ParentClass = UGSTN_GA_BaseCombat::StaticClass();
	}
	return Changes;
}

FGSTN_CombatTimelineData UGSTN_CombatMoveAuthoring::MakeStarterTimeline(const FGSTN_NewCombatMoveOptions& InOptions)
{
	using namespace GSTNMoveAuthoring;
	FGSTN_NewCombatMoveOptions Options = InOptions;
	SanitizeOptions(Options);

	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = Options.TotalFrames;
	Data.FirstActionableFrame = Options.FirstActionableFrame;
	Data.Montage = Options.Montage;
	Data.MontagePlayRate = Options.MontagePlayRate;

	switch (Options.Template)
	{
	case EGSTN_CombatMoveTemplate::Strike:
	{
		FGSTN_HitboxSpec Box;
		Box.Shape.Type = EGSTN_ShapeType::Sphere;
		Box.Shape.Extent = FVector(StrikeRadius);
		Box.Space = EGSTN_HitboxSpace::Root;
		Box.HitGroup = 0;
		// Keys are relative to the track start: from close to far over the active frames, swept so a thin hurtbox is not skipped
		FGSTN_HitboxKey From;
		From.Frame = 0;
		From.Offset = FVector(StrikeReachFrom, 0.0, 0.0);
		FGSTN_HitboxKey To;
		To.Frame = Options.ActiveEnd - Options.ActiveStart;
		To.Offset = FVector(StrikeReachTo, 0.0, 0.0);
		Box.Keys = { From, To };
		Box.bSweepBetweenFrames = true;
		Box.Data.Damage = StrikeDamage;
		Box.Data.BaseKnockback = StrikeBaseKnockback;
		Box.Data.AngleDeg = StrikeAngleDeg;

		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = Options.ActiveStart;
		Track.EndFrame = Options.ActiveEnd + 1;
		Track.Hitboxes.Add(Box);
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		break;
	}
	case EGSTN_CombatMoveTemplate::Action:
	{
		FGSTN_TimelineTrack_Event Key;
		Key.StartFrame = Options.ActiveStart;
		Key.EndFrame = Options.ActiveStart + 1;
		Key.KeyTag = Options.KeyTag;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(Key));
		break;
	}
	case EGSTN_CombatMoveTemplate::Empty:
		break;
	}
	return Data;
}

bool UGSTN_CombatMoveAuthoring::IsCombatMoveBlueprint(const UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UGSTN_GA_BaseCombat::StaticClass());
}

FString UGSTN_CombatMoveAuthoring::TimelineAssetNameFor(const FString& MoveName)
{
	FString Base = MoveName;
	Base.RemoveFromStart(TEXT("GA_"), ESearchCase::CaseSensitive);
	return TEXT("TL_") + (Base.IsEmpty() ? FString(TEXT("Move")) : Base);
}

UBlueprint* UGSTN_CombatMoveAuthoring::CreateCombatMove(const FString& FolderPath, const FString& AssetName, FGSTN_NewCombatMoveOptions Options,
	FString& OutReport, FString& OutError)
{
	OutReport.Reset();
	OutError.Reset();
	const FString PackageName = FolderPath / AssetName;
	if (AssetName.IsEmpty() || !FPackageName::IsValidLongPackageName(PackageName))
	{
		OutError = FString::Printf(TEXT("%s is not a valid asset path (a mounted root such as /Game/Moves, then a name)"), *PackageName);
		return nullptr;
	}
	if (FPackageName::DoesPackageExist(PackageName) || FindPackage(nullptr, *PackageName))
	{
		OutError = FString::Printf(TEXT("%s already exists"), *PackageName);
		return nullptr;
	}
	UPackage* Package = CreatePackage(*PackageName);
	UBlueprint* Blueprint = CreateCombatMoveIn(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional, MoveTemp(Options), OutReport, OutError);
	if (Blueprint)
	{
		FAssetRegistryModule::AssetCreated(Blueprint);
	}
	return Blueprint;
}

UBlueprint* UGSTN_CombatMoveAuthoring::CreateCombatMoveIn(UObject* Outer, FName Name, EObjectFlags Flags, FGSTN_NewCombatMoveOptions Options,
	FString& OutReport, FString& OutError)
{
	using namespace GSTNMoveAuthoring;
	OutReport.Reset();
	OutError.Reset();
	TArray<FString> Report = SanitizeOptions(Options);
	UClass* Parent = Options.ParentClass.Get();
	if (!Outer || Name.IsNone() || !Parent->IsChildOf(UGSTN_GA_BaseCombat::StaticClass()) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(Parent))
	{
		OutError = FString::Printf(TEXT("cannot make a combat move of class %s"), *ClassName(Parent));
		return nullptr;
	}

	// The asset is a Gameplay Ability Blueprint (the engine shows it as one), with a plain K2 event graph: the move events are Blueprint
	// events of the parent class, which any K2 graph can override, and the graph authoring library finds that graph by its standard name
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Outer, Name, BPTYPE_Normal, UGameplayAbilityBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(), FName(TEXT("GSTNCombatMoveFactory")));
	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("could not make the Blueprint %s"), *Name.ToString());
		return nullptr;
	}
	Blueprint->SetFlags(Flags);

	if (Options.bAddMoveEvents)
	{
		FString Error;
		for (const TPair<const TCHAR*, int32>& Event : MoveEvents)
		{
			if (UGSTN_MoveGraphAuthoring::AddEventNode(Blueprint, Event.Key, 0, Event.Value, Error).IsEmpty())
			{
				Report.Add(FString::Printf(TEXT("event %s not added: %s"), Event.Key, *Error));
			}
		}
	}
	TArray<FString> Messages;
	if (UGSTN_MoveGraphAuthoring::CompileAndReport(Blueprint, Messages) > 0)
	{
		OutError = FString::Printf(TEXT("%s does not compile: %s"), *Name.ToString(), *FString::Join(Messages, TEXT(" | ")));
		return Blueprint;
	}

	// The class defaults last: after the compile, so nothing regenerates the default object over them
	UGSTN_GA_BaseCombat* Defaults = Blueprint->GeneratedClass ? Cast<UGSTN_GA_BaseCombat>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Defaults)
	{
		OutError = FString::Printf(TEXT("%s has no class default object"), *Name.ToString());
		return Blueprint;
	}
	const FGSTN_CombatTimelineData Starter = MakeStarterTimeline(Options);
	Defaults->Modify();
	if (Options.bTimelineAsAsset)
	{
		const FString Folder = FPackageName::GetLongPackagePath(Outer->GetOutermost()->GetName());
		FString TimelinePackageName;
		FString TimelineName;
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateUniqueAssetName(Folder / TimelineAssetNameFor(Name.ToString()),
			FString(), TimelinePackageName, TimelineName);
		UPackage* TimelinePackage = CreatePackage(*TimelinePackageName);
		UGSTN_CombatTimeline* Timeline = NewObject<UGSTN_CombatTimeline>(TimelinePackage, FName(*TimelineName), RF_Public | RF_Standalone | RF_Transactional);
		Timeline->Data = Starter;
		FAssetRegistryModule::AssetCreated(Timeline);
		Timeline->MarkPackageDirty();
		Defaults->TimelineAsset = Timeline;
		Report.Add(FString::Printf(TEXT("timeline asset %s"), *TimelinePackageName));
	}
	else
	{
		Defaults->Timeline = Starter;
	}
	Blueprint->MarkPackageDirty();
	Report.Add(FString::Printf(TEXT("%s: %s, %d frames, active %d-%d, actionable %d"), *Name.ToString(), *UEnum::GetDisplayValueAsText(Options.Template).ToString(),
		Options.TotalFrames, Options.ActiveStart, Options.ActiveEnd, Options.FirstActionableFrame));

	if (Options.Hookup.WantsBinding())
	{
		FString BindReport;
		AddMoveToKit(Blueprint->GeneratedClass.Get(), Options.Hookup, BindReport);
		Report.Add(BindReport);
	}
	OutReport = FString::Join(Report, TEXT("\n"));
	return Blueprint;
}

bool UGSTN_CombatMoveAuthoring::UpsertBinding(TArray<FGSTN_AbilityBinding>& Bindings, const FGSTN_AbilityBinding& Binding)
{
	for (FGSTN_AbilityBinding& Existing : Bindings)
	{
		if (Existing.HasSameKey(Binding))
		{
			Existing = Binding;
			return true;
		}
	}
	Bindings.Add(Binding);
	return false;
}

bool UGSTN_CombatMoveAuthoring::AddMoveToKit(TSubclassOf<UGameplayAbility> MoveClass, const FGSTN_MoveKitHookup& Hookup, FString& OutReport)
{
	OutReport.Reset();
	UGSTN_PrimaryKitDefinition* Kit = Hookup.Kit.IsNull() ? nullptr : Hookup.Kit.LoadSynchronous();
	UGSTN_WeaponDefinition* Weapon = Hookup.Weapon.IsNull() ? nullptr : Hookup.Weapon.LoadSynchronous();
	TArray<FString> Missing;
	if (!Hookup.Kit.IsNull() && !Kit)
	{
		Missing.Add(FString::Printf(TEXT("kit %s does not load"), *Hookup.Kit.ToString()));
	}
	if (!Hookup.Weapon.IsNull() && !Weapon)
	{
		Missing.Add(FString::Printf(TEXT("weapon %s does not load"), *Hookup.Weapon.ToString()));
	}
	FString Report;
	const bool bBound = (Kit || Weapon) && AddMoveToKitAssets(MoveClass, Kit, Weapon, Hookup.Command, Hookup.Requirements, Report);
	if (!Report.IsEmpty())
	{
		Missing.Insert(Report, 0);
	}
	if (!Kit && !Weapon && Missing.IsEmpty())
	{
		Missing.Add(TEXT("no kit and no weapon named: the move is not bound"));
	}
	OutReport = FString::Join(Missing, TEXT("\n"));
	return bBound;
}

bool UGSTN_CombatMoveAuthoring::AddMoveToKitAssets(TSubclassOf<UGameplayAbility> MoveClass, UGSTN_PrimaryKitDefinition* Kit, UGSTN_WeaponDefinition* Weapon,
	FGameplayTag Command, const FGSTN_TagRequirements& Requirements, FString& OutReport)
{
	OutReport.Reset();
	if (!MoveClass)
	{
		OutReport = TEXT("no move class: nothing bound");
		return false;
	}
	if (!Command.IsValid())
	{
		OutReport = TEXT("no command: pick the command the move answers (Command.Attack.Neutral...), nothing bound");
		return false;
	}
	FGSTN_AbilityBinding Binding;
	Binding.Command = Command;
	Binding.Requirements = Requirements;
	Binding.Ability = TSoftClassPtr<UGameplayAbility>(MoveClass.Get());

	TArray<FString> Lines;
	auto Bind = [&](UObject* Asset, TArray<FGSTN_AbilityBinding>& Bindings)
	{
		Asset->Modify();
		const bool bReplaced = UpsertBinding(Bindings, Binding);
		Asset->MarkPackageDirty();
		Lines.Add(FString::Printf(TEXT("%s: %s %s -> %s (run Validate on it: the command must be in its command set)"), *Asset->GetName(),
			bReplaced ? TEXT("replaced") : TEXT("added"), *Command.ToString(), *MoveClass->GetName()));
	};
	if (Kit)
	{
		Bind(Kit, Kit->KitBindings);
	}
	if (Weapon)
	{
		Bind(Weapon, Weapon->Bindings);
	}
	OutReport = FString::Join(Lines, TEXT("\n"));
	return !Lines.IsEmpty();
}

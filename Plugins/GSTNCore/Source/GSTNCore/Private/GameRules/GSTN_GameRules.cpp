#include "GameRules/GSTN_GameRules.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNGameRules, Log, All);

namespace GSTN::GameRules::Private
{
	TWeakObjectPtr<const UGSTN_GameRules> GActive;
	uint32 GSerial = 0;
}

UGSTN_GameRulesPiece* UGSTN_GameRules::FindPiece(const TSubclassOf<UGSTN_GameRulesPiece> PieceClass) const
{
	for (UGSTN_GameRulesPiece* Piece : Pieces)
	{
		if (Piece && PieceClass && Piece->IsA(PieceClass))
		{
			return Piece;
		}
	}
	return nullptr;
}

const UGSTN_GameRules* FGSTN_ActiveGameRules::Get()
{
	return GSTN::GameRules::Private::GActive.Get();
}

void FGSTN_ActiveGameRules::Set(const UGSTN_GameRules* Rules)
{
	using namespace GSTN::GameRules::Private;
	const bool bChanged = GActive.Get() != Rules;
	GActive = Rules;
	UE_LOG(LogGSTNGameRules, Log, TEXT("GameRulesActive %s"), Rules ? *Rules->GetPathName() : TEXT("defaults"));
	if (bChanged)
	{
		++GSerial;
		OnChanged().Broadcast();
	}
}

uint32 FGSTN_ActiveGameRules::GetSerial()
{
	return GSTN::GameRules::Private::GSerial;
}

FSimpleMulticastDelegate& FGSTN_ActiveGameRules::OnChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

AGSTN_GameRulesInfo::AGSTN_GameRulesInfo()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetPriority = 10.f;
	SetNetUpdateFrequency(1.f);
}

void AGSTN_GameRulesInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AGSTN_GameRulesInfo, Rules, COND_InitialOnly);
}

void AGSTN_GameRulesInfo::OnRep_Rules()
{
	if (UGSTN_GameRulesSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UGSTN_GameRulesSubsystem>() : nullptr)
	{
		Subsystem->ApplyRules(Rules);
	}
}

bool UGSTN_GameRulesSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGSTN_GameRulesSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	ApplyRules(ChooseRules(InWorld));
	if (InWorld.GetNetMode() != NM_Standalone)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		if (AGSTN_GameRulesInfo* Info = InWorld.SpawnActor<AGSTN_GameRulesInfo>(Params))
		{
			Info->Rules = Rules;
		}
	}
}

void UGSTN_GameRulesSubsystem::Deinitialize()
{
	if (Rules && FGSTN_ActiveGameRules::Get() == Rules)
	{
		FGSTN_ActiveGameRules::Set(nullptr);
	}
	Rules = nullptr;
	Super::Deinitialize();
}

void UGSTN_GameRulesSubsystem::ApplyRules(const UGSTN_GameRules* InRules)
{
	Rules = InRules;
	FGSTN_ActiveGameRules::Set(InRules);
}

const UGSTN_GameRules* UGSTN_GameRulesSubsystem::ChooseRules(const UWorld& World)
{
	// 1. The URL (a mode chosen by the menu or the command line travels as GameRules=/Game/Path/Asset)
	if (const TCHAR* Option = World.URL.GetOption(TEXT("GameRules="), nullptr))
	{
		if (const UGSTN_GameRules* FromUrl = LoadObject<UGSTN_GameRules>(nullptr, Option))
		{
			return FromUrl;
		}
		UE_LOG(LogGSTNGameRules, Warning, TEXT("GameRules URL option names no rules asset: %s"), Option);
	}

	// 2-3. An asset that lists this level, else this game mode. Few assets, loaded once per world at begin play (outside the simulation)
	const IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UGSTN_GameRules::StaticClass()->GetClassPathName(), Assets, true);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	const FString LevelPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	const AGameModeBase* GameMode = World.GetAuthGameMode();
	const UGSTN_GameRules* ByMode = nullptr;
	for (const FAssetData& Data : Assets)
	{
		const UGSTN_GameRules* Candidate = Cast<UGSTN_GameRules>(Data.GetAsset());
		if (!Candidate)
		{
			continue;
		}
		for (const TSoftObjectPtr<UWorld>& Level : Candidate->Levels)
		{
			if (Level.ToSoftObjectPath().GetLongPackageName() == LevelPackage)
			{
				return Candidate;
			}
		}
		if (!ByMode && GameMode)
		{
			for (const TSoftClassPtr<AGameModeBase>& Mode : Candidate->GameModes)
			{
				if (const UClass* ModeClass = Mode.LoadSynchronous(); ModeClass && GameMode->IsA(ModeClass))
				{
					ByMode = Candidate;
					break;
				}
			}
		}
	}
	return ByMode;
}

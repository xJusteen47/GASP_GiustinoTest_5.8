// Levels mode, server side.

#include "GSTN_LevelsSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_LevelSetDefinition.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLevels, Log, All);

// Debug window variables (owner rule 11-21: every setting also in GSTN Debug Options, one control each)
static TAutoConsoleVariable<int32> CVarLevelsEnabled(TEXT("gstn.Levels.Enabled"), 0,
	TEXT("Levels mode (M12 G6). 0 = as the level's GameRules (off without an asset), 1 = force off, 2 = force on. Takes effect at the next poll; turning on starts the level from zero."));
static TAutoConsoleVariable<int32> CVarLevelsFriendlyFire(TEXT("gstn.Levels.FriendlyFire"), -1,
	TEXT("Friendly fire in Levels. -1 = the Levels piece of the GameRules (ON by default, GM.13), 0 = off, 1 = on. Read when the combat rules are prepared, before pawns simulate."));

namespace
{
	bool ShouldRun()
	{
		switch (CVarLevelsEnabled.GetValueOnGameThread())
		{
		case 1: return false;
		case 2: return true;
		default: return UGSTN_LevelsRules::Get().bEnabled;
		}
	}

	UWorld* FindServerWorld(UWorld* Given)
	{
		if (Given)
		{
			return Given;
		}
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && World->GetNetMode() != NM_Client)
			{
				return World;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdStatus(TEXT("gstn.Levels.Status"), TEXT("gstn.Levels.Status: the level's phase, goal, tally and score to the log (server)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_LevelsSubsystem* Levels = UGSTN_LevelsSubsystem::GetLevelsSubsystem(FindServerWorld(World)))
			{
				UE_LOG(LogGSTNLevels, Display, TEXT("%s"), *Levels->Describe());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdComplete(TEXT("gstn.Levels.Complete"), TEXT("gstn.Levels.Complete: completes the level now (server, debug)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_LevelsSubsystem* Levels = UGSTN_LevelsSubsystem::GetLevelsSubsystem(FindServerWorld(World)))
			{
				Levels->CompleteLevel();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdNext(TEXT("gstn.Levels.Next"), TEXT("gstn.Levels.Next: travel to the next level now, once the level is complete (server, debug)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_LevelsSubsystem* Levels = UGSTN_LevelsSubsystem::GetLevelsSubsystem(FindServerWorld(World)))
			{
				Levels->TravelToNextLevel();
			}
		}));

	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	/** The placeholder results screen: plain Slate text on a dark panel, no assets. A designer replaces it with UMG reading AGSTN_LevelStateInfo */
	class SLevelResults : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLevelResults) {}
			SLATE_ARGUMENT(FGSTN_LevelResults, Results)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			const FGSTN_LevelResults& Results = InArgs._Results;
			const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 28);
			const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);

			TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
			auto AddLine = [&Box](const FString& Text, const FSlateFontInfo& Font)
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 3.f)
				[
					SNew(STextBlock).Text(FText::FromString(Text)).Font(Font)
				];
			};

			FString Title = TEXT("LEVEL COMPLETE");
			if (Results.LevelIndex != INDEX_NONE)
			{
				Title += FString::Printf(TEXT("  (%d)"), Results.LevelIndex + 1);
			}
			AddLine(Title, TitleFont);
			AddLine(FString::Printf(TEXT("Time  %s"), *FormatTime(Results.ElapsedSeconds)), BodyFont);
			AddLine(FString::Printf(TEXT("Team collectibles  %d"), Results.TeamCollected), BodyFont);
			for (const FGSTN_ResultLine& Line : Results.Lines)
			{
				AddLine(FString::Printf(TEXT("%s  x%d  %+d"), *Line.Label.ToString(), Line.Count, Line.Points), BodyFont);
			}
			if (Results.TimeBonus != 0)
			{
				AddLine(FString::Printf(TEXT("Time bonus  %+d"), Results.TimeBonus), BodyFont);
			}
			if (Results.CompletionBonus != 0)
			{
				AddLine(FString::Printf(TEXT("Completion bonus  %+d"), Results.CompletionBonus), BodyFont);
			}
			AddLine(FString::Printf(TEXT("SCORE  %d%s"), Results.TotalScore, Results.RankLabel.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("   RANK %s"), *Results.RankLabel.ToString())), TitleFont);
			AddLine(Results.bFinalLevel ? TEXT("Last level of the set") : TEXT("Next level starting soon..."), BodyFont);

			ChildSlot
			[
				SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.8f))
					.Padding(FMargin(40.f, 24.f))
					[
						Box
					]
				]
			];
		}
	};
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// State actor and results screen

AGSTN_LevelStateInfo::AGSTN_LevelStateInfo()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
}

AGSTN_LevelStateInfo* AGSTN_LevelStateInfo::GetLevelState(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AGSTN_LevelStateInfo> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AGSTN_LevelStateInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_LevelStateInfo, Phase);
	DOREPLIFETIME(AGSTN_LevelStateInfo, LevelIndex);
	DOREPLIFETIME(AGSTN_LevelStateInfo, NumLevels);
	DOREPLIFETIME(AGSTN_LevelStateInfo, GraceEndTime);
	DOREPLIFETIME(AGSTN_LevelStateInfo, TeamCollected);
	DOREPLIFETIME(AGSTN_LevelStateInfo, Results);
}

void AGSTN_LevelStateInfo::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideResultsScreen();
	Super::EndPlay(EndPlayReason);
}

float AGSTN_LevelStateInfo::GetGraceSecondsLeft() const
{
	if (GraceEndTime < 0.f)
	{
		return 0.f;
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	return FMath::Max(0.f, GraceEndTime - static_cast<float>(Now));
}

void AGSTN_LevelStateInfo::NotifyChanged()
{
	OnStateChanged.Broadcast();
	RefreshResultsScreen();
}

void AGSTN_LevelStateInfo::OnRep_State()
{
	NotifyChanged();
}

void AGSTN_LevelStateInfo::RefreshResultsScreen()
{
	if (Phase != EGSTN_LevelPhase::Completed)
	{
		HideResultsScreen();
		return;
	}
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (ResultsWidget.IsValid() || !Viewport || World->GetNetMode() == NM_DedicatedServer || !FSlateApplication::IsInitialized())
	{
		return;
	}
	ResultsWidget = SNew(SLevelResults).Results(Results);
	Viewport->AddViewportWidgetContent(ResultsWidget.ToSharedRef(), 300);
}

void AGSTN_LevelStateInfo::HideResultsScreen()
{
	if (!ResultsWidget.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(ResultsWidget.ToSharedRef());
	}
	ResultsWidget.Reset();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Goal zone and collectible

AGSTN_LevelGoalZone::AGSTN_LevelGoalZone()
{
	if (Area)
	{
		Area->ShapeColor = FColor(255, 215, 0);
	}
}

FGSTN_GoalGraceResult AGSTN_LevelGoalZone::EvaluateGoal(double Now, int32 AliveInside, int32 AliveTotal, const FGSTN_GoalGraceParams& RulesParams)
{
	FGSTN_GoalGraceParams Params = RulesParams;
	if (GraceSecondsOverride >= 0.f)
	{
		Params.GraceSeconds = GraceSecondsOverride;
	}
	return Tracker.Update(Now, AliveInside, AliveTotal, Params);
}

void AGSTN_LevelGoalZone::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_LevelsSubsystem* Levels = UGSTN_LevelsSubsystem::GetLevelsSubsystem(this))
	{
		Levels->RegisterGoalZone(this);
	}
}

void AGSTN_LevelGoalZone::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGSTN_LevelsSubsystem* Levels = UGSTN_LevelsSubsystem::GetLevelsSubsystem(this))
	{
		Levels->UnregisterGoalZone(this);
	}
	Super::EndPlay(Reason);
}

AGSTN_LevelCollectible::AGSTN_LevelCollectible()
{
}

void AGSTN_LevelCollectible::BeginPlay()
{
	Super::BeginPlay();
	// Added here, not in the constructor: a class default object may be built before the native tags are registered
	const FGameplayTag Identity = GSTNLevelsTags::Levels_Collectible.GetTag();
	if (Identity.IsValid() && !CollectibleTags.HasTagExact(Identity))
	{
		CollectibleTags.AddTag(Identity);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Subsystem

UGSTN_LevelsSubsystem* UGSTN_LevelsSubsystem::GetLevelsSubsystem(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject && GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGSTN_LevelsSubsystem>() : nullptr;
}

bool UGSTN_LevelsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_LevelsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The event stream must exist before our listener is added
	Collection.InitializeDependency<UGSTN_SimEventSubsystem>();
}

void UGSTN_LevelsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (HasAuthority())
	{
		// Poll decides whether to run each time, so the order of this and the game rules subsystem does not matter
		InWorld.GetTimerManager().SetTimer(PollTimer, this, &UGSTN_LevelsSubsystem::Poll, 0.25f, true);
	}
}

void UGSTN_LevelsSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
	}
	if (bRunning)
	{
		Stop();
	}
	Super::Deinitialize();
}

bool UGSTN_LevelsSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

int32 UGSTN_LevelsSubsystem::PlayerKeyOf(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	return PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE;
}

bool UGSTN_LevelsSubsystem::GetEffectiveFriendlyFire() const
{
	const int32 Override = CVarLevelsFriendlyFire.GetValueOnGameThread();
	return Override >= 0 ? Override != 0 : UGSTN_LevelsRules::Get().bFriendlyFire;
}

FGSTN_PlayerCountScaling UGSTN_LevelsSubsystem::GetPlayerScaling() const
{
	return bRunning ? UGSTN_LevelsRules::Get().GetScaling(FMath::Max(1, PlayerCount)) : FGSTN_PlayerCountScaling();
}

void UGSTN_LevelsSubsystem::Start()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	const UGSTN_LevelsRules& Rules = UGSTN_LevelsRules::Get();

	LevelSet = Rules.LevelSet.IsNull() ? nullptr : Rules.LevelSet.LoadSynchronous();
	Score = Rules.ScoreRules.IsNull() ? nullptr : Rules.ScoreRules.LoadSynchronous();
	if (!Score)
	{
		Score = UGSTN_ScoreRules::MakeDefault(this);
	}
	LevelIndex = LevelSet ? LevelSet->FindIndexByMap(World->GetMapName()) : INDEX_NONE;
	if (LevelSet && LevelIndex == INDEX_NONE)
	{
		UE_LOG(LogGSTNLevels, Warning, TEXT("Map %s is not in the level set %s: it plays as a stand-alone level, no travel after the goal"), *World->GetMapName(), *GetNameSafe(LevelSet));
	}

	Tally.Reset();
	Downed.Reset();
	Results = FGSTN_LevelResults();
	Phase = EGSTN_LevelPhase::Playing;
	StartTime = World->GetTimeSeconds();
	GraceEnd = -1.0;
	for (const TWeakObjectPtr<AGSTN_LevelGoalZone>& Weak : GoalZones)
	{
		if (AGSTN_LevelGoalZone* Zone = Weak.Get())
		{
			Zone->ResetGoal();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	StateInfo = World->SpawnActor<AGSTN_LevelStateInfo>(SpawnParams);

	if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(World))
	{
		// Every event of the stream: the handler sorts them (down/up, restart, counters)
		Listener = Events->ListenNative(FGameplayTagQuery(), [this](const FGSTN_SimEvent& Event) { HandleEvent(Event); });
	}

	bRunning = true;
	World->GetTimerManager().SetTimer(PollTimer, this, &UGSTN_LevelsSubsystem::Poll, 1.f / FMath::Max(1.f, Rules.PollHz), true);
	UE_LOG(LogGSTNLevels, Log, TEXT("Levels mode on: level %d of %d, friendly fire %s"), LevelIndex, LevelSet ? LevelSet->GetNumLevels() : 0, GetEffectiveFriendlyFire() ? TEXT("on") : TEXT("off"));
	PublishEvent(GSTNLevelsTags::Message_Levels_LevelStarted.GetTag(), nullptr, static_cast<float>(LevelIndex));
	SyncState();
}

void UGSTN_LevelsSubsystem::Stop()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(TravelTimer);
		if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(World))
		{
			Events->StopListening(Listener);
		}
	}
	Listener = FGSTN_SimEventListenerHandle();
	if (AGSTN_LevelStateInfo* Info = StateInfo.Get())
	{
		if (World && !World->bIsTearingDown)
		{
			Info->Destroy();
		}
	}
	StateInfo.Reset();
	Phase = EGSTN_LevelPhase::Inactive;
	bRunning = false;
}

void UGSTN_LevelsSubsystem::PublishEvent(const FGameplayTag& Type, AActor* Instigator, float Magnitude) const
{
	if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(GetWorld()))
	{
		FGSTN_SimEvent Event;
		Event.EventType = Type;
		Event.Tags.AddTag(Type);
		Event.Instigator = Instigator;
		Event.Magnitude = Magnitude;
		Events->Broadcast(Event);
	}
}

void UGSTN_LevelsSubsystem::SyncState()
{
	AGSTN_LevelStateInfo* Info = StateInfo.Get();
	if (!Info)
	{
		return;
	}
	Info->Phase = Phase;
	Info->LevelIndex = LevelIndex;
	Info->NumLevels = LevelSet ? LevelSet->GetNumLevels() : 0;
	Info->GraceEndTime = GraceEnd >= 0.0 ? static_cast<float>(GraceEnd) : -1.f;
	Info->TeamCollected = Tally.TeamCollected;
	Info->Results = Results;
	Info->NotifyChanged();
}

void UGSTN_LevelsSubsystem::HandleEvent(const FGSTN_SimEvent& Event)
{
	if (!bRunning || Phase != EGSTN_LevelPhase::Playing)
	{
		return;
	}
	const UGSTN_LevelsRules& Rules = UGSTN_LevelsRules::Get();

	// Who is alive for the goal. A KO of an enemy lands here too and is pruned when the actor dies; only player pawns are ever looked up
	if (AActor* Target = Event.Target.Get())
	{
		if (Event.EventType.MatchesAny(Rules.DownEvents))
		{
			Downed.Add(Target);
		}
		else if (Event.EventType.MatchesAny(Rules.UpEvents))
		{
			Downed.Remove(Target);
		}
	}

	// The respawn framework restarted the level: collectibles are back (the missions reset on the same event), so the tally and the timers start again
	if (Event.EventType.MatchesAny(Rules.RestartEvents))
	{
		Tally.ResetForRestart(Score);
		GraceEnd = -1.0;
		for (const TWeakObjectPtr<AGSTN_LevelGoalZone>& Weak : GoalZones)
		{
			if (AGSTN_LevelGoalZone* Zone = Weak.Get())
			{
				Zone->ResetGoal();
			}
		}
		SyncState();
		return;
	}

	if (Tally.AddEvent(Score, Event, PlayerKeyOf(Event.Instigator.Get())))
	{
		SyncState();
	}
}

void UGSTN_LevelsSubsystem::Poll()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	const bool bWant = ShouldRun();
	if (bWant != bRunning)
	{
		bWant ? Start() : Stop();
	}
	if (!bRunning)
	{
		return;
	}

	for (auto It = Downed.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// Server-side positions of the alive players, outside the simulation; a knocked-out pawn waiting for its respawn is not alive (DownEvents)
	TArray<FVector> AliveLocations;
	PlayerCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller)
		{
			continue;
		}
		++PlayerCount;
		APawn* Pawn = Controller->GetPawn();
		if (Pawn && !Downed.Contains(TWeakObjectPtr<AActor>(Pawn)))
		{
			AliveLocations.Add(Pawn->GetActorLocation());
		}
	}
	const UGSTN_LevelsRules& Rules = UGSTN_LevelsRules::Get();
	if (PlayerCount > Rules.MaxPlayers)
	{
		UE_LOG(LogGSTNLevels, Warning, TEXT("%d players, the rules say at most %d (the game mode's player limit is the real gate)"), PlayerCount, Rules.MaxPlayers);
	}
	if (Phase != EGSTN_LevelPhase::Playing)
	{
		return;
	}

	FGSTN_GoalGraceParams Params;
	Params.GraceSeconds = Rules.GoalGraceSeconds;
	Params.bFinishEarlyWhenAllInside = Rules.bGoalFinishEarlyWhenAllInside;
	Params.bCancelWhenNobodyInside = Rules.bGoalCancelWhenNobodyInside;
	const double Now = World->GetTimeSeconds();

	AGSTN_LevelGoalZone* Finished = nullptr;
	double SoonestEnd = -1.0;
	for (const TWeakObjectPtr<AGSTN_LevelGoalZone>& Weak : GoalZones)
	{
		AGSTN_LevelGoalZone* Zone = Weak.Get();
		if (!Zone)
		{
			continue;
		}
		int32 Inside = 0;
		for (const FVector& Location : AliveLocations)
		{
			Inside += Zone->ContainsLocation(Location) ? 1 : 0;
		}
		const FGSTN_GoalGraceResult Result = Zone->EvaluateGoal(Now, Inside, AliveLocations.Num(), Params);
		if (Result.bGraceStarted)
		{
			const float Grace = Zone->GraceSecondsOverride >= 0.f ? Zone->GraceSecondsOverride : Rules.GoalGraceSeconds;
			UE_LOG(LogGSTNLevels, Log, TEXT("Goal %s: first alive player in, %.1f s of grace"), *GetNameSafe(Zone), Grace);
			PublishEvent(GSTNLevelsTags::Message_Levels_GoalGraceStarted.GetTag(), Zone, Grace);
		}
		if (Result.bCompleted && !Finished)
		{
			Finished = Zone;
		}
		const double End = Zone->GetGraceEnd();
		if (End >= 0.0 && (SoonestEnd < 0.0 || End < SoonestEnd))
		{
			SoonestEnd = End;
		}
	}

	if (Finished)
	{
		PublishEvent(GSTNLevelsTags::Message_Levels_GoalReached.GetTag(), Finished, 0.f);
		CompleteLevel();
	}
	else if (!FMath::IsNearlyEqual(SoonestEnd, GraceEnd))
	{
		GraceEnd = SoonestEnd;
		SyncState();
	}
}

bool UGSTN_LevelsSubsystem::CompleteLevel()
{
	UWorld* World = GetWorld();
	if (!World || !bRunning || !HasAuthority() || Phase != EGSTN_LevelPhase::Playing)
	{
		return false;
	}
	const UGSTN_LevelsRules& Rules = UGSTN_LevelsRules::Get();

	Phase = EGSTN_LevelPhase::Completed;
	GraceEnd = -1.0;
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - StartTime);
	Results = GSTNLevelsLogic::ComputeResults(*Score, Tally, Elapsed, true);
	Results.LevelIndex = LevelIndex;
	if (const FGSTN_LevelEntry* Entry = LevelSet ? LevelSet->GetEntry(LevelIndex) : nullptr)
	{
		Results.LevelId = Entry->GetId();
	}
	Results.bFinalLevel = LevelSet && LevelIndex != INDEX_NONE && LevelSet->GetNextIndex(LevelIndex) == INDEX_NONE;
	UE_LOG(LogGSTNLevels, Log, TEXT("Level complete in %.1f s: score %d rank %s, %d collectibles"), Elapsed, Results.TotalScore, *Results.RankId.ToString(), Results.TeamCollected);

	PublishEvent(GSTNLevelsTags::Message_Levels_LevelCompleted.GetTag(), nullptr, static_cast<float>(Results.TotalScore));
	if (Results.bFinalLevel)
	{
		PublishEvent(GSTNLevelsTags::Message_Levels_LevelSetCompleted.GetTag(), nullptr, 0.f);
	}
	SyncState();

	if (Rules.ResultsSeconds <= 0.f)
	{
		TravelToNextLevel();
	}
	else
	{
		World->GetTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { TravelToNextLevel(); }), Rules.ResultsSeconds, false);
	}
	return true;
}

bool UGSTN_LevelsSubsystem::TravelToNextLevel()
{
	UWorld* World = GetWorld();
	if (!World || !bRunning || !HasAuthority() || Phase != EGSTN_LevelPhase::Completed)
	{
		return false;
	}
	World->GetTimerManager().ClearTimer(TravelTimer);

	// A map the set does not list is a stand-alone level: nowhere to go
	if (!LevelSet || LevelIndex == INDEX_NONE)
	{
		return false;
	}
	const bool bListen = World->GetNetMode() == NM_ListenServer;
	FString URL;
	const int32 Next = LevelSet->GetNextIndex(LevelIndex);
	if (Next != INDEX_NONE)
	{
		URL = LevelSet->BuildTravelURL(Next, bListen);
	}
	else if (!LevelSet->ReturnMap.IsNull())
	{
		URL = LevelSet->ReturnMap.GetLongPackageName() + (bListen ? TEXT("?listen") : TEXT(""));
	}
	if (URL.IsEmpty())
	{
		return false;
	}

	// Seamless travel keeps the connections and the player controllers; the project needs a transition map for it (Project Settings > Maps & Modes)
	if (AGameModeBase* GameMode = World->GetAuthGameMode())
	{
		GameMode->bUseSeamlessTravel = UGSTN_LevelsRules::Get().bSeamlessTravel;
	}
	UE_LOG(LogGSTNLevels, Log, TEXT("Travel to %s"), *URL);
	World->ServerTravel(URL, false);
	return true;
}

FString UGSTN_LevelsSubsystem::Describe() const
{
	if (!bRunning)
	{
		return TEXT("Levels mode off (gstn.Levels.Enabled or the level's GameRules)");
	}
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	return FString::Printf(TEXT("Levels: level %d/%d phase %s | %.1f s | grace %s | collected %d (value %d) | friendly fire %s | players %d"),
		LevelIndex, LevelSet ? LevelSet->GetNumLevels() : 0, *UEnum::GetValueAsString(Phase), Now - StartTime,
		GraceEnd >= 0.0 ? *FString::Printf(TEXT("%.1f s left"), FMath::Max(0.0, GraceEnd - Now)) : TEXT("none"),
		Tally.TeamCollected, Tally.TeamCollectedValue, GetEffectiveFriendlyFire() ? TEXT("on") : TEXT("off"), PlayerCount);
}

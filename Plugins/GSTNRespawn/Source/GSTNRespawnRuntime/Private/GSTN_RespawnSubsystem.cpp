// Respawn, server side.

#include "GSTN_RespawnSubsystem.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_Checkpoint.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "GSTN_RespawnRules.h"
#include "GSTN_RespawnTags.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "TimerManager.h"
#include "World/GSTN_CombatWorldSubsystem.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNRespawnRuntime);

DEFINE_LOG_CATEGORY_STATIC(LogGSTNRespawn, Log, All);

// Debug window variables (owner rule 11-21: every setting also in GSTN Debug Options, one control each). The window lists gstn.Respawn.*; ints whose
// help names a value of 2 or more get a number box, the others a check box. They override the level's rules piece for playtests.
static TAutoConsoleVariable<int32> CVarRespawnEnabled(TEXT("gstn.Respawn.Enabled"), 0,
	TEXT("Lives and respawn (M12). 0 = as the level's GameRules (off without an asset), 1 = force off, 2 = force on with the rules' numbers. Takes effect at the next poll; turning on refills the pool."));
static TAutoConsoleVariable<int32> CVarRespawnLives(TEXT("gstn.Respawn.Lives"), -1,
	TEXT("Shared lives pool size. -1 = the rules' LivesPoolSize; e.g. 4 = four lives, 20 = twenty. Applied when the framework (re)starts or the team restarts."));
static TAutoConsoleVariable<float> CVarRespawnRegen(TEXT("gstn.Respawn.RegenSeconds"), -1.f,
	TEXT("Seconds per refilled life. -1 = the rules' LifeRegenSeconds, 0 = no refill."));
static TAutoConsoleVariable<float> CVarRespawnDelay(TEXT("gstn.Respawn.DelaySeconds"), -1.f,
	TEXT("Seconds between a KO and the return. -1 = the rules' RespawnDelaySeconds."));
static TAutoConsoleVariable<int32> CVarRespawnCheckpoints(TEXT("gstn.Respawn.Checkpoints"), 0,
	TEXT("Checkpoints. 0 = as the rules (bUseCheckpoints), 1 = force off, 2 = force on."));
static TAutoConsoleVariable<int32> CVarRespawnOnExhausted(TEXT("gstn.Respawn.OnLivesExhausted"), 0,
	TEXT("When the lives run out. 0 = as the rules, 1 = restart from the last checkpoint, 2 = restart the level (start, checkpoints forgotten), 3 = reload the map."));
static TAutoConsoleVariable<int32> CVarRespawnMethod(TEXT("gstn.Respawn.Method"), 0,
	TEXT("How a player comes back. 0 = as the rules, 1 = new pawn through the game mode, 2 = same pawn in place (harness style)."));
static TAutoConsoleVariable<bool> CVarRespawnDebug(TEXT("gstn.Respawn.Debug"), false,
	TEXT("On-screen line with the lives pool, the active checkpoint and the waiting players (server)."));

namespace
{
	/** The level's piece with the debug overrides applied, as a transient copy the subsystem owns */
	UGSTN_RespawnRules* MakeEffectiveRules(UObject* Outer)
	{
		// A copy built with the active piece as template (the class default object when no asset is active), never the piece itself: the
		// asset is shared and the overrides are this session's
		UGSTN_RespawnRules* Source = const_cast<UGSTN_RespawnRules*>(&UGSTN_RespawnRules::Get());
		UGSTN_RespawnRules* Rules = NewObject<UGSTN_RespawnRules>(Outer, Source->GetClass(), NAME_None, RF_Transient, Source);
		if (CVarRespawnLives.GetValueOnGameThread() >= 0)
		{
			Rules->LivesPoolSize = CVarRespawnLives.GetValueOnGameThread();
			Rules->MaxLivesWithExtras = FMath::Max(Rules->MaxLivesWithExtras, Rules->LivesPoolSize);
		}
		if (CVarRespawnRegen.GetValueOnGameThread() >= 0.f)
		{
			Rules->LifeRegenSeconds = CVarRespawnRegen.GetValueOnGameThread();
		}
		if (CVarRespawnDelay.GetValueOnGameThread() >= 0.f)
		{
			Rules->RespawnDelaySeconds = CVarRespawnDelay.GetValueOnGameThread();
		}
		switch (CVarRespawnCheckpoints.GetValueOnGameThread())
		{
		case 1: Rules->bUseCheckpoints = false; break;
		case 2: Rules->bUseCheckpoints = true; break;
		default: break;
		}
		switch (CVarRespawnOnExhausted.GetValueOnGameThread())
		{
		case 1: Rules->OnLivesExhausted = EGSTN_OnLivesExhausted::RestartFromCheckpoint; break;
		case 2: Rules->OnLivesExhausted = EGSTN_OnLivesExhausted::RestartLevel; Rules->bReloadMapOnRestartLevel = false; break;
		case 3: Rules->OnLivesExhausted = EGSTN_OnLivesExhausted::RestartLevel; Rules->bReloadMapOnRestartLevel = true; break;
		default: break;
		}
		switch (CVarRespawnMethod.GetValueOnGameThread())
		{
		case 1: Rules->Method = EGSTN_RespawnMethod::NewPawn; break;
		case 2: Rules->Method = EGSTN_RespawnMethod::InPlace; break;
		default: break;
		}
		return Rules;
	}

	bool ShouldRun()
	{
		switch (CVarRespawnEnabled.GetValueOnGameThread())
		{
		case 1: return false;
		case 2: return true;
		default: return UGSTN_RespawnRules::Get().bEnabled;
		}
	}

	/** A human player's controller (bots and minions have no lives) */
	AController* PlayerControllerOf(AActor* Actor)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		AController* Controller = Pawn ? Pawn->GetController() : nullptr;
		const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
		return Controller && Controller->IsPlayerController() && (!PlayerState || !PlayerState->IsABot()) ? Controller : nullptr;
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

	FAutoConsoleCommandWithWorldAndArgs CmdAddLives(TEXT("gstn.Respawn.AddLives"), TEXT("gstn.Respawn.AddLives N: extra lives for the team (server)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_RespawnSubsystem* Respawn = UGSTN_RespawnSubsystem::GetRespawnSubsystem(FindServerWorld(World)))
			{
				const int32 Added = Respawn->AddLives(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1);
				UE_LOG(LogGSTNRespawn, Display, TEXT("AddLives +%d -> %s"), Added, *Respawn->Describe());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdRestart(TEXT("gstn.Respawn.Restart"), TEXT("gstn.Respawn.Restart: the team restart now, as if the lives ran out (server)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_RespawnSubsystem* Respawn = UGSTN_RespawnSubsystem::GetRespawnSubsystem(FindServerWorld(World)))
			{
				Respawn->ForceTeamRestart();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdStatus(TEXT("gstn.Respawn.Status"), TEXT("gstn.Respawn.Status: one line of respawn state to the log (server)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_RespawnSubsystem* Respawn = UGSTN_RespawnSubsystem::GetRespawnSubsystem(FindServerWorld(World)))
			{
				UE_LOG(LogGSTNRespawn, Display, TEXT("%s"), *Respawn->Describe());
			}
		}));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// State actor

AGSTN_RespawnStateInfo::AGSTN_RespawnStateInfo()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
}

AGSTN_RespawnStateInfo* AGSTN_RespawnStateInfo::GetRespawnState(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AGSTN_RespawnStateInfo> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AGSTN_RespawnStateInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_RespawnStateInfo, Lives);
	DOREPLIFETIME(AGSTN_RespawnStateInfo, PoolSize);
	DOREPLIFETIME(AGSTN_RespawnStateInfo, SecondsToNextLife);
	DOREPLIFETIME(AGSTN_RespawnStateInfo, ActiveCheckpoint);
	DOREPLIFETIME(AGSTN_RespawnStateInfo, Restarts);
}

void AGSTN_RespawnStateInfo::OnRep_State()
{
	OnStateChanged.Broadcast();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Subsystem

UGSTN_RespawnSubsystem* UGSTN_RespawnSubsystem::GetRespawnSubsystem(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject && GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGSTN_RespawnSubsystem>() : nullptr;
}

bool UGSTN_RespawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_RespawnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The authoritative event stream: only the host publishes, so on a client this listener never fires
	if (UGSTN_SimEventSubsystem* Events = Collection.InitializeDependency<UGSTN_SimEventSubsystem>())
	{
		FGameplayTagContainer KO;
		KO.AddTag(GSTNCombatTags::Event_KO);
		TWeakObjectPtr<UGSTN_RespawnSubsystem> WeakThis(this);
		KOListener = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(KO), [WeakThis](const FGSTN_SimEvent& Event)
		{
			if (UGSTN_RespawnSubsystem* This = WeakThis.Get())
			{
				This->HandleKO(Event);
			}
		});
	}
}

void UGSTN_RespawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	// The poll runs even while the framework is off, so the debug window's switch takes effect in a running session
	const float Interval = 1.f / FMath::Clamp(UGSTN_RespawnRules::Get().PollHz, 1.f, 30.f);
	InWorld.GetTimerManager().SetTimer(PollTimer, FTimerDelegate::CreateUObject(this, &UGSTN_RespawnSubsystem::Poll), Interval, true);
	if (ShouldRun())
	{
		Start();
	}
}

void UGSTN_RespawnSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
		if (KOListener.IsValid())
		{
			if (UGSTN_SimEventSubsystem* Events = World->GetSubsystem<UGSTN_SimEventSubsystem>())
			{
				Events->StopListening(KOListener);
			}
		}
	}
	KOListener = FGSTN_SimEventListenerHandle();
	Super::Deinitialize();
}

bool UGSTN_RespawnSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UGSTN_RespawnSubsystem::Start()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	Effective = MakeEffectiveRules(this);
	Pool.Reset(*Effective);
	Progress.Clear();
	if (AGSTN_Checkpoint* Old = ActiveCheckpoint.Get())
	{
		Old->SetActive(false);
	}
	ActiveCheckpoint.Reset();
	Pending.Reset();
	RestartAt = -1.0;
	LastPollTime = World->GetTimeSeconds();
	if (!StateInfo.IsValid())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		StateInfo = World->SpawnActor<AGSTN_RespawnStateInfo>(Params);
	}
	bRunning = true;
	SyncState();
	UE_LOG(LogGSTNRespawn, Log, TEXT("Respawn on: %s"), *Describe());
}

void UGSTN_RespawnSubsystem::Stop()
{
	bRunning = false;
	Pending.Reset();
	RestartAt = -1.0;
	if (AGSTN_RespawnStateInfo* Info = StateInfo.Get())
	{
		Info->Destroy();
	}
	StateInfo.Reset();
	UE_LOG(LogGSTNRespawn, Log, TEXT("Respawn off"));
}

FString UGSTN_RespawnSubsystem::Describe() const
{
	if (!bRunning)
	{
		return TEXT("Respawn off (gstn.Respawn.Enabled or the level's GameRules)");
	}
	const AGSTN_Checkpoint* Active = ActiveCheckpoint.Get();
	return FString::Printf(TEXT("Lives %d/%d next %.1fs | checkpoint %s | waiting %d | restart %s | restarts %d"), Pool.Lives, Pool.PoolSize, Pool.SecondsToNextLife(),
		Active ? *Active->GetName() : TEXT("start"), Pending.Num(), RestartAt >= 0.0 ? TEXT("scheduled") : TEXT("-"), Restarts);
}

int32 UGSTN_RespawnSubsystem::AddLives(int32 Count)
{
	if (!bRunning)
	{
		return 0;
	}
	const int32 Added = Pool.Add(Count);
	if (Added > 0)
	{
		Publish(GSTNRespawnTags::Message_Respawn_PoolChanged, nullptr, nullptr, Pool.Lives);
		SyncState();
	}
	return Added;
}

void UGSTN_RespawnSubsystem::RegisterCheckpoint(AGSTN_Checkpoint* Checkpoint)
{
	if (Checkpoint)
	{
		Checkpoints.AddUnique(Checkpoint);
	}
}

void UGSTN_RespawnSubsystem::UnregisterCheckpoint(AGSTN_Checkpoint* Checkpoint)
{
	Checkpoints.Remove(Checkpoint);
}

bool UGSTN_RespawnSubsystem::ActivateCheckpoint(AGSTN_Checkpoint* Checkpoint, AActor* Player)
{
	if (!bRunning || !Checkpoint || !Effective || !Progress.TryActivate(Checkpoint->Order, Checkpoint->GetFName(), *Effective))
	{
		return false;
	}
	if (AGSTN_Checkpoint* Old = ActiveCheckpoint.Get())
	{
		Old->SetActive(false);
	}
	ActiveCheckpoint = Checkpoint;
	Checkpoint->SetActive(true);
	Publish(GSTNRespawnTags::Message_Respawn_CheckpointActivated, Player, Checkpoint, Checkpoint->Order);
	SyncState();
	UE_LOG(LogGSTNRespawn, Log, TEXT("Checkpoint %s (order %d) by %s"), *Checkpoint->GetName(), Checkpoint->Order, Player ? *Player->GetName() : TEXT("script"));
	return true;
}

bool UGSTN_RespawnSubsystem::IsPending(const AController* Controller) const
{
	return Pending.ContainsByPredicate([Controller](const FPendingRespawn& Entry) { return Entry.Controller.Get() == Controller; });
}

void UGSTN_RespawnSubsystem::HandleKO(const FGSTN_SimEvent& Event)
{
	// The credit partner's copy of a KO names the same victim: one KO, one life
	if (!bRunning || !HasAuthority() || !Event.EventType.MatchesTagExact(GSTNCombatTags::Event_KO) || Event.Tags.HasTagExact(GSTNCombatNPTags::EventHit_CreditPartner))
	{
		return;
	}
	AController* Controller = PlayerControllerOf(Event.Target.Get());
	if (!Controller || IsPending(Controller) || RestartAt >= 0.0 || !Effective)
	{
		return;
	}
	const FGSTN_KOReasonRespawn Reason = Effective->GetReason(Event.Tags);
	const EGSTN_LifeSpendResult Result = Pool.Spend(Reason.LifeCost);
	const double Now = GetWorld()->GetTimeSeconds();
	SafeGround.Remove(Controller);
	Publish(GSTNRespawnTags::Message_Respawn_Down, Event.Instigator.Get(), Event.Target.Get(), Pool.Lives);
	if (Reason.LifeCost > 0)
	{
		Publish(GSTNRespawnTags::Message_Respawn_PoolChanged, nullptr, nullptr, Pool.Lives);
	}
	if (Result == EGSTN_LifeSpendResult::TeamRestart)
	{
		RestartAt = Now + FMath::Max(0.f, Effective->RestartDelaySeconds);
	}
	else
	{
		FPendingRespawn Entry;
		Entry.Controller = Controller;
		Entry.Time = Now + (Reason.DelaySeconds >= 0.f ? Reason.DelaySeconds : FMath::Max(0.f, Effective->RespawnDelaySeconds));
		Pending.Add(Entry);
	}
	SyncState();
	UE_LOG(LogGSTNRespawn, Log, TEXT("KO %s cost %d -> %s"), *GetNameSafe(Event.Target.Get()), Reason.LifeCost, *Describe());
}

void UGSTN_RespawnSubsystem::Poll()
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
	const double Now = World->GetTimeSeconds();
	const float Delta = LastPollTime >= 0.0 ? float(Now - LastPollTime) : 0.f;
	LastPollTime = Now;
	if (RestartAt < 0.0 && Pool.Advance(Delta) > 0)
	{
		Publish(GSTNRespawnTags::Message_Respawn_PoolChanged, nullptr, nullptr, Pool.Lives);
		SyncState();
	}

	if (RestartAt >= 0.0 && Now >= RestartAt)
	{
		DoTeamRestart();
		return;
	}

	// Due respawns, in KO order; the Index spreads players that come back together over the checkpoint's points
	int32 Index = 0;
	for (int32 It = 0; It < Pending.Num();)
	{
		if (Now < Pending[It].Time)
		{
			++It;
			continue;
		}
		AController* Controller = Pending[It].Controller.Get();
		Pending.RemoveAt(It);
		if (Controller)
		{
			RespawnController(Controller, false, Index++);
		}
	}

	PollPlayers();

	if (LastStateSyncTime < 0.0 || Now - LastStateSyncTime >= 1.0)
	{
		SyncState();
	}
	if (CVarRespawnDebug.GetValueOnGameThread() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(GetUniqueID()), 0.5f, FColor::Yellow, Describe());
	}
}

void UGSTN_RespawnSubsystem::PollPlayers()
{
	UWorld* World = GetWorld();
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn || IsPending(Controller) || RestartAt >= 0.0)
		{
			continue;
		}
		// Server-side positions only (the host's simulated pawns), outside the simulation
		const FVector Location = Pawn->GetActorLocation();
		for (const TWeakObjectPtr<AGSTN_Checkpoint>& Weak : Checkpoints)
		{
			AGSTN_Checkpoint* Checkpoint = Weak.Get();
			if (Checkpoint && Checkpoint != ActiveCheckpoint.Get() && Checkpoint->ContainsLocation(Location))
			{
				ActivateCheckpoint(Checkpoint, Pawn);
			}
		}
		// Safe ground: walkable floor right below, outside every KO zone
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNRespawnSafeGround), false, Pawn);
		const float Probe = Pawn->GetSimpleCollisionHalfHeight() + 30.f;
		if (World->LineTraceSingleByChannel(Floor, Location, Location - FVector(0.f, 0.f, Probe), ECC_Visibility, Params)
			&& Floor.ImpactNormal.Z >= 0.7f && (!Combat || !Combat->IsInKOZone(Location)))
		{
			SafeGround.Add(Controller, FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Location));
		}
	}
}

bool UGSTN_RespawnSubsystem::FindSafeTeammate(const AController* Except, FTransform& OutWhere) const
{
	for (const TPair<TWeakObjectPtr<AController>, FTransform>& Pair : SafeGround)
	{
		const AController* Controller = Pair.Key.Get();
		if (Controller && Controller != Except && Controller->GetPawn() && !IsPending(Controller))
		{
			// Beside the teammate, not inside them
			OutWhere = Pair.Value;
			OutWhere.AddToTranslation(OutWhere.GetRotation().RotateVector(FVector(-120.f, 0.f, 0.f)));
			return true;
		}
	}
	return false;
}

FTransform UGSTN_RespawnSubsystem::ChooseTransform(AController* Controller, bool bTeamRestart, int32 Index, EGSTN_RespawnPlacement& OutPlacement) const
{
	FTransform Teammate;
	const bool bTeammate = !bTeamRestart && FindSafeTeammate(Controller, Teammate);
	OutPlacement = GSTNRespawnLogic::ChoosePlacement(*Effective, ActiveCheckpoint.IsValid(), bTeammate, bTeamRestart);
	switch (OutPlacement)
	{
	case EGSTN_RespawnPlacement::Checkpoint:
		return ActiveCheckpoint->GetSpawnTransform(Index);
	case EGSTN_RespawnPlacement::NearTeammate:
		return Teammate;
	default:
		break;
	}
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (AActor* Start = GameMode ? GameMode->FindPlayerStart(Controller) : nullptr)
	{
		FTransform Where = Start->GetActorTransform();
		Where.SetScale3D(FVector::OneVector);
		// Players restarting together on one start stand side by side
		Where.AddToTranslation(Where.GetRotation().RotateVector(FVector(0.f, Index * 120.f, 0.f)));
		return Where;
	}
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? Pawn->GetActorTransform() : FTransform::Identity;
}

void UGSTN_RespawnSubsystem::RespawnController(AController* Controller, bool bTeamRestart, int32 Index)
{
	UWorld* World = GetWorld();
	if (!Controller || !World || !Effective)
	{
		return;
	}
	EGSTN_RespawnPlacement Placement = EGSTN_RespawnPlacement::LevelStart;
	const FTransform Where = ChooseTransform(Controller, bTeamRestart, Index, Placement);
	APawn* Old = Controller->GetPawn();
	bool bDone = false;

	if (Effective->Method == EGSTN_RespawnMethod::InPlace && Old)
	{
		Old->TeleportTo(Where.GetLocation(), Where.Rotator(), false, true);
		if (UGSTN_CombatSimComponent* Sim = Old->FindComponentByClass<UGSTN_CombatSimComponent>())
		{
			Sim->ServerRespawn();
		}
		bDone = true;
	}
	else
	{
		AGameModeBase* GameMode = World->GetAuthGameMode();
		if (GameMode && GameMode->GetClass()->ImplementsInterface(UGSTN_PawnSpawnService::StaticClass()))
		{
			bDone = IGSTN_PawnSpawnService::Execute_RespawnPlayer(GameMode, Controller, Where);
		}
		if (!bDone && GameMode)
		{
			// A fresh pawn = a fresh Network Prediction entity (game-modes 2.5); the old one goes away after its KO presentation had RespawnDelaySeconds
			Controller->UnPossess();
			if (Old)
			{
				Old->Destroy();
			}
			GameMode->RestartPlayerAtTransform(Controller, Where);
			bDone = Controller->GetPawn() != nullptr;
		}
	}
	Publish(GSTNRespawnTags::Message_Respawn_Respawned, nullptr, Controller->GetPawn(), Index);
	UE_LOG(LogGSTNRespawn, Log, TEXT("Respawn %s at %s (%s, %s)"), *Controller->GetName(), *Where.GetLocation().ToCompactString(),
		*UEnum::GetValueAsString(Placement), bDone ? TEXT("ok") : TEXT("FAILED: no game mode or pawn"));
}

void UGSTN_RespawnSubsystem::ForceTeamRestart()
{
	if (bRunning)
	{
		DoTeamRestart();
	}
}

void UGSTN_RespawnSubsystem::DoTeamRestart()
{
	UWorld* World = GetWorld();
	RestartAt = -1.0;
	Pending.Reset();
	++Restarts;
	// Rules may have changed in the debug window since the start: a restart re-reads them
	Effective = MakeEffectiveRules(this);
	const bool bLevel = Effective->OnLivesExhausted == EGSTN_OnLivesExhausted::RestartLevel;
	Publish(bLevel ? GSTNRespawnTags::Message_Respawn_RestartLevel : GSTNRespawnTags::Message_Respawn_RestartFromCheckpoint, nullptr, nullptr, Restarts);
	if (bLevel && Effective->bReloadMapOnRestartLevel)
	{
		UE_LOG(LogGSTNRespawn, Log, TEXT("Lives out: reloading the map"));
		World->ServerTravel(TEXT("?restart"), false);
		return;
	}
	if (bLevel)
	{
		if (AGSTN_Checkpoint* Old = ActiveCheckpoint.Get())
		{
			Old->SetActive(false);
		}
		ActiveCheckpoint.Reset();
		Progress.Clear();
	}
	Pool.Reset(*Effective);
	SafeGround.Reset();
	int32 Index = 0;
	TArray<AController*> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Controller = It->Get())
		{
			Players.Add(Controller);
		}
	}
	for (AController* Controller : Players)
	{
		RespawnController(Controller, true, Index++);
	}
	Publish(GSTNRespawnTags::Message_Respawn_PoolChanged, nullptr, nullptr, Pool.Lives);
	SyncState();
	UE_LOG(LogGSTNRespawn, Log, TEXT("Team restart (%s): %s"), bLevel ? TEXT("level start") : TEXT("checkpoint"), *Describe());
}

void UGSTN_RespawnSubsystem::Publish(const FGameplayTag& Type, AActor* Instigator, AActor* Target, float Magnitude) const
{
	if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(GetWorld()))
	{
		FGSTN_SimEvent Event;
		Event.EventType = Type;
		Event.Tags.AddTag(Type);
		Event.Instigator = Instigator;
		Event.Target = Target;
		Event.Magnitude = Magnitude;
		Events->Broadcast(Event);
	}
}

void UGSTN_RespawnSubsystem::SyncState()
{
	AGSTN_RespawnStateInfo* Info = StateInfo.Get();
	if (!Info)
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		LastStateSyncTime = World->GetTimeSeconds();
	}
	Info->Lives = Pool.Lives;
	Info->PoolSize = Pool.PoolSize;
	Info->SecondsToNextLife = Pool.SecondsToNextLife();
	Info->ActiveCheckpoint = ActiveCheckpoint.Get();
	Info->Restarts = Restarts;
	Info->OnRep_State();
}

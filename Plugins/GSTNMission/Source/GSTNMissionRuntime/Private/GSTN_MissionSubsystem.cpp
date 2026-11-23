// Missions, server side.

#include "GSTN_MissionSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNMissionRuntime);

DEFINE_LOG_CATEGORY_STATIC(LogGSTNMission, Log, All);

// Debug window variables (owner rule 11-21: every setting also in GSTN Debug Options, one control each)
static TAutoConsoleVariable<int32> CVarMissionEnabled(TEXT("gstn.Mission.Enabled"), 0,
	TEXT("Missions (M12). 0 = as the level's GameRules (off without an asset), 1 = force off, 2 = force on with the rules' missions. Takes effect at the next poll; turning on starts the missions from zero."));
static TAutoConsoleVariable<bool> CVarMissionDebug(TEXT("gstn.Mission.Debug"), false,
	TEXT("On-screen lines with every mission and objective (server)."));

namespace
{
	bool ShouldRun()
	{
		switch (CVarMissionEnabled.GetValueOnGameThread())
		{
		case 1: return false;
		case 2: return true;
		default: return UGSTN_MissionRules::Get().bEnabled;
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

	FAutoConsoleCommandWithWorldAndArgs CmdStatus(TEXT("gstn.Mission.Status"), TEXT("gstn.Mission.Status: every mission and objective to the log (server)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_MissionSubsystem* Missions = UGSTN_MissionSubsystem::GetMissionSubsystem(FindServerWorld(World)))
			{
				UE_LOG(LogGSTNMission, Display, TEXT("%s"), *Missions->Describe());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdReset(TEXT("gstn.Mission.Reset"), TEXT("gstn.Mission.Reset: every mission from zero (server)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_MissionSubsystem* Missions = UGSTN_MissionSubsystem::GetMissionSubsystem(FindServerWorld(World)))
			{
				Missions->ResetMissions();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdComplete(TEXT("gstn.Mission.CompleteActive"), TEXT("gstn.Mission.CompleteActive: completes every active objective (server, debug)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_MissionSubsystem* Missions = UGSTN_MissionSubsystem::GetMissionSubsystem(FindServerWorld(World)))
			{
				Missions->CompleteActiveObjectives();
			}
		}));

	UGSTN_MissionSubsystem* MissionsOf(const UObject* Context)
	{
		return UGSTN_MissionSubsystem::GetMissionSubsystem(Context);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// State actor

AGSTN_MissionStateInfo::AGSTN_MissionStateInfo()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.f);
}

AGSTN_MissionStateInfo* AGSTN_MissionStateInfo::GetMissionState(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AGSTN_MissionStateInfo> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AGSTN_MissionStateInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_MissionStateInfo, Missions);
}

void AGSTN_MissionStateInfo::OnRep_Missions()
{
	OnStateChanged.Broadcast();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Zone and collectible

AGSTN_ObjectiveZone::AGSTN_ObjectiveZone()
{
	PrimaryActorTick.bCanEverTick = false;
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetBoxExtent(FVector(300.f, 300.f, 200.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetGenerateOverlapEvents(false);
	Area->SetHiddenInGame(true);
	Area->ShapeColor = FColor(0, 200, 255);
	RootComponent = Area;
}

bool AGSTN_ObjectiveZone::ContainsLocation(const FVector& Location) const
{
	if (!Area)
	{
		return false;
	}
	const FVector Local = Area->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AGSTN_ObjectiveZone::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_MissionSubsystem* Missions = MissionsOf(this))
	{
		Missions->RegisterZone(this);
	}
}

void AGSTN_ObjectiveZone::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGSTN_MissionSubsystem* Missions = MissionsOf(this))
	{
		Missions->UnregisterZone(this);
	}
	Super::EndPlay(Reason);
}

AGSTN_Collectible::AGSTN_Collectible()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	Area = CreateDefaultSubobject<USphereComponent>(TEXT("Area"));
	Area->SetSphereRadius(80.f);
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetGenerateOverlapEvents(false);
	Area->SetHiddenInGame(true);
	RootComponent = Area;
}

float AGSTN_Collectible::GetRadius() const
{
	return Area ? Area->GetScaledSphereRadius() : 0.f;
}

void AGSTN_Collectible::Take(AActor* Player)
{
	if (bTaken)
	{
		return;
	}
	bTaken = true;
	OnRep_Taken();
	if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(this))
	{
		FGSTN_SimEvent Event;
		Event.EventType = GSTNMissionTags::Message_Mission_Collected;
		Event.Tags = CollectibleTags;
		Event.Tags.AddTag(GSTNMissionTags::Message_Mission_Collected);
		Event.Instigator = Player;
		Event.Target = this;
		Event.Magnitude = Value;
		Events->Broadcast(Event);
	}
}

void AGSTN_Collectible::Restore()
{
	if (bTaken)
	{
		bTaken = false;
		OnRep_Taken();
	}
}

void AGSTN_Collectible::OnTakenChanged_Implementation(bool bIsTaken)
{
	SetActorHiddenInGame(bIsTaken);
}

void AGSTN_Collectible::OnRep_Taken()
{
	OnTakenChanged(bTaken);
}

void AGSTN_Collectible::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_MissionSubsystem* Missions = MissionsOf(this))
	{
		Missions->RegisterCollectible(this);
	}
}

void AGSTN_Collectible::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGSTN_MissionSubsystem* Missions = MissionsOf(this))
	{
		Missions->UnregisterCollectible(this);
	}
	Super::EndPlay(Reason);
}

void AGSTN_Collectible::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_Collectible, bTaken);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Subsystem

UGSTN_MissionSubsystem* UGSTN_MissionSubsystem::GetMissionSubsystem(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject && GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGSTN_MissionSubsystem>() : nullptr;
}

bool UGSTN_MissionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_MissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Every event of the authoritative stream (an empty query matches all): the objectives' own queries pick theirs. Host only publishes
	if (UGSTN_SimEventSubsystem* Events = Collection.InitializeDependency<UGSTN_SimEventSubsystem>())
	{
		TWeakObjectPtr<UGSTN_MissionSubsystem> WeakThis(this);
		Listener = Events->ListenNative(FGameplayTagQuery(), [WeakThis](const FGSTN_SimEvent& Event)
		{
			if (UGSTN_MissionSubsystem* This = WeakThis.Get())
			{
				This->HandleEvent(Event);
			}
		});
	}
}

void UGSTN_MissionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	const float Interval = 1.f / FMath::Clamp(UGSTN_MissionRules::Get().PollHz, 1.f, 30.f);
	InWorld.GetTimerManager().SetTimer(PollTimer, FTimerDelegate::CreateUObject(this, &UGSTN_MissionSubsystem::Poll), Interval, true);
	if (ShouldRun())
	{
		Start();
	}
}

void UGSTN_MissionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
		if (Listener.IsValid())
		{
			if (UGSTN_SimEventSubsystem* Events = World->GetSubsystem<UGSTN_SimEventSubsystem>())
			{
				Events->StopListening(Listener);
			}
		}
	}
	Listener = FGSTN_SimEventListenerHandle();
	Runs.Reset();
	Super::Deinitialize();
}

bool UGSTN_MissionSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

float UGSTN_MissionSubsystem::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void UGSTN_MissionSubsystem::Start()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	const UGSTN_MissionRules& Rules = UGSTN_MissionRules::Get();
	bMergeSameFrame = Rules.bMergeSameFrameEvents;
	ResetOn = Rules.ResetOnEvents;
	DownOn = Rules.DownEvents;
	UpOn = Rules.UpEvents;
	Downed.Reset();
	if (!StateInfo.IsValid())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		StateInfo = World->SpawnActor<AGSTN_MissionStateInfo>(Params);
	}
	bRunning = true;
	Runs.Reset();
	Loaded.Reset();
	for (const TSoftObjectPtr<UGSTN_MissionDefinition>& Soft : Rules.Missions)
	{
		if (UGSTN_MissionDefinition* Definition = Soft.LoadSynchronous())
		{
			StartMission(Definition);
		}
		else if (!Soft.IsNull())
		{
			UE_LOG(LogGSTNMission, Warning, TEXT("Mission %s not found"), *Soft.ToString());
		}
	}
	SyncState();
	UE_LOG(LogGSTNMission, Log, TEXT("Missions on: %s"), *Describe());
}

void UGSTN_MissionSubsystem::Stop()
{
	bRunning = false;
	Runs.Reset();
	Loaded.Reset();
	Queue.Reset();
	if (AGSTN_MissionStateInfo* Info = StateInfo.Get())
	{
		Info->Destroy();
	}
	StateInfo.Reset();
	UE_LOG(LogGSTNMission, Log, TEXT("Missions off"));
}

void UGSTN_MissionSubsystem::StartMission(UGSTN_MissionDefinition* Definition)
{
	if (!bRunning || !Definition || !HasAuthority())
	{
		return;
	}
	Loaded.AddUnique(Definition);
	const int32 Index = Runs.AddDefaulted();
	Runs[Index].Definition = Definition;
	TArray<FGSTN_MissionChange> Changes;
	Runs[Index].Tracker.Start(Definition, Now(), bMergeSameFrame, Changes);
	Apply(Index, Changes);
	SyncState();
}

void UGSTN_MissionSubsystem::ResetMissions()
{
	if (!bRunning)
	{
		return;
	}
	for (const TWeakObjectPtr<AGSTN_Collectible>& Weak : Collectibles)
	{
		AGSTN_Collectible* Collectible = Weak.Get();
		if (Collectible && Collectible->bRestoreOnReset)
		{
			Collectible->Restore();
		}
	}
	TArray<UGSTN_MissionDefinition*> Definitions;
	for (const FRun& Run : Runs)
	{
		Definitions.Add(Run.Definition);
	}
	Runs.Reset();
	for (UGSTN_MissionDefinition* Definition : Definitions)
	{
		StartMission(Definition);
	}
	SyncState();
	UE_LOG(LogGSTNMission, Log, TEXT("Missions reset: %s"), *Describe());
}

void UGSTN_MissionSubsystem::CompleteActiveObjectives()
{
	for (int32 Index = 0; Index < Runs.Num(); ++Index)
	{
		TArray<FGSTN_MissionChange> Changes;
		Runs[Index].Tracker.CompleteActive(Now(), Changes);
		Apply(Index, Changes);
	}
	SyncState();
}

void UGSTN_MissionSubsystem::HandleEvent(const FGSTN_SimEvent& Event)
{
	if (!bRunning || !HasAuthority())
	{
		return;
	}
	if (AActor* Target = Event.Target.Get())
	{
		if (DownOn.Num() > 0 && Event.EventType.MatchesAny(DownOn))
		{
			Downed.Add(Target);
		}
		else if (UpOn.Num() > 0 && Event.EventType.MatchesAny(UpOn))
		{
			Downed.Remove(Target);
		}
	}
	Queue.Add(Event);
	if (!bDraining)
	{
		DrainQueue();
	}
}

void UGSTN_MissionSubsystem::DrainQueue()
{
	bDraining = true;
	bool bChanged = false;
	// Our own Message.Mission.* events re-enter through the stream while we publish: they wait in the queue, so a tracker never runs inside itself.
	// A chain longer than 256 events in one go is a loop in the data (a mission counting its own messages): stop and say so
	int32 Budget = 256;
	while (Queue.Num() > 0 && Budget-- > 0)
	{
		const FGSTN_SimEvent Event = Queue[0];
		Queue.RemoveAt(0);
		if (ResetOn.Num() > 0 && Event.EventType.MatchesAny(ResetOn))
		{
			ResetMissions();
			bChanged = true;
			continue;
		}
		for (int32 Index = 0; Index < Runs.Num(); ++Index)
		{
			TArray<FGSTN_MissionChange> Changes;
			Runs[Index].Tracker.HandleEvent(Event, Now(), Changes);
			bChanged |= Changes.Num() > 0;
			Apply(Index, Changes);
		}
	}
	if (Queue.Num() > 0)
	{
		UE_LOG(LogGSTNMission, Warning, TEXT("Mission event loop: %d events dropped (does a mission count its own Message.Mission events?)"), Queue.Num());
		Queue.Reset();
	}
	bDraining = false;
	if (bChanged)
	{
		SyncState();
	}
}

void UGSTN_MissionSubsystem::Apply(int32 MissionIndex, const TArray<FGSTN_MissionChange>& Changes)
{
	if (Changes.Num() == 0 || !Runs.IsValidIndex(MissionIndex))
	{
		return;
	}
	UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(GetWorld());
	// Copies: what we publish re-enters HandleEvent, which may reset the runs
	const UGSTN_MissionDefinition* Definition = Runs[MissionIndex].Definition;
	const TArray<FGSTN_ObjectiveState> Objectives = Runs[MissionIndex].Tracker.GetObjectives();
	TArray<FGSTN_SimEvent> Out;
	for (const FGSTN_MissionChange& Change : Changes)
	{
		FGSTN_SimEvent& Event = Out.AddDefaulted_GetRef();
		Event.EventType = Change.EventType;
		Event.Tags.AddTag(Change.EventType);
		if (Definition && Definition->MissionTag.IsValid())
		{
			Event.Tags.AddTag(Definition->MissionTag);
		}
		if (Objectives.IsValidIndex(Change.Objective) && Objectives[Change.Objective].ObjectiveTag.IsValid())
		{
			Event.Tags.AddTag(Objectives[Change.Objective].ObjectiveTag);
		}
		Event.Magnitude = Change.Progress;
		UE_LOG(LogGSTNMission, Log, TEXT("%s %s obj %d progress %d"), *GetNameSafe(Definition), *Change.EventType.ToString(), Change.Objective, Change.Progress);
	}
	// Our own events wait in the queue while we publish, then run in order
	const bool bWasDraining = bDraining;
	bDraining = true;
	for (const FGSTN_SimEvent& Event : Out)
	{
		if (Events)
		{
			Events->Broadcast(Event);
		}
	}
	bDraining = bWasDraining;
	if (!bDraining && Queue.Num() > 0)
	{
		DrainQueue();
	}
}

void UGSTN_MissionSubsystem::Poll()
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

	FGSTN_ObjectivePollContext Context;
	Context.Now = Now();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn || Downed.Contains(Pawn))
		{
			continue;
		}
		// Server-side positions, outside the simulation; a knocked-out pawn waiting for its respawn is not alive (DownEvents)
		++Context.AlivePlayers;
		const FVector Location = Pawn->GetActorLocation();
		for (const TWeakObjectPtr<AGSTN_ObjectiveZone>& Weak : Zones)
		{
			const AGSTN_ObjectiveZone* Zone = Weak.Get();
			if (Zone && Zone->ZoneTag.IsValid() && Zone->ContainsLocation(Location))
			{
				Context.PlayersInZone.FindOrAdd(Zone->ZoneTag)++;
			}
		}
		for (const TWeakObjectPtr<AGSTN_Collectible>& Weak : Collectibles)
		{
			AGSTN_Collectible* Collectible = Weak.Get();
			if (Collectible && !Collectible->bTaken && FVector::DistSquared(Collectible->GetActorLocation(), Location) <= FMath::Square(Collectible->GetRadius()))
			{
				Collectible->Take(Pawn);
			}
		}
	}

	bool bChanged = false;
	for (int32 Index = 0; Index < Runs.Num(); ++Index)
	{
		TArray<FGSTN_MissionChange> Changes;
		Runs[Index].Tracker.Poll(Context, Changes);
		bChanged |= Changes.Num() > 0;
		Apply(Index, Changes);
	}
	if (bChanged)
	{
		SyncState();
	}
	if (CVarMissionDebug.GetValueOnGameThread() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(GetUniqueID()), 0.6f, FColor::Cyan, Describe());
	}
}

FString UGSTN_MissionSubsystem::Describe() const
{
	if (!bRunning)
	{
		return TEXT("Missions off (gstn.Mission.Enabled or the level's GameRules)");
	}
	FString Text = FString::Printf(TEXT("%d mission(s)"), Runs.Num());
	for (const FRun& Run : Runs)
	{
		Text += TEXT("\n") + Run.Tracker.Describe();
	}
	return Text;
}

void UGSTN_MissionSubsystem::SyncState()
{
	AGSTN_MissionStateInfo* Info = StateInfo.Get();
	if (!Info)
	{
		return;
	}
	Info->Missions.Reset();
	for (const FRun& Run : Runs)
	{
		FGSTN_MissionReplicated& Out = Info->Missions.AddDefaulted_GetRef();
		Out.Definition = Run.Definition;
		Out.MissionTag = Run.Definition ? Run.Definition->MissionTag : FGameplayTag();
		Out.State = Run.Tracker.GetState();
		Out.MissionEnd = Run.Tracker.GetMissionEnd();
		Out.Objectives = Run.Tracker.GetObjectives();
	}
	Info->OnRep_Missions();
}

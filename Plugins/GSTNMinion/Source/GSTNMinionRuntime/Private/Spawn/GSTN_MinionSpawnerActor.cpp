// Minion runtime (authority batch, M8 G9a).

#include "Spawn/GSTN_MinionSpawnerActor.h"

#include "Components/BoxComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_MinionWaveDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTNMinionCoreModule.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Spawn/GSTN_MinionSpawner.h"

AGSTN_MinionSpawnerActor::AGSTN_MinionSpawnerActor()
{
	// Ticks only while a wave runs (StartWave enables it, the clear disables it)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(TriggerBox);
	TriggerBox->InitBoxExtent(FVector(300.0, 300.0, 150.0));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetHiddenInGame(true);
}

bool AGSTN_MinionSpawnerActor::IsAuthorityWorld() const
{
	// A level actor that does not replicate has local role Authority on a client too, so the role says nothing: the net mode does
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void AGSTN_MinionSpawnerActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsAuthorityWorld() && bUseTrigger)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AGSTN_MinionSpawnerActor::HandleTriggerOverlap);
	}
	else
	{
		// A client or a script-started spawner has no use for overlap events
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (IsAuthorityWorld() || bPreloadOnClients)
	{
		BeginPreload();
	}
	if (IsAuthorityWorld() && bStartOnBeginPlay)
	{
		StartWave();
	}
}

void AGSTN_MinionSpawnerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bRunning = false;
	DefinitionsHandle.Reset();
	DependenciesHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGSTN_MinionSpawnerActor::BeginPreload()
{
	if (bPreloadStarted)
	{
		return;
	}
	bPreloadStarted = true;
	if (!Wave)
	{
		bPreloaded = true;
		return;
	}

	// Step 1: the definitions. Only then can the soft references they hold be listed
	TArray<FSoftObjectPath> Paths;
	Wave->GetDefinitionPaths(Paths);
	Paths.RemoveAll([](const FSoftObjectPath& Path) { return Path.ResolveObject() != nullptr; });
	if (Paths.IsEmpty())
	{
		RequestDependencies();
		return;
	}
	DefinitionsHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &AGSTN_MinionSpawnerActor::OnDefinitionsLoaded));
	if (!DefinitionsHandle.IsValid())
	{
		RequestDependencies();
	}
}

void AGSTN_MinionSpawnerActor::OnDefinitionsLoaded()
{
	if (!bPreloaded && !DependenciesHandle.IsValid())
	{
		RequestDependencies();
	}
}

void AGSTN_MinionSpawnerActor::RequestDependencies()
{
	if (!Wave || DependenciesHandle.IsValid())
	{
		bPreloaded = true;
		return;
	}
	TArray<FSoftObjectPath> Paths;
	Wave->GetAssetsToPreload(Paths, !IsAuthorityWorld());
	// What is in memory already is kept by its users (the subsystem's resolved types, the pooled actors); only the rest is requested
	Paths.RemoveAll([](const FSoftObjectPath& Path) { return Path.ResolveObject() != nullptr; });
	if (Paths.IsEmpty())
	{
		bPreloaded = true;
		return;
	}
	DependenciesHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &AGSTN_MinionSpawnerActor::OnDependenciesLoaded));
	if (!DependenciesHandle.IsValid())
	{
		bPreloaded = true;
	}
}

void AGSTN_MinionSpawnerActor::OnDependenciesLoaded()
{
	bPreloaded = true;
	UE_LOG(LogGSTNMinion, Verbose, TEXT("Spawner %s: wave assets preloaded"), *GetName());
}

void AGSTN_MinionSpawnerActor::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bTriggered || !bUseTrigger || !Cast<APawn>(OtherActor))
	{
		return;
	}
	bTriggered = true;
	StartWave();
}

bool AGSTN_MinionSpawnerActor::StartWave()
{
	if (!IsAuthorityWorld() || bRunning || !Wave || Wave->Entries.IsEmpty())
	{
		return false;
	}
	if (!UGSTN_MinionCombatSubsystem::GetActive(GetWorld()))
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Spawner %s: the minion subsystem is not active, the wave does not start"), *GetName());
		return false;
	}
	if (SpawnGroup == 0)
	{
		SpawnGroup = static_cast<int32>(GetUniqueID() & 0x7FFFFFFF);
	}
	BeginPreload();

	EntrySpawned.Init(0, Wave->Entries.Num());
	Tracked.Reset();
	Spawned.Reset();
	TotalToSpawn = Wave->GetTotalCount();
	TotalSpawned = 0;
	bSpawnPointsGathered = false;
	bAwaitingStart = true;
	bRunning = true;
	SetActorTickEnabled(true);
	return true;
}

void AGSTN_MinionSpawnerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateWave();
}

void AGSTN_MinionSpawnerActor::GatherSpawnPoints()
{
	bSpawnPointsGathered = true;
	SpawnPoints.Reset();
	UWorld* World = GetWorld();
	if (!World || !Wave)
	{
		return;
	}
	// Once per distinct tag, at the start: no search while the wave runs
	TArray<AActor*> Found;
	for (const FGSTN_MinionWaveEntry& Entry : Wave->Entries)
	{
		if (Entry.SpawnPointTag.IsNone() || SpawnPoints.Contains(Entry.SpawnPointTag))
		{
			continue;
		}
		Found.Reset();
		UGameplayStatics::GetAllActorsWithTag(World, Entry.SpawnPointTag, Found);
		TArray<FTransform>& Transforms = SpawnPoints.Add(Entry.SpawnPointTag);
		for (const AActor* Point : Found)
		{
			Transforms.Add(Point->GetActorTransform());
		}
		if (Transforms.IsEmpty())
		{
			UE_LOG(LogGSTNMinion, Warning, TEXT("Spawner %s: no actor carries the spawn point tag %s, its minions spawn at the spawner"), *GetName(), *Entry.SpawnPointTag.ToString());
		}
	}
}

void AGSTN_MinionSpawnerActor::SpawnEntry(int32 EntryIndex, UGSTN_MinionCombatSubsystem& Minions)
{
	const FGSTN_MinionWaveEntry& Entry = Wave->Entries[EntryIndex];
	// Normally in memory by now (the preload); a synchronous load here is only the fallback
	UGSTN_MinionDefinition* Definition = Entry.Definition.LoadSynchronous();
	if (!Definition)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Spawner %s: entry %d has no loadable definition, skipped"), *GetName(), EntryIndex);
		TotalSpawned += Entry.Count - EntrySpawned[EntryIndex];
		EntrySpawned[EntryIndex] = Entry.Count;
		return;
	}

	const TArray<FTransform>* Points = Entry.SpawnPointTag.IsNone() ? nullptr : SpawnPoints.Find(Entry.SpawnPointTag);
	const bool bHasPoints = Points && !Points->IsEmpty();
	const FTransform Fallback = GetActorTransform();
	const int32 NumPoints = bHasPoints ? Points->Num() : 1;

	FGSTN_MinionSpawnParams Params;
	Params.SpawnGroup = SpawnGroup;
	while (EntrySpawned[EntryIndex] < Entry.Count)
	{
		// Round robin over the points; the minions that share point P stand on a ring around it
		const int32 K = EntrySpawned[EntryIndex];
		const int32 PointIndex = K % NumPoints;
		const int32 SlotOnPoint = K / NumPoints;
		const int32 AtThisPoint = (Entry.Count - PointIndex + NumPoints - 1) / NumPoints;
		const FTransform& Base = bHasPoints ? (*Points)[PointIndex] : Fallback;
		FVector Location = Base.GetLocation();
		if (AtThisPoint > 1 && Entry.RingRadius > 0.f)
		{
			const double Angle = UE_DOUBLE_TWO_PI * SlotOnPoint / AtThisPoint;
			Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Entry.RingRadius;
		}
		const FRotator Facing(0.0, Base.Rotator().Yaw, 0.0);

		const FGSTN_MinionHandle Handle = UGSTN_MinionSpawner::Spawn(this, Definition, FTransform(Facing, Location), Params);
		if (Handle.IsNull())
		{
			// Store full or subsystem gone: the rest of the entry waits for the next update
			UE_LOG(LogGSTNMinion, Verbose, TEXT("Spawner %s: entry %d spawn refused at %d of %d"), *GetName(), EntryIndex, K, Entry.Count);
			return;
		}
		Tracked.Add(Handle);
		Spawned.Add(Handle);
		++EntrySpawned[EntryIndex];
		++TotalSpawned;
	}
}

void AGSTN_MinionSpawnerActor::UpdateWave()
{
	if (!bRunning || !Wave)
	{
		return;
	}
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	if (!Minions || !bPreloaded)
	{
		return;
	}
	if (bAwaitingStart)
	{
		// The wave's clock starts when its assets are ready, so the delays are not eaten by the load
		bAwaitingStart = false;
		StartFrame = FMath::Max(Minions->GetLastBatchFrame(), 0);
		GatherSpawnPoints();
	}

	const int32 Frame = Minions->GetLastBatchFrame();
	for (int32 EntryIndex = 0; EntryIndex < Wave->Entries.Num(); ++EntryIndex)
	{
		const FGSTN_MinionWaveEntry& Entry = Wave->Entries[EntryIndex];
		if (EntrySpawned[EntryIndex] < Entry.Count && Frame >= StartFrame + FMath::Max(Entry.DelayFrames, 0))
		{
			SpawnEntry(EntryIndex, *Minions);
		}
	}

	// Drop what is no longer alive (dying counts as gone), then look for the clear
	for (int32 Index = 0; Index < Tracked.Num();)
	{
		if (Minions->IsAlive(Tracked[Index]))
		{
			++Index;
		}
		else
		{
			Tracked.RemoveAtSwap(Index);
		}
	}
	if (TotalSpawned >= TotalToSpawn && Tracked.IsEmpty())
	{
		FinishWave(Frame);
	}
}

void AGSTN_MinionSpawnerActor::FinishWave(int32 Frame)
{
	bRunning = false;
	SetActorTickEnabled(false);
	UE_LOG(LogGSTNMinion, Verbose, TEXT("Spawner %s: wave cleared at frame %d"), *GetName(), Frame);
	OnWaveClearedNative.Broadcast(Frame);
	OnWaveCleared.Broadcast();
}

int32 AGSTN_MinionSpawnerActor::GetAliveCount() const
{
	const UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	int32 Count = 0;
	if (Minions)
	{
		for (const FGSTN_MinionHandle& Handle : Tracked)
		{
			Count += Minions->IsAlive(Handle) ? 1 : 0;
		}
	}
	return Count;
}

void AGSTN_MinionSpawnerActor::GetSpawnedHandles(TArray<FGSTN_MinionHandle>& Out) const
{
	const UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	if (!Minions)
	{
		return;
	}
	for (const FGSTN_MinionHandle& Handle : Spawned)
	{
		if (Minions->IsValidHandle(Handle))
		{
			Out.Add(Handle);
		}
	}
}

void AGSTN_MinionSpawnerActor::GetAliveHandles(TArray<FGSTN_MinionHandle>& Out) const
{
	const UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	if (!Minions)
	{
		return;
	}
	for (const FGSTN_MinionHandle& Handle : Tracked)
	{
		if (Minions->IsAlive(Handle))
		{
			Out.Add(Handle);
		}
	}
}

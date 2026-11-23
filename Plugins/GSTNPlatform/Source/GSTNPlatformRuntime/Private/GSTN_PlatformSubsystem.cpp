// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_PlatformSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GSTN_EnemyPlatformComponent.h"
#include "GSTN_PlatformSettings.h"
#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformSubsystem)

bool UGSTN_PlatformSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_PlatformSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UGSTN_PlatformSubsystem::HandleActorSpawned));
	}
}

void UGSTN_PlatformSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	for (const TWeakObjectPtr<UMoverComponent>& Mover : BoundMovers)
	{
		if (UMoverComponent* Component = Mover.Get())
		{
			Component->OnPreSimulationTick.RemoveDynamic(this, &UGSTN_PlatformSubsystem::HandleMoverPreSimulationTick);
		}
	}
	BoundMovers.Reset();
	Platforms.Reset();
	Super::Deinitialize();
}

void UGSTN_PlatformSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindAllMovers();
}

void UGSTN_PlatformSubsystem::RegisterPlatform(UGSTN_EnemyPlatformComponent* Platform)
{
	if (!Platform)
	{
		return;
	}
	const bool bFirst = Platforms.Num() == 0;
	Platforms.AddUnique(Platform);
	if (bFirst)
	{
		// Pawns that were there before the first platform (a platform spawned during play)
		BindAllMovers();
	}
}

void UGSTN_PlatformSubsystem::UnregisterPlatform(UGSTN_EnemyPlatformComponent* Platform)
{
	Platforms.RemoveAll([Platform](const TWeakObjectPtr<UGSTN_EnemyPlatformComponent>& Entry) { return !Entry.IsValid() || Entry.Get() == Platform; });
}

FGSTN_PlatformPoseContext UGSTN_PlatformSubsystem::MakePoseContext(const FMoverTimeStep& TimeStep)
{
	FGSTN_PlatformPoseContext Context;
	Context.StepMs = TimeStep.StepMs;
	Context.TimeMs = TimeStep.BaseSimTimeMs;
	Context.bFromSimulation = true;
	if (TimeStep.ServerFrame != INDEX_NONE)
	{
		Context.Frame = TimeStep.ServerFrame - 1;
	}
	else if (TimeStep.StepMs > 0.f && TimeStep.BaseSimTimeMs >= 0.0)
	{
		// Not a fixed tick: the frame number is the count of whole steps (still the same number on every machine for the same time)
		Context.Frame = FMath::FloorToInt32(TimeStep.BaseSimTimeMs / TimeStep.StepMs + 0.5) - 1;
	}
	return Context;
}

bool UGSTN_PlatformSubsystem::ApplyPosePoint(const FGSTN_PlatformPoseContext& Context)
{
	if (Context.Frame == INDEX_NONE)
	{
		return false;
	}
	if (const UWorld* World = GetWorld())
	{
		LastSimulationPoseRealSeconds = World->GetRealTimeSeconds();
	}
	if (bHasLastApplied && Context.Frame == LastAppliedFrame && Context.TimeMs == LastAppliedTimeMs)
	{
		return false;
	}
	bHasLastApplied = true;
	LastAppliedFrame = Context.Frame;
	LastAppliedTimeMs = Context.TimeMs;

	for (int32 Index = Platforms.Num() - 1; Index >= 0; --Index)
	{
		if (UGSTN_EnemyPlatformComponent* Platform = Platforms[Index].Get())
		{
			Platform->ApplyPosePoint(Context);
		}
		else
		{
			Platforms.RemoveAtSwap(Index);
		}
	}
	return true;
}

bool UGSTN_PlatformSubsystem::HasRecentSimulationPose() const
{
	const UWorld* World = GetWorld();
	return World && World->GetRealTimeSeconds() - LastSimulationPoseRealSeconds <= UGSTN_PlatformSettings::Get().PresentationFallbackSeconds;
}

void UGSTN_PlatformSubsystem::HandleMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	if (TimeStep.BaseSimTimeMs < 0.0 || Platforms.Num() == 0)
	{
		return;
	}
	ApplyPosePoint(MakePoseContext(TimeStep));
}

void UGSTN_PlatformSubsystem::BindMover(UMoverComponent* Mover)
{
	if (!Mover || BoundMovers.Contains(Mover))
	{
		return;
	}
	Mover->OnPreSimulationTick.AddUniqueDynamic(this, &UGSTN_PlatformSubsystem::HandleMoverPreSimulationTick);
	BoundMovers.Add(Mover);
}

void UGSTN_PlatformSubsystem::HandleActorSpawned(AActor* Actor)
{
	BindMoversOf(Actor);
}

void UGSTN_PlatformSubsystem::BindMoversOf(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	TInlineComponentArray<UMoverComponent*> Movers(Actor);
	for (UMoverComponent* Mover : Movers)
	{
		BindMover(Mover);
	}
}

void UGSTN_PlatformSubsystem::BindAllMovers()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			BindMoversOf(*It);
		}
	}
}

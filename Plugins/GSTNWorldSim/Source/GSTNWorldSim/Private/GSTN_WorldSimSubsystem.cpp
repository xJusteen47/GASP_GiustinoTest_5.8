// Rollback plan F0 spike: spawns the world sim entity, and its console commands.

#include "GSTN_WorldSimSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_WorldSimComponent.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNWorldSimSpawn, Log, All);

namespace GSTNWorldSim
{
	static bool bSpawn = false;
	static FAutoConsoleVariableRef CVarSpawn(TEXT("gstn.WorldSim.Spawn"), bSpawn,
		TEXT("Spawn the rollback plan F0 world sim entity on the authority at the next world begin play. Off by default."));

	/** Every live world sim of the process (PIE runs server and clients in one process), so the harness gets all of them in one log */
	static void ForEachWorldSim(TFunctionRef<void(UGSTN_WorldSimComponent&)> Visit)
	{
		for (TObjectIterator<UGSTN_WorldSimComponent> It; It; ++It)
		{
			if (!It->IsTemplate() && It->GetWorld() && It->GetWorld()->IsGameWorld())
			{
				Visit(**It);
			}
		}
	}

	static FAutoConsoleCommand ReportCommand(TEXT("gstn.WorldSim.Report"),
		TEXT("Log the timing of every world sim: ticks, re-simulated ticks, average cost of each, worst game frame."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			int32 Count = 0;
			ForEachWorldSim([&Count](UGSTN_WorldSimComponent& Sim)
			{
				UE_LOG(LogGSTNWorldSimSpawn, Display, TEXT("%s"), *Sim.DescribeStats());
				++Count;
			});
			if (Count == 0)
			{
				UE_LOG(LogGSTNWorldSimSpawn, Display, TEXT("WorldSim: none (set gstn.WorldSim.Spawn 1 before the world begins play)"));
			}
		}));

	static FAutoConsoleCommand ResetCommand(TEXT("gstn.WorldSim.ResetStats"),
		TEXT("Reset the timing of every world sim (start a measurement after changing gstn.WorldSim.Entities or .Traces)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachWorldSim([](UGSTN_WorldSimComponent& Sim) { Sim.ResetStats(); });
		}));
}

bool UGSTN_WorldSimSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGSTN_WorldSimSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!GSTNWorldSim::bSpawn || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	WorldSimActor = InWorld.SpawnActor<AGSTN_WorldSimActor>(Params);
	UE_LOG(LogGSTNWorldSimSpawn, Log, TEXT("WorldSim spawned in %s: %s"), *InWorld.GetName(), *GetNameSafe(WorldSimActor));
}

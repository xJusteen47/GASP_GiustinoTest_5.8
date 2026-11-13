// Platform runtime: console commands for tests and the PIE harness (not in Shipping).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GSTN_EnemyPlatformComponent.h"
#include "GSTN_PlatformTestFighter.h"
#include "GSTNPlatformCoreModule.h"
#include "HAL/IConsoleManager.h"

namespace GSTNPlatformDebug
{
	/** Spawns the scripted test fighter in the world the command runs in, on the authority only (it replicates to the clients) */
	static FAutoConsoleCommandWithWorldAndArgs CmdSpawnTestFighter(
		TEXT("gstn.Platform.SpawnTestFighter"),
		TEXT("gstn.Platform.SpawnTestFighter X Y Z [Yaw=0]: spawns the scripted test fighter (WallSlide flank, Stand top, 300 cm/s) on the server of this world"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || World->GetNetMode() == NM_Client || Args.Num() < 3)
			{
				UE_LOG(LogGSTNPlatform, Warning, TEXT("gstn.Platform.SpawnTestFighter: needs X Y Z and a server or standalone world"));
				return;
			}
			const FVector Location(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2]));
			const FRotator Rotation(0.0, Args.Num() > 3 ? FCString::Atod(*Args[3]) : 0.0, 0.0);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const AGSTN_PlatformTestFighter* Fighter = World->SpawnActor<AGSTN_PlatformTestFighter>(Location, Rotation, Params);
			UE_LOG(LogGSTNPlatform, Log, TEXT("Spawned %s at %s yaw %.1f"), Fighter ? *Fighter->GetName() : TEXT("nothing"), *Location.ToString(), Rotation.Yaw);
		}));

	/** Sets the shape mask of every platform of every game world (each machine gets the same input, as the parts bridge would give) */
	static FAutoConsoleCommand CmdSetMask(
		TEXT("gstn.Platform.SetMask"),
		TEXT("gstn.Platform.SetMask Mask [Inactive]: shapes allowed (bit per shape id) on every platform of every game world; the others go away with their Breaking telegraph, or at once with Inactive"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				return;
			}
			const uint8 Mask = uint8(FCString::Atoi(*Args[0]) & 0xFF);
			const EGSTN_PlatformShapeState DisableAs = Args.Num() > 1 && Args[1].Equals(TEXT("Inactive"), ESearchCase::IgnoreCase)
				? EGSTN_PlatformShapeState::Inactive : EGSTN_PlatformShapeState::Breaking;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (!World || !World->IsGameWorld())
				{
					continue;
				}
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					TInlineComponentArray<UGSTN_EnemyPlatformComponent*> Platforms(*It);
					for (UGSTN_EnemyPlatformComponent* Platform : Platforms)
					{
						Platform->SetShapeMask(Mask, DisableAs);
						UE_LOG(LogGSTNPlatform, Log, TEXT("%s: shape mask 0x%02x from frame %d"), *Platform->GetPathName(), Mask, Platform->GetLatestPoseFrame() + 1);
					}
				}
			}
		}));
}

#endif

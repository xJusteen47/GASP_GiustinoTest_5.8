// Dev command to drop a screw in front of the local player: playtests without editing a shared level.

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_SpinScrew.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
namespace GSTNSpinScrewCommands
{
	/**
	 * gstn.SpinScrew.Spawn [Distance=150] [Vanish=0]: on the authority (standalone, listen host or the server of PIE), puts a screw on the floor
	 * Distance cm in front of the first player's pawn. Vanish=1 makes it pop and vanish instead of going to the hand
	 */
	static FAutoConsoleCommandWithWorldAndArgs SpawnCommand(TEXT("gstn.SpinScrew.Spawn"),
		TEXT("Spawn a spin screw on the floor in front of the first player (authority). Args: [Distance=150] [Vanish=0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || World->GetNetMode() == NM_Client)
			{
				UE_LOG(LogTemp, Warning, TEXT("gstn.SpinScrew.Spawn: run it on the authority (server or listen host)"));
				return;
			}
			const APlayerController* Player = World->GetFirstPlayerController();
			const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
			if (!Pawn)
			{
				UE_LOG(LogTemp, Warning, TEXT("gstn.SpinScrew.Spawn: no player pawn"));
				return;
			}
			const float Distance = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 150.f;
			const bool bVanish = Args.Num() > 1 && FCString::Atoi(*Args[1]) != 0;
			const FVector Up = Pawn->GetActorUpVector();
			const FVector Forward = FVector::VectorPlaneProject(Pawn->GetActorForwardVector(), Up).GetSafeNormal();
			const FVector Above = Pawn->GetActorLocation() + Forward * Distance;
			FHitResult Floor;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTN_SpinScrewSpawn), false, Pawn);
			const FVector At = World->LineTraceSingleByChannel(Floor, Above, Above - Up * 1000.f, ECC_WorldStatic, Params) ? Floor.ImpactPoint : Above - Up * 90.f;
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AGSTN_SpinScrew* Screw = World->SpawnActor<AGSTN_SpinScrew>(AGSTN_SpinScrew::StaticClass(), At, FRotationMatrix::MakeFromZX(Up, Forward).Rotator(), SpawnParams);
			if (Screw)
			{
				Screw->Release = bVanish ? EGSTN_ScrewRelease::PopAndVanish : EGSTN_ScrewRelease::ToHandOrFloor;
				UE_LOG(LogTemp, Log, TEXT("gstn.SpinScrew.Spawn: %s at %s (%s)"), *Screw->GetName(), *At.ToCompactString(), bVanish ? TEXT("pop and vanish") : TEXT("to hand or floor"));
			}
		}));

	/** gstn.SpinScrew.Report: progress of every screw in the world */
	static FAutoConsoleCommandWithWorld ReportCommand(TEXT("gstn.SpinScrew.Report"), TEXT("Log every spin screw's progress"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (TActorIterator<AGSTN_SpinScrew> It(World); It; ++It)
			{
				UE_LOG(LogTemp, Log, TEXT("SpinScrew %s progress=%d/%d released=%d"), *It->GetName(), It->GetProgress(), It->StepsToRelease, It->IsReleased() ? 1 : 0);
			}
		}));
}
#endif

// Fill out your copyright notice in the Description page of Project Settings.

#include "GSTN_WallMovementDebug.h"

#if GSTN_WALL_DEBUG

#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_WallMovementLibrary.h"
#include "HAL/IConsoleManager.h"
#include "MoverComponent.h"

#include <atomic>

DEFINE_LOG_CATEGORY(LogGSTNWall);

namespace GSTNWallDebug
{
	static std::atomic<bool> bJumpRequested = false;
	static std::atomic<bool> bDetachHeld = false;

	/** Calls Fn on the Mover component of every player pawn in PIE/game worlds */
	static void ForEachPlayerMover(TFunctionRef<void(UMoverComponent&)> Fn)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* World = Ctx.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
				if (UMoverComponent* Mover = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr)
				{
					Fn(*Mover);
				}
			}
		}
	}

	static FAutoConsoleCommand CmdLaunch(TEXT("gstn.Wall.Launch"),
		TEXT("gstn.Wall.Launch X Y Z [Additive=0]: sets (or adds) the player pawn's velocity through Mover's ApplyVelocity effect"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 3)
			{
				return;
			}
			const FVector Velocity(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
			const bool bAdditive = Args.Num() > 3 && FCString::Atoi(*Args[3]) != 0;
			ForEachPlayerMover([&](UMoverComponent& Mover)
			{
				TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
				Effect->VelocityToApply = Velocity;
				Effect->bAdditiveVelocity = bAdditive;
				Mover.QueueInstantMovementEffect(Effect);
				UE_LOG(LogGSTNWall, Log, TEXT("Launch %s additive=%d"), *Velocity.ToCompactString(), bAdditive ? 1 : 0);
			});
		}));

	static FAutoConsoleCommand CmdJump(TEXT("gstn.Wall.Jump"),
		TEXT("Simulates one jump press for wall modes (wall jump)"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			bJumpRequested = true;
			UE_LOG(LogGSTNWall, Log, TEXT("Jump requested"));
		}));

	static FAutoConsoleCommand CmdDetach(TEXT("gstn.Wall.Detach"),
		TEXT("gstn.Wall.Detach 1|0: holds/releases the wall detach button. Needed because the GASP pawn re-applies its own crouch input every sim tick, overriding Crouch() calls"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			bDetachHeld = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			UE_LOG(LogGSTNWall, Log, TEXT("Detach button %d"), bDetachHeld ? 1 : 0);
		}));

	static FAutoConsoleCommand CmdResetMemory(TEXT("gstn.Wall.ResetMemory"),
		TEXT("Calls UGSTN_WallMovementLibrary::ResetWallMemory on the player pawn"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachPlayerMover([](UMoverComponent& Mover) { UGSTN_WallMovementLibrary::ResetWallMemory(&Mover); });
		}));

	bool ConsumeJumpRequest()
	{
		return bJumpRequested.exchange(false);
	}

	bool IsDetachHeld()
	{
		return bDetachHeld;
	}

	static TAutoConsoleVariable<int32> CVarLog(TEXT("gstn.Wall.Log"), 0,
		TEXT("Wall movement logging. 1 = one line per substep, 2 = also every probe sweep and pivot rotation"), ECVF_Cheat);

	static TAutoConsoleVariable<FString> CVarDebugInput(TEXT("gstn.Wall.DebugInput"), TEXT(""),
		TEXT("Overrides the wall input. \"X Y Z\" = world-space vector projected on the wall plane, ")
		TEXT("\"stick R F\" = camera-relative stick fed through the input mapping, empty = real input"), ECVF_Cheat);

	static FAutoConsoleCommand CmdEnter(TEXT("gstn.Wall.Enter"),
		TEXT("Queues a movement mode (default WallSliding) on every player pawn's Mover in PIE/game worlds"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const FName ModeName = Args.Num() > 0 ? FName(*Args[0]) : FName(TEXT("WallSliding"));
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				UWorld* World = Ctx.World();
				if (!World || !World->IsGameWorld())
				{
					continue;
				}
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
					if (UMoverComponent* Mover = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr)
					{
						Mover->QueueNextMode(ModeName);
						UE_LOG(LogGSTNWall, Log, TEXT("Queued mode %s on %s"), *ModeName.ToString(), *Pawn->GetName());
					}
				}
			}
		}));

	int32 GetLogLevel()
	{
		return CVarLog.GetValueOnAnyThread();
	}

	EInputOverride GetInputOverride(FVector& OutValue)
	{
		const FString Str = CVarDebugInput.GetValueOnAnyThread();
		TArray<FString> Parts;
		Str.ParseIntoArrayWS(Parts);

		if (Parts.Num() >= 1 && Parts[0].Equals(TEXT("stick"), ESearchCase::IgnoreCase))
		{
			OutValue = FVector(Parts.Num() > 1 ? FCString::Atof(*Parts[1]) : 0.f, Parts.Num() > 2 ? FCString::Atof(*Parts[2]) : 0.f, 0.f);
			return EInputOverride::Stick;
		}
		if (Parts.Num() >= 3)
		{
			OutValue = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			return EInputOverride::World;
		}
		return EInputOverride::None;
	}

	FString Vec(const FVector& V)
	{
		return FString::Printf(TEXT("(%.2f %.2f %.2f)"), V.X, V.Y, V.Z);
	}
}

#endif // GSTN_WALL_DEBUG

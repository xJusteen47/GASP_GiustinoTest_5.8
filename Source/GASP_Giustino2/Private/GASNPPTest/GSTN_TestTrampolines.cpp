#include "GASNPPTest/GSTN_TestTrampolines.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GSTN_Trampoline.h"
#include "Math/RotationMatrix.h"

namespace
{
	const FName TestTag(TEXT("GSTN_TestTrampoline"));
}

FString UGSTN_TrampolineTestLibrary::SpawnTestTrampoline(UObject* WorldContext, FVector Center, FVector Up, FVector Size, float Speed, FVector LocalDirection, bool bKeepTangential, bool bResetAirJumps, bool bResetCoyote)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return FString();
	}
	const FTransform Transform(FRotationMatrix::MakeFromZ(Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector)).ToQuat(), Center);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGSTN_Trampoline* Trampoline = World->SpawnActorDeferred<AGSTN_Trampoline>(AGSTN_Trampoline::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Trampoline)
	{
		return FString();
	}
	Trampoline->Size = Size;
	Trampoline->Launch.DirectionMode = EGSTN_SurfaceLaunchDirection::LocalDirection;
	Trampoline->Launch.LocalDirection = LocalDirection;
	Trampoline->Launch.SpeedMode = EGSTN_SurfaceLaunchSpeed::Fixed;
	Trampoline->Launch.Speed = Speed;
	Trampoline->Launch.bKeepTangentialVelocity = bKeepTangential;
	Trampoline->Launch.bResetAirJumps = bResetAirJumps;
	Trampoline->Launch.bResetCoyote = bResetCoyote;
	Trampoline->Tags.Add(TestTag);
	Trampoline->FinishSpawning(Transform);
	return Trampoline->GetName();
}

int32 UGSTN_TrampolineTestLibrary::ClearTestTrampolines(UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	int32 Count = 0;
	if (World)
	{
		for (TActorIterator<AGSTN_Trampoline> It(World); It; ++It)
		{
			if (It->ActorHasTag(TestTag))
			{
				It->Destroy();
				++Count;
			}
		}
	}
	return Count;
}

#include "GASNPPTest/GSTN_TestSurfaces.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "GASNPPTest/GSTN_TestCombatAbilities.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "Net/UnrealNetwork.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/StrongObjectPtr.h"

AGSTN_TestSurface::AGSTN_TestSurface()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetBoxExtent(HalfExtent);
	RootComponent = Box;
}

void AGSTN_TestSurface::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_TestSurface, SurfaceType);
	DOREPLIFETIME(AGSTN_TestSurface, HalfExtent);
	DOREPLIFETIME(AGSTN_TestSurface, bBroken);
}

void AGSTN_TestSurface::PostNetInit()
{
	Super::PostNetInit();
	ApplyShape();
}

FGSTN_SurfaceBehaviour AGSTN_TestSurface::GetSurfaceBehaviour_Implementation(const FGSTN_SurfaceQuery& Query) const
{
	FGSTN_SurfaceBehaviour Behaviour;
	Behaviour.SurfaceType = SurfaceType;
	return Behaviour;
}

void AGSTN_TestSurface::BreakSurface_Implementation()
{
	++BreakCalls;
	if (HasAuthority())
	{
		bBroken = true;
		ApplyShape();
	}
}

void AGSTN_TestSurface::OnRep_Broken()
{
	ApplyShape();
}

void AGSTN_TestSurface::ApplyShape()
{
	if (Box)
	{
		Box->SetBoxExtent(HalfExtent);
	}
	SetActorEnableCollision(!bBroken);
	SetActorHiddenInGame(bBroken);
}

FString UGSTN_ImpactTestLibrary::SpawnTestSurface(UObject* WorldContext, FVector Center, FVector HalfExtent, float YawDegrees, const FString& SurfaceLeaf)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return FString();
	}
	const FGameplayTag Type = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Surface.Type.%s"), *SurfaceLeaf)), false);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGSTN_TestSurface* Surface = World->SpawnActor<AGSTN_TestSurface>(AGSTN_TestSurface::StaticClass(), FTransform(FRotator(0.0, YawDegrees, 0.0), Center), Params);
	if (!Surface)
	{
		return FString();
	}
	Surface->SurfaceType = Type;
	Surface->HalfExtent = HalfExtent;
	Surface->ForceNetUpdate();
	if (UBoxComponent* Box = Cast<UBoxComponent>(Surface->GetRootComponent()))
	{
		Box->SetBoxExtent(HalfExtent);
	}
	return Surface->GetName();
}

int32 UGSTN_ImpactTestLibrary::ClearTestSurfaces(UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	int32 Count = 0;
	if (World)
	{
		for (TActorIterator<AGSTN_TestSurface> It(World); It; ++It)
		{
			It->Destroy();
			++Count;
		}
	}
	return Count;
}

FString UGSTN_ImpactTestLibrary::GetTestSurfaces(UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	FString Out;
	if (World)
	{
		for (TActorIterator<AGSTN_TestSurface> It(World); It; ++It)
		{
			Out += FString::Printf(TEXT("%s,%d,%d;"), *It->GetName(), It->bBroken ? 1 : 0, It->BreakCalls);
		}
	}
	return Out;
}

namespace
{
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> GTestImpactTable;
}

int32 UGSTN_ImpactTestLibrary::ConfigureImpactTable(const FString& Rows)
{
	GTestImpactTable.Reset();
	if (!Rows.IsEmpty())
	{
		UGSTN_SurfaceImpactTable* Table = NewObject<UGSTN_SurfaceImpactTable>(GetTransientPackage());
		TArray<FString> Lines;
		Rows.ParseIntoArray(Lines, TEXT(";"), true);
		for (const FString& Line : Lines)
		{
			TArray<FString> F;
			Line.ParseIntoArray(F, TEXT(","), false);
			if (F.Num() < 9)
			{
				continue;
			}
			FGSTN_SurfaceImpactRow Row;
			Row.Surface = FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Surface.Type.%s"), *F[0])), false);
			Row.DefaultOutcome = static_cast<EGSTN_ImpactOutcome>(FCString::Atoi(*F[1]));
			Row.MinImpactSpeed = FCString::Atof(*F[2]);
			Row.Restitution = FCString::Atof(*F[3]);
			Row.SplatFrames = FCString::Atoi(*F[4]);
			Row.ImpactDamageScale = FCString::Atof(*F[5]);
			Row.bForceDefault = FCString::Atoi(*F[6]) != 0;
			const int32 Mask = FCString::Atoi(*F[7]);
			Row.Allowed.bSplat = (Mask & 1) != 0;
			Row.Allowed.bBounce = (Mask & 2) != 0;
			Row.Allowed.bBreakSurface = (Mask & 4) != 0;
			Row.ResidualSpeedScale = FCString::Atof(*F[8]);
			Table->Rows.Add(Row);
		}
		Table->Compile();
		GTestImpactTable.Reset(Table);
	}

	int32 Count = 0;
	for (TObjectIterator<UGSTN_CombatSimComponent> It; It; ++It)
	{
		if (It->GetWorld() && !It->IsTemplate())
		{
			It->SetImpactTable(GTestImpactTable.Get());
			++Count;
		}
	}
	return Count;
}

float UGSTN_ImpactTestLibrary::SweepClearDistance(UObject* WorldContext, FVector Start, FVector Direction, float MaxDistance)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || Direction.IsNearlyZero())
	{
		return 0.f;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNImpactClear), false);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	for (TActorIterator<AGSTN_TestSurface> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	FHitResult Hit;
	const FVector Dir = Direction.GetSafeNormal();
	if (World->SweepSingleByChannel(Hit, Start, Start + Dir * MaxDistance, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, 88.f), Query))
	{
		return Hit.Distance;
	}
	return MaxDistance;
}

bool UGSTN_ImpactTestLibrary::HasFloorBelow(UObject* WorldContext, FVector Location, float Depth)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNImpactFloor), false);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	for (TActorIterator<AGSTN_TestSurface> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	FHitResult Hit;
	return World->SweepSingleByChannel(Hit, Location, Location - FVector(0.0, 0.0, Depth), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, 88.f), Query);
}

bool UGSTN_ImpactTestLibrary::SetHitImpactProfile(int32 Force, int32 ForbidMask, bool bForceIgnoresSurface, float SpeedScale)
{
	FGSTN_ImpactProfile Profile;
	Profile.Force = static_cast<EGSTN_ImpactOutcome>(FMath::Clamp(Force, 0, 3));
	if (ForbidMask & 1) { Profile.Forbid.Add(EGSTN_ImpactOutcome::Splat); }
	if (ForbidMask & 2) { Profile.Forbid.Add(EGSTN_ImpactOutcome::Bounce); }
	if (ForbidMask & 4) { Profile.Forbid.Add(EGSTN_ImpactOutcome::BreakSurface); }
	Profile.bForceIgnoresSurface = bForceIgnoresSurface;
	Profile.SpeedScale = SpeedScale;

	// The timeline is read from the class default object by every combat component of the process
	UGSTN_GA_TestHitMove* Move = GetMutableDefault<UGSTN_GA_TestHitMove>();
	for (TInstancedStruct<FGSTN_TimelineTrack>& Track : Move->Timeline.Tracks)
	{
		if (FGSTN_TimelineTrack_Hitbox* Hitbox = Track.GetMutablePtr<FGSTN_TimelineTrack_Hitbox>())
		{
			for (FGSTN_HitboxSpec& Spec : Hitbox->Hitboxes)
			{
				Spec.Data.Impact = Profile;
			}
		}
	}
	return true;
}

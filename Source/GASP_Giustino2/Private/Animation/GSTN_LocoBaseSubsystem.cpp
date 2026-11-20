#include "Animation/GSTN_LocoBaseSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLocoBase, Log, All);

namespace GSTNLocoBase
{
	static void OnBaseChanged(IConsoleVariable* Var);

	static TAutoConsoleVariable<int32> CVarLocoBase(
		TEXT("gstn.Loco.Base"), 0,
		TEXT("Main anim graph of the GASP Mover character. 0 = current base (SandboxCharacter_Mover_ABP with our nodes), ")
		TEXT("1 = clean GASP stock copy (ABP_GSTN_StockMover + CHT_GSTN_StockMover_PoseMatch). Set before Play."),
		FConsoleVariableDelegate::CreateStatic(&OnBaseChanged), ECVF_Default);

	static UClass* LoadBaseClass(const TCHAR* Path)
	{
		return LoadClass<UAnimInstance>(nullptr, Path);
	}

	static void OnBaseChanged(IConsoleVariable* /*Var*/)
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld())
			{
				if (UGSTN_LocoBaseSubsystem* Subsystem = World->GetSubsystem<UGSTN_LocoBaseSubsystem>())
				{
					Subsystem->ApplyToWorld();
				}
			}
		}
	}
}

int32 UGSTN_LocoBaseSubsystem::GetSelectedBase()
{
	return GSTNLocoBase::CVarLocoBase.GetValueOnGameThread() == 1 ? 1 : 0;
}

bool UGSTN_LocoBaseSubsystem::ApplyToMesh(USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return false;
	}
	UClass* Current = Mesh->GetAnimClass();
	if (!Current)
	{
		return false;
	}
	// Match by path first so the stock class is only loaded when a GASP Mover mesh exists
	const FString CurrentPath = Current->GetPathName();
	const bool bIsCurrentBase = CurrentPath == CurrentBaseClassPath;
	const bool bIsStockBase = CurrentPath == StockBaseClassPath;
	if (!bIsCurrentBase && !bIsStockBase)
	{
		return false;
	}
	const bool bWantStock = GetSelectedBase() == 1;
	if (bWantStock == bIsStockBase)
	{
		return false;
	}
	UClass* Target = GSTNLocoBase::LoadBaseClass(bWantStock ? StockBaseClassPath : CurrentBaseClassPath);
	if (!Target)
	{
		UE_LOG(LogGSTNLocoBase, Warning, TEXT("gstn.Loco.Base: cannot load %s, %s keeps %s"),
			bWantStock ? StockBaseClassPath : CurrentBaseClassPath, *GetNameSafe(Mesh->GetOwner()), *Current->GetName());
		return false;
	}
	Mesh->SetAnimInstanceClass(Target);
	UE_LOG(LogGSTNLocoBase, Log, TEXT("gstn.Loco.Base=%d: %s now runs %s"), GetSelectedBase(), *GetNameSafe(Mesh->GetOwner()), *Target->GetName());
	return true;
}

void UGSTN_LocoBaseSubsystem::ApplyToWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		HandleActorSpawned(*It);
	}
}

bool UGSTN_LocoBaseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGSTN_LocoBaseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UGSTN_LocoBaseSubsystem::HandleActorSpawned));
	ApplyToWorld();
}

void UGSTN_LocoBaseSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Super::Deinitialize();
}

void UGSTN_LocoBaseSubsystem::HandleActorSpawned(AActor* Actor)
{
	// Default 0 never loads or swaps anything: the current game is untouched unless someone opts in
	if (!Actor || (GetSelectedBase() == 0 && !IsValid(FindObject<UClass>(nullptr, StockBaseClassPath))))
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		ApplyToMesh(Mesh);
	}
}

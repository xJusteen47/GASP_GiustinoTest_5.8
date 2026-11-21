#include "Animation/GSTN_LocoBaseSubsystem.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLocoBase, Log, All);

namespace GSTNLocoBase
{
	static TAutoConsoleVariable<int32> CVarLocoBase(
		TEXT("gstn.Loco.Base"), 1,
		TEXT("Main anim graph of the GASP Mover characters. 0 = current base (SandboxCharacter_Mover_ABP / ABP_GSTN_Locomotion ")
		TEXT("with our nodes), 1 = clean GASP stock copy (ABP_GSTN_StockMover + CHT_GSTN_StockMover_PoseMatch). Live: applied on ")
		TEXT("the next frame; restart Play for a fair comparison (pose history, montages)."),
		ECVF_Default);

	// Generated class names of the current bases (compared by FName: no allocation in the per-frame check)
	static const FName SandboxMoverAbp(TEXT("SandboxCharacter_Mover_ABP_C"));
	static const FName GstnLocomotionAbp(TEXT("ABP_GSTN_Locomotion_C"));

	// Run start variants (runstart-v8c, 2026-11-21): the stock pattern for switching chooser rows from a setting is a chooser
	// filter column bound to an ABP variable filled every frame (Unreal Assistant). Only the clean base has the variable and the
	// column (leaf table "Run F Loco Transitions", Tools/AnimPipeline/cards/runstart/runstart_variant_rows_ue.py), so it is
	// written only while gstn.Loco.Base is 1. Divergence row in docs/notes/gasp-divergenze.md.
	static TAutoConsoleVariable<int32> CVarRunStartVariant(
		TEXT("gstn.RunStart.Variant"), 20,
		TEXT("Run start clip on the clean base (gstn.Loco.Base 1). 20 = generic w1 (owner 2026-11-21: new standard, default). 0 = v4r7 (approved), 1 = v4r8c (run loop sped up at ")
		TEXT("start), 2 = v4r8a (one high-knee pump), 3 = v4r8b (cartoon wheel), 4 = stock relaxed start, 5/6/7 = generic rev-up ")
		TEXT("g1/g2/g3, 8 = generic sprinter drive s1, 9 = sprinter s2 (longer drive), 20 = generic w1 (sprinter weight transfer, ")
		TEXT("frantic limbs from frame 1; 10-19 are the Handoff copies of 0-9, so new variants start at 20). Read by the chooser on the ")
		TEXT("next start."),
		ECVF_Default);
	static const FName RunStartVariantVar(TEXT("GSTN_RunStartVariant"));

	// Run start hand-off fix (runstart-phase-desktop, 2026-11-21): each run start row has a sibling at variant + 10 playing a copy
	// of the clip with the stock `phase` convention (the run loop's pose search reads it to pick the loop foot) and a short,
	// first-frame entry (Tools/AnimPipeline/cards/runstart/runstart_handoff_ue.py). The offset keeps the approved rows (v4r7)
	// untouched at 0. Divergence row 19 in docs/notes/gasp-divergenze.md.
	static TAutoConsoleVariable<int32> CVarRunStartHandoff(
		TEXT("gstn.RunStart.Handoff"), 1,
		TEXT("Run start hand-off fix on the clean base (gstn.Loco.Base 1). 0 = rows as they are, 1 = rows with ")
		TEXT("stock-phase clip copies, entry from frame 0 and short blend-in (default: the w1 standard was reviewed with it). Combines with gstn.RunStart.Variant."),
		ECVF_Default);
	static constexpr int32 RunStartHandoffOffset = 10;

	static void ApplyRunStartVariant(USkeletalMeshComponent* Mesh)
	{
		UAnimInstance* Anim = Mesh->GetAnimInstance();
		if (!Anim)
		{
			return;
		}
		FProperty* Prop = Anim->GetClass()->FindPropertyByName(RunStartVariantVar);
		const double Value = CVarRunStartVariant.GetValueOnGameThread()
			+ (CVarRunStartHandoff.GetValueOnGameThread() != 0 ? RunStartHandoffOffset : 0);
		if (FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
		{
			D->SetPropertyValue_InContainer(Anim, Value);
		}
		else if (FFloatProperty* F = CastField<FFloatProperty>(Prop))
		{
			F->SetPropertyValue_InContainer(Anim, static_cast<float>(Value));
		}
	}
}

int32 UGSTN_LocoBaseSubsystem::GetSelectedBase()
{
	return GSTNLocoBase::CVarLocoBase.GetValueOnGameThread() == 1 ? 1 : 0;
}

bool UGSTN_LocoBaseSubsystem::IsCurrentBaseClass(const UClass* AnimClass)
{
	return AnimClass && (AnimClass->GetFName() == GSTNLocoBase::SandboxMoverAbp || AnimClass->GetFName() == GSTNLocoBase::GstnLocomotionAbp);
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
	for (TActorIterator<APawn> It(&InWorld); It; ++It)
	{
		HandleActorSpawned(*It);
	}
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
	if (!Cast<APawn>(Actor))
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> ActorMeshes(Actor);
	for (USkeletalMeshComponent* Mesh : ActorMeshes)
	{
		Meshes.AddUnique(Mesh);
	}
}

void UGSTN_LocoBaseSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Nothing to do at 0 unless a mesh still has to be restored: the default game pays one cvar read per frame
	if (GetSelectedBase() == 0 && Originals.IsEmpty())
	{
		return;
	}
	for (int32 Index = Meshes.Num() - 1; Index >= 0; --Index)
	{
		USkeletalMeshComponent* Mesh = Meshes[Index].Get();
		if (!Mesh)
		{
			Originals.Remove(Meshes[Index]);
			Meshes.RemoveAtSwap(Index);
			continue;
		}
		ApplyToMesh(Mesh);
		GSTNLocoBase::ApplyRunStartVariant(Mesh);
	}
}

TStatId UGSTN_LocoBaseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_LocoBaseSubsystem, STATGROUP_Tickables);
}

UClass* UGSTN_LocoBaseSubsystem::GetStockClass()
{
	if (!StockClass.IsValid() && !bStockLoadFailed)
	{
		StockClass = LoadClass<UAnimInstance>(nullptr, StockBaseClassPath);
		bStockLoadFailed = !StockClass.IsValid();
		if (bStockLoadFailed)
		{
			UE_LOG(LogGSTNLocoBase, Warning, TEXT("gstn.Loco.Base: cannot load %s, meshes keep their class"), StockBaseClassPath);
		}
	}
	return StockClass.Get();
}

void UGSTN_LocoBaseSubsystem::ApplyToMesh(USkeletalMeshComponent* Mesh)
{
	UClass* Current = Mesh->GetAnimClass();
	if (GetSelectedBase() == 1)
	{
		if (!IsCurrentBaseClass(Current))
		{
			return;
		}
		UClass* Stock = GetStockClass();
		if (!Stock)
		{
			return;
		}
		Originals.Add(Mesh, Current);
		Mesh->SetAnimInstanceClass(Stock);
		UE_LOG(LogGSTNLocoBase, Log, TEXT("gstn.Loco.Base=1: %s %s -> %s"), *GetNameSafe(Mesh->GetOwner()), *Current->GetName(), *Stock->GetName());
		return;
	}
	TWeakObjectPtr<UClass> Original;
	if (Originals.RemoveAndCopyValue(Mesh, Original) && Current == StockClass.Get() && Original.IsValid())
	{
		Mesh->SetAnimInstanceClass(Original.Get());
		UE_LOG(LogGSTNLocoBase, Log, TEXT("gstn.Loco.Base=0: %s back to %s"), *GetNameSafe(Mesh->GetOwner()), *Original->GetName());
	}
}

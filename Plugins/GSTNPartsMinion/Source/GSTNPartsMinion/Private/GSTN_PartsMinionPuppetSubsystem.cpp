// Bridge between parts and minions (plan P16 sub-gate B).

#include "GSTN_PartsMinionPuppetSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GSTN_PartsMinionSubsystem.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Present/GSTN_MinionPresentationSubsystem.h"
#include "Present/GSTN_MinionPuppet.h"

namespace
{
	// Bench only: the mesh a part-bearing puppet gets when its definition gave it none. The bench definitions are transient objects of the
	// server's world and a client may resolve another instance (or none of the PuppetMesh the bench cvar set), so the scenario cannot rely on
	// gstn.Minion.Bench.PuppetMesh reaching every world's puppets before they spawn. Empty = off (a real definition's mesh is never replaced)
	TAutoConsoleVariable<FString> CVarBenchPuppetMesh(
		TEXT("gstn.PartsMinion.Bench.PuppetMesh"), TEXT(""),
		TEXT("Object path of a USkeletalMesh given to part-bearing puppets that have no mesh (bench: the null-mesh puppets have no bone to hide)."));
	TAutoConsoleVariable<bool> CVarBenchPuppetVerbose(
		TEXT("gstn.PartsMinion.Bench.PuppetVerbose"), false,
		TEXT("Log, once per puppet, the skeletal mesh a part-bearing puppet uses."));
}

bool UGSTN_PartsMinionPuppetSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Same rule as the minion presentation: nothing renders on a dedicated server
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UGSTN_PartsMinionPuppetSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_PartsMinionPuppetSubsystem::Tick(float DeltaTime)
{
	ApplyToPuppets();
}

TStatId UGSTN_PartsMinionPuppetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_PartsMinionPuppetSubsystem, STATGROUP_Tickables);
}

int32 UGSTN_PartsMinionPuppetSubsystem::SetPartBonesHidden(USkeletalMeshComponent& Mesh, const UGSTN_PartSetDefinition& Set, uint8 Mask, bool bHidden)
{
	int32 Missing = 0;
	const TConstArrayView<FGSTN_PartDefinition> Parts = Set.GetParts();
	for (int32 Index = 0; Index < Parts.Num() && Index < 8; ++Index)
	{
		if ((Mask & (1u << Index)) == 0)
		{
			continue;
		}
		for (const FName Bone : Parts[Index].Bones)
		{
			if (Mesh.GetBoneIndex(Bone) == INDEX_NONE)
			{
				++Missing;
				continue;
			}
			if (bHidden)
			{
				Mesh.HideBoneByName(Bone, EPhysBodyOp::PBO_None);
			}
			else
			{
				Mesh.UnHideBoneByName(Bone);
			}
		}
	}
	return Missing;
}

void UGSTN_PartsMinionPuppetSubsystem::ApplyToPuppets()
{
	UWorld* World = GetWorld();
	const UGSTN_MinionPresentationSubsystem* Presentation = UGSTN_MinionPresentationSubsystem::GetActive(World);
	UGSTN_PartsMinionSubsystem* Bridge = World ? World->GetSubsystem<UGSTN_PartsMinionSubsystem>() : nullptr;
	if (!Presentation || !Bridge)
	{
		return;
	}
	const bool bFromStore = Presentation->GetStats().Source == EGSTN_MinionPresentSource::Store;
	const UGSTN_MinionClientRegistry* Registry = bFromStore ? nullptr : World->GetSubsystem<UGSTN_MinionClientRegistry>();
	if (!bFromStore && !Registry)
	{
		return;
	}

	for (const TPair<int32, FGSTN_MinionPresentRecord>& Pair : Presentation->GetRecords())
	{
		const FGSTN_MinionPresentRecord& Record = Pair.Value;
		AGSTN_MinionPuppet* Puppet = Record.Puppet.Get();
		if (!Puppet)
		{
			continue;
		}
		const UGSTN_PartSetDefinition* Set = Bridge->GetPresentationSet(Record.Definition.Get());
		FApplied* Previous = Applied.IsEmpty() ? nullptr : Applied.Find(Puppet);
		if (!Set && !Previous)
		{
			// A type without parts on a puppet that never showed any: the common case, nothing to do
			continue;
		}

		uint8 Mask = 0;
		if (Set)
		{
			if (bFromStore)
			{
				// Host: the server row itself (there is no pack item to read on the authority)
				const FGSTN_PartsMinionRow* Row = Record.Slot != INDEX_NONE
					? Bridge->GetRow(FGSTN_MinionHandle(static_cast<uint16>(Record.Slot), Record.Generation)) : nullptr;
				Mask = Row ? Row->BrokenMask : 0;
			}
			else
			{
				Mask = Registry->GetExtByte(Record.Key);
			}
		}

		if (Previous && Previous->Key == Record.Key && Previous->Generation == Record.Generation && Previous->Mask == Mask && Previous->Set.Get() == Set)
		{
			continue;
		}

		if (Set)
		{
			// Bench only (cvar empty otherwise): a puppet with no mesh gets the configured one before its bones are looked up
			USkeletalMeshComponent* PuppetMesh = Puppet->GetMesh();
			const FString BenchMesh = PuppetMesh ? CVarBenchPuppetMesh.GetValueOnGameThread() : FString();
			if (PuppetMesh && !BenchMesh.IsEmpty() && !PuppetMesh->GetSkeletalMeshAsset())
			{
				if (USkeletalMesh* Loaded = LoadObject<USkeletalMesh>(nullptr, *BenchMesh))
				{
					PuppetMesh->SetSkeletalMeshAsset(Loaded);
				}
			}
			if (!Previous && CVarBenchPuppetVerbose.GetValueOnGameThread() && PuppetMesh)
			{
				UE_LOG(LogTemp, Log, TEXT("PartsMinionBench puppet %s mesh=%s head=%d"), *Puppet->GetName(),
					*GetNameSafe(PuppetMesh->GetSkeletalMeshAsset()), PuppetMesh->GetBoneIndex(FName(TEXT("head"))) != INDEX_NONE);
			}
		}

		// Show the previous minion's parts again first (a pooled puppet reused, or a restore), then hide the current mask. Snap, no blend
		USkeletalMeshComponent* Mesh = Puppet->GetMesh();
		if (Previous && Previous->Mask != 0)
		{
			const UGSTN_PartSetDefinition* OldSet = Previous->Set.Get();
			if (Mesh && OldSet)
			{
				SetPartBonesHidden(*Mesh, *OldSet, Previous->Mask, false);
			}
			--Stats.Hiding;
		}
		if (Set && Mask != 0)
		{
			if (Mesh)
			{
				Stats.MissingBones += SetPartBonesHidden(*Mesh, *Set, Mask, true);
			}
			++Stats.Hiding;
		}
		if (!Previous)
		{
			Previous = &Applied.Add(Puppet);
			++Stats.Tracked;
		}
		Previous->Key = Record.Key;
		Previous->Generation = Record.Generation;
		Previous->Mask = Mask;
		Previous->Set = Set;
		++Stats.Changes;
	}
}

uint8 UGSTN_PartsMinionPuppetSubsystem::GetAppliedMask(const AGSTN_MinionPuppet* Puppet) const
{
	const FApplied* Found = Puppet ? Applied.Find(Puppet) : nullptr;
	return Found ? Found->Mask : 0;
}

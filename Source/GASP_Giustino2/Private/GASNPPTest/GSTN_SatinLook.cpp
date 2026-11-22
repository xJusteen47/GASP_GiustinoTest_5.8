#include "GASNPPTest/GSTN_SatinLook.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GSTNSatinLook
{
	static int32 Enabled = 0;
	static FAutoConsoleVariableRef CVarEnabled(
		TEXT("gstn.Debug.SatinMetal"), Enabled,
		TEXT("1 = pawn meshes in Astro Bot satin metal (same colours; metallic/roughness from /Game/GSTN/Materials/MI_GSTN_Mannequin_Satin). 0 = original materials."));

	static const TCHAR* const SatinPath = TEXT("/Game/GSTN/Materials/MI_GSTN_Mannequin_Satin.MI_GSTN_Mannequin_Satin");
	static const FName Marker(TEXT("Primary Metallic"));
	/** New pawns are looked for this often while on */
	static constexpr float ScanInterval = 0.25f;
}

const TArray<FName>& UGSTN_SatinLookSubsystem::SatinParameters()
{
	static const TArray<FName> Names = { TEXT("Primary Metallic"), TEXT("Primary Roughness"), TEXT("Secondary Metallic"), TEXT("Secondary Roughness") };
	return Names;
}

bool UGSTN_SatinLookSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

TStatId UGSTN_SatinLookSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_SatinLookSubsystem, STATGROUP_Tickables);
}

void UGSTN_SatinLookSubsystem::Deinitialize()
{
	RestoreAll();
	Super::Deinitialize();
}

void UGSTN_SatinLookSubsystem::Tick(float DeltaTime)
{
	if (GSTNSatinLook::Enabled == 0)
	{
		if (Slots.Num() > 0)
		{
			RestoreAll();
		}
		SinceScan = GSTNSatinLook::ScanInterval;
		return;
	}
	SinceScan += DeltaTime;
	if (SinceScan >= GSTNSatinLook::ScanInterval)
	{
		SinceScan = 0.f;
		Apply();
	}
}

void UGSTN_SatinLookSubsystem::Apply()
{
	if (!bSatinTried)
	{
		bSatinTried = true;
		SatinAsset = LoadObject<UMaterialInterface>(nullptr, GSTNSatinLook::SatinPath);
	}
	if (!SatinAsset)
	{
		return;
	}
	Slots.RemoveAll([](const FSlot& S) { return !S.Mesh.IsValid(); });

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(*It);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
			{
				UMaterialInterface* Current = Mesh->GetMaterial(Index);
				float Unused = 0.f;
				if (!Current || !Current->GetScalarParameterValue(GSTNSatinLook::Marker, Unused))
				{
					continue;
				}
				FSlot* Known = Slots.FindByPredicate([Mesh, Index](const FSlot& S) { return S.Mesh.Get() == Mesh && S.Index == Index; });
				if (Known && Known->Satin.Get() == Current)
				{
					continue; // still ours
				}
				// New slot, or someone (a skin, a kit look) put another material there since: wrap that one, keeping its colours
				UMaterialInstanceDynamic* Satin = UMaterialInstanceDynamic::Create(Current, Mesh);
				for (const FName& Name : SatinParameters())
				{
					float Value = 0.f;
					if (SatinAsset->GetScalarParameterValue(Name, Value))
					{
						Satin->SetScalarParameterValue(Name, Value);
					}
				}
				Mesh->SetMaterial(Index, Satin);
				if (!Known)
				{
					Known = &Slots.AddDefaulted_GetRef();
					Known->Mesh = Mesh;
					Known->Index = Index;
				}
				Known->Original = Current;
				Known->Satin = Satin;
			}
		}
	}
}

void UGSTN_SatinLookSubsystem::RestoreAll()
{
	for (const FSlot& S : Slots)
	{
		UMeshComponent* Mesh = S.Mesh.Get();
		// Only a slot that still shows our instance: a material set since then is someone else's choice
		if (Mesh && S.Satin.IsValid() && Mesh->GetMaterial(S.Index) == S.Satin.Get())
		{
			Mesh->SetMaterial(S.Index, S.Original.Get());
		}
	}
	Slots.Reset();
}

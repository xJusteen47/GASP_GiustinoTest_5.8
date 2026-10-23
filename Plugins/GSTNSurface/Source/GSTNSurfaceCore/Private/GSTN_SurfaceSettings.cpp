// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceSettings.h"

#include "Engine/Engine.h"
#include "GSTNSurfaceCoreModule.h"
#include "HAL/PlatformTLS.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Misc/CoreMisc.h"

UGSTN_SurfaceSettings::UGSTN_SurfaceSettings()
{
	// Default is filled from Config/DefaultGSTNSurface.ini: native tags are not guaranteed to be registered while a CDO is built
}

const FGSTN_SurfaceTable& UGSTN_SurfaceSettings::GetTable() const
{
	if (!Table.AreMaterialsBaked() || !Table.AreTypesBaked())
	{
		if (!IsInGameThread())
		{
			// Baking writes the table, which another thread may be reading: only the game thread may do it
			ensureMsgf(false, TEXT("The surface table was read from a worker thread before it was baked"));
			static const FGSTN_SurfaceTable Unbaked;
			return Unbaked;
		}
		// A settings object is a class default object, never const in fact; the fallback is a cache fill
		const_cast<UGSTN_SurfaceSettings*>(this)->BakeLoadedOnly();
	}
	return Table;
}

void UGSTN_SurfaceSettings::BakeLoadedOnly()
{
	if (!bWarnedLazyBake)
	{
		bWarnedLazyBake = true;
		UE_LOG(LogGSTNSurface, Warning, TEXT("The surface table was used before it was baked (before engine init?). It holds the surface types and only the physical materials already in memory; it is baked fully once the engine has started"));
	}

	TMap<const UPhysicalMaterial*, FGSTN_SurfaceBehaviour> Entries;
	BakedMaterials.Reset();
	for (const TPair<TSoftObjectPtr<UPhysicalMaterial>, FGSTN_SurfaceBehaviour>& Entry : PhysicalMaterials)
	{
		// Get, not LoadSynchronous: this can run inside the simulation, which must not wait for the disk
		if (UPhysicalMaterial* Material = Entry.Key.Get())
		{
			BakedMaterials.Add(Material);
			Entries.Add(Material, Entry.Value);
		}
	}
	Table.BakeMaterials(Entries);
	if (!Table.AreTypesBaked())
	{
		Table.BakeTypesFromTagManager();
	}
}

void UGSTN_SurfaceSettings::Rebake()
{
	check(IsInGameThread());
	TMap<const UPhysicalMaterial*, FGSTN_SurfaceBehaviour> Entries;
	BakedMaterials.Reset();
	for (const TPair<TSoftObjectPtr<UPhysicalMaterial>, FGSTN_SurfaceBehaviour>& Entry : PhysicalMaterials)
	{
		if (Entry.Key.IsNull())
		{
			continue;
		}
		UPhysicalMaterial* Material = Entry.Key.LoadSynchronous();
		if (!Material)
		{
			UE_LOG(LogGSTNSurface, Warning, TEXT("Physical material %s of the surface table could not be loaded"),
				*Entry.Key.ToString());
			continue;
		}
		BakedMaterials.Add(Material);
		Entries.Add(Material, Entry.Value);
	}

	Table.BakeMaterials(Entries);
	Table.BakeTypesFromTagManager();

	// The indices are what sync states store, so in the running game they never change after this bake. The editor keeps rebaking:
	// a designer who adds a Surface.Type tag needs its index at once, and no match runs against a half-edited tag set
	if (!GIsEditor)
	{
		Table.Freeze();
	}
	UE_LOG(LogGSTNSurface, Log, TEXT("Surface table baked: %d materials, %d surface types, type checksum %08X%s"),
		Entries.Num(), Table.NumTypes(), Table.GetTypesChecksum(), Table.IsFrozen() ? TEXT(" (frozen)") : TEXT(""));
}

void UGSTN_SurfaceSettings::RebakeTypes()
{
	Table.BakeTypesFromTagManager();
}

void UGSTN_SurfaceSettings::InvalidateTable()
{
	Table.ResetMaterials();
	BakedMaterials.Reset();
}

#if WITH_EDITOR
void UGSTN_SurfaceSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	InvalidateTable();
	if (GEngine && GEngine->IsInitialized() && IsInGameThread())
	{
		Rebake(); // the editor loads at once, so the next lookup in PIE does not have to
	}
}
#endif

void UGSTN_SurfaceSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);
	InvalidateTable();
	if (GEngine && GEngine->IsInitialized() && IsInGameThread())
	{
		Rebake();
	}
}

void UGSTN_SurfaceSettings::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	Collector.AddReferencedObjects(CastChecked<UGSTN_SurfaceSettings>(InThis)->BakedMaterials);
}

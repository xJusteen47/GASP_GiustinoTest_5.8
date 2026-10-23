// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceSettings.h"

#include "GSTNSurfaceCoreModule.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UGSTN_SurfaceSettings::UGSTN_SurfaceSettings()
{
	// Default is filled from Config/DefaultGSTNSurface.ini: native tags are not guaranteed to be registered while a CDO is built
}

const FGSTN_SurfaceTable& UGSTN_SurfaceSettings::GetTable() const
{
	if (!Table.AreMaterialsBaked() || !Table.AreTypesBaked())
	{
		Rebake();
	}
	return Table;
}

void UGSTN_SurfaceSettings::Rebake() const
{
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
	UE_LOG(LogGSTNSurface, Verbose, TEXT("Surface table baked: %d materials, %d surface types"), Entries.Num(), Table.NumTypes());
}

void UGSTN_SurfaceSettings::RebakeTypes() const
{
	Table.BakeTypesFromTagManager();
}

void UGSTN_SurfaceSettings::InvalidateTable() const
{
	Table.Reset();
	BakedMaterials.Reset();
}

#if WITH_EDITOR
void UGSTN_SurfaceSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	InvalidateTable();
}
#endif

void UGSTN_SurfaceSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);
	InvalidateTable();
}

void UGSTN_SurfaceSettings::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	Collector.AddReferencedObjects(CastChecked<UGSTN_SurfaceSettings>(InThis)->BakedMaterials);
}

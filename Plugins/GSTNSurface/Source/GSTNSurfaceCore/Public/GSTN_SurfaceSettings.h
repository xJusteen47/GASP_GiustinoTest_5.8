// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_SurfaceTable.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceSettings.generated.h"

class UPhysicalMaterial;

/**
 * Settings of the surface plugin (Project Settings > Plugins > GSTN Surface): steps 3 and 4 of the resolution order
 * (traversal-surfaces.md §2.2). Defaults ship in the plugin's Config/DefaultGSTNSurface.ini; a project overrides them in its own
 * Config/DefaultGSTNSurface.ini.
 *
 * The simulation of every machine reads these values, so they must be equal on all machines of a match and never change during
 * one, like any other setting the simulation reads. The physical material table is baked into FGSTN_SurfaceTable after the
 * engine starts (and again when the settings or the tag tree change in the editor), never inside the simulation. A lookup before
 * that must not load anything: it bakes the surface types and only the materials that are already in memory, and warns once.
 * In the running game the surface type indices are frozen at the first bake (see FGSTN_SurfaceTable::Freeze); the editor rebakes.
 */
UCLASS(config = GSTNSurface, defaultconfig, meta = (DisplayName = "GSTN Surface"))
class GSTNSURFACECORE_API UGSTN_SurfaceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_SurfaceSettings();

	/** The settings of the running project */
	static const UGSTN_SurfaceSettings& Get() { return *GetDefault<UGSTN_SurfaceSettings>(); }

	/** For the module and the editor, which bake the table; the simulation only reads */
	static UGSTN_SurfaceSettings& GetMutable() { return *GetMutableDefault<UGSTN_SurfaceSettings>(); }

	/**
	 * What a surface nothing marks offers. Shipped as {WallSlide, WallClimb, Stand} with Surface.Type.Wall, so enabling the
	 * plugin changes no existing wall until a designer marks one
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FGSTN_SurfaceBehaviour Default;

	/** Surfaces by physical material, for whole families of meshes (all glass, all crates) without a component on each */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	TMap<TSoftObjectPtr<UPhysicalMaterial>, FGSTN_SurfaceBehaviour> PhysicalMaterials;

	/**
	 * The baked tables. Safe to read from any thread once baked (the module bakes them after engine init). Never loads an asset:
	 * if nothing baked them yet (a lookup before engine init) it bakes the types and the materials already in memory, logs a
	 * warning once, and only on the game thread; from another thread it returns an empty table and fails an ensure
	 */
	const FGSTN_SurfaceTable& GetTable() const;

	/** Loads the physical materials of the table and rebuilds both tables. Game thread only, outside the simulation */
	void Rebake();

	/** Rebuilds only the surface type indices (the tag tree changed); loads nothing. A frozen table refuses a different set */
	void RebakeTypes();

	/** Forgets the baked material entries (the settings changed); the next use bakes the loaded ones. Types are kept */
	void InvalidateTable();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private:
	/** The fallback of GetTable: types, and the materials that are already loaded. Never loads */
	void BakeLoadedOnly();

	/** Keeps the baked materials loaded (reported in AddReferencedObjects), so the table's pointer keys stay valid */
	TArray<TObjectPtr<UPhysicalMaterial>> BakedMaterials;

	FGSTN_SurfaceTable Table;

	bool bWarnedLazyBake = false;
};

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
 * engine starts (and again when the settings or the tag tree change in the editor), never inside the simulation; a lookup before
 * that bakes on first use as a fallback, which only costs a hitch.
 */
UCLASS(config = GSTNSurface, defaultconfig, meta = (DisplayName = "GSTN Surface"))
class GSTNSURFACECORE_API UGSTN_SurfaceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_SurfaceSettings();

	/** The settings of the running project */
	static const UGSTN_SurfaceSettings& Get() { return *GetDefault<UGSTN_SurfaceSettings>(); }

	/**
	 * What a surface nothing marks offers. Shipped as {WallSlide, WallClimb, Stand} with Surface.Type.Wall, so enabling the
	 * plugin changes no existing wall until a designer marks one
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FGSTN_SurfaceBehaviour Default;

	/** Surfaces by physical material, for whole families of meshes (all glass, all crates) without a component on each */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	TMap<TSoftObjectPtr<UPhysicalMaterial>, FGSTN_SurfaceBehaviour> PhysicalMaterials;

	/** The baked tables, baking on first use if nothing baked them yet */
	const FGSTN_SurfaceTable& GetTable() const;

	/** Loads the physical materials of the table and rebuilds both tables. Call outside the simulation */
	void Rebake() const;

	/** Rebuilds only the surface type indices (the tag tree changed); loads nothing */
	void RebakeTypes() const;

	/** Forgets the baked tables (the settings changed); the next use bakes again */
	void InvalidateTable() const;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private:
	/** Keeps the baked materials loaded (reported in AddReferencedObjects), so the table's pointer keys stay valid */
	mutable TArray<TObjectPtr<UPhysicalMaterial>> BakedMaterials;

	mutable FGSTN_SurfaceTable Table;
};

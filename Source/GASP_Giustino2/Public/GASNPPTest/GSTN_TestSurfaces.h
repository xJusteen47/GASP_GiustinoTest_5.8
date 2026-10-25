// GAS + NPP + Mover network test harness: surfaces and impact rules for the surface impact scenarios (combat-m4-hits 7b, M4 H11).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_SurfaceProvider.h"
#include "GSTN_TestSurfaces.generated.h"

class UBoxComponent;

/**
 * A box the scenarios spawn at run time (no map edit) that answers for its own surface: a provider of GSTNSurface with a configurable
 * Surface.Type. Replicated (spawned on the authority, the clients get a copy with the same type), so every machine's prediction collides
 * with the same wall. BreakSurface is the surface's Break(): on the authority it removes the collision and hides the box, and the
 * replicated flag does the same on the clients. Counts the calls, which the scenarios read.
 */
UCLASS()
class GASP_GIUSTINO2_API AGSTN_TestSurface : public AActor, public IGSTN_SurfaceProvider
{
	GENERATED_BODY()

public:
	AGSTN_TestSurface();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FGSTN_SurfaceBehaviour GetSurfaceBehaviour_Implementation(const FGSTN_SurfaceQuery& Query) const override;
	virtual void BreakSurface_Implementation() override;

	/** The Surface.Type this box answers with */
	UPROPERTY(Replicated)
	FGameplayTag SurfaceType;

	UPROPERTY(Replicated)
	FVector HalfExtent = FVector(50.0, 400.0, 600.0);

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;

	/** Calls of BreakSurface on this copy (the authority's is the one that counts) */
	int32 BreakCalls = 0;

	virtual void PostNetInit() override;

private:
	UFUNCTION()
	void OnRep_Broken();

	void ApplyShape();

	UPROPERTY()
	TObjectPtr<UBoxComponent> Box;
};

/** Static helpers the Python scenarios call; they work on every world of the process (the harness runs the server and the clients in one) */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_ImpactTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Spawns a surface box on the authority world of WorldContext (replicates to the clients). Returns its name, empty if it could not be spawned */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Impacts", meta = (WorldContext = "WorldContext"))
	static FString SpawnTestSurface(UObject* WorldContext, FVector Center, FVector HalfExtent, float YawDegrees, const FString& SurfaceLeaf);

	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Impacts", meta = (WorldContext = "WorldContext"))
	static int32 ClearTestSurfaces(UObject* WorldContext);

	/** "name,broken,breakCalls;..." of the test surfaces of WorldContext's world */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Impacts", meta = (WorldContext = "WorldContext"))
	static FString GetTestSurfaces(UObject* WorldContext);

	/**
	 * Builds the surface impact table from rows "Leaf,outcome,minSpeed,restitution,splatFrames,damageScale,force,allowedMask,residual;..."
	 * (outcome 0 none, 1 splat, 2 bounce, 3 break; force 0/1 = bForceDefault; allowedMask bit 0 splat, 1 bounce, 2 break) and hands it to every
	 * combat component of every world. An empty string removes the table. Returns the number of components that got it
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Impacts")
	static int32 ConfigureImpactTable(const FString& Rows);

	/**
	 * How far a pawn sized capsule can travel from Start along Direction before it is blocked (pawns and the test surfaces are ignored), at most
	 * MaxDistance. The scenarios use it to find a spot with a free run for a launched victim
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Impacts", meta = (WorldContext = "WorldContext"))
	static float SweepClearDistance(UObject* WorldContext, FVector Start, FVector Direction, float MaxDistance);

	/** True if the ground is within Depth below Location (a downward capsule sweep ignoring pawns and the test surfaces) */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Impacts", meta = (WorldContext = "WorldContext"))
	static bool HasFloorBelow(UObject* WorldContext, FVector Location, float Depth);

	/** Rewrites the impact profile of every hit of the test move (UGSTN_GA_TestHitMove): force outcome, forbid mask (bit 0 splat, 1 bounce, 2 break), flags */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Impacts")
	static bool SetHitImpactProfile(int32 Force, int32 ForbidMask, bool bForceIgnoresSurface, float SpeedScale);
};

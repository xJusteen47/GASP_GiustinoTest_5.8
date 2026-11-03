// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_KOZone.generated.h"

class UBoxComponent;

/** The bounds of a zone as the simulation reads them: an oriented box, copied at registration */
struct GSTNCOMBATNP_API FGSTN_KOZoneBounds
{
	FTransform Transform;
	FVector HalfExtent = FVector::ZeroVector;

	bool Contains(const FVector& Point) const
	{
		const FVector Local = Transform.InverseTransformPositionNoScale(Point);
		return FMath::Abs(Local.X) <= HalfExtent.X && FMath::Abs(Local.Y) <= HalfExtent.Y && FMath::Abs(Local.Z) <= HalfExtent.Z;
	}
};

/**
 * A box that knocks out whoever is inside it (combat-m4-hits 7, M4 H9): a blast zone, a pit. It registers its bounds with the combat world
 * subsystem when it begins play, and the combat tick of every combatant reads its simulated position at the end of the previous frame
 * against the registered zones, so the KO is a function of the simulation state and the static level, the same on every machine and every
 * re-simulation. The KO is the terminal action state with the KOReason Zone, and the authority publishes Combat.Event.KO once.
 *
 * A zone is static data: its transform and extent are read once, at registration, because the simulation must not depend on a
 * transform that moved between machines. Place it in the level (it exists on every machine like any level actor), or spawn the same zone on
 * every machine the same way (SpawnZone, for tests). It lives in the plugin: no project type is needed.
 */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API AGSTN_KOZone : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_KOZone();

	/** Half the size of the box in local space, cm. Changing it after BeginPlay has no effect on the simulation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KO Zone", meta = (ClampMin = "1", ForceUnits = "cm"))
	FVector HalfExtent = FVector(500.0, 500.0, 500.0);

	/** Spawns a zone in the world of WorldContext, centred on Center with HalfExtent, registered at once. Call it on every machine that simulates */
	UFUNCTION(BlueprintCallable, Category = "Combat|KO Zone", meta = (WorldContext = "WorldContext"))
	static AGSTN_KOZone* SpawnZone(UObject* WorldContext, FVector Center, FVector InHalfExtent);

	/** Removes every zone of the world (tests) */
	UFUNCTION(BlueprintCallable, Category = "Combat|KO Zone", meta = (WorldContext = "WorldContext"))
	static void DestroyAllZones(UObject* WorldContext);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "KO Zone")
	TObjectPtr<UBoxComponent> Box;
};

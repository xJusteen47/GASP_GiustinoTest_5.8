// Combat debug adapter (not in Shipping).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_DebugOptionsPad.generated.h"

class UStaticMeshComponent;

/**
 * A pad in a test level that opens the GSTN Debug Options editor window when the local player's pawn steps on it (owner 2026-11-16, like
 * GASP's level button in its base map). It works in a plain Play with no script: stepping on runs the console command gstn.Debug.OpenWindow,
 * which the editor registers (in a standalone game the command does not exist and nothing happens). Not simulation: a presentation hook.
 * A distance test on a slow tick instead of an overlap: the Mover/NP pawn is placed by the simulation and does not always fire overlaps.
 */
UCLASS(Blueprintable)
class AGSTN_DebugOptionsPad : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_DebugOptionsPad();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Debug Pad")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Horizontal reach from the pad's centre, cm */
	UPROPERTY(EditAnywhere, Category = "Debug Pad", meta = (ClampMin = "10", ForceUnits = "cm"))
	float Radius = 120.f;

	/** The console command the pad runs on a step (once per step: leave the pad to run it again) */
	UPROPERTY(EditAnywhere, Category = "Debug Pad")
	FString Command = TEXT("gstn.Debug.OpenWindow");

private:
	bool bOccupied = false;
};

// Loadout debug pad: a floor plate that opens the loadout screen for the player standing on it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_LoadoutMenuPad.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A plate on the floor, like the Game Animation Sample's debug pads and the hub's kit pads: a local player whose pawn steps on it gets the
 * loadout screen (bDebug: the bare debug list, UGSTN_LoadoutMenuSubsystem::OpenMenu). Stepping off and on again opens it again; B closes it.
 *
 * Local presentation only: nothing replicates and nothing touches the simulation. Every world (the host's and each client's) has its own
 * pad, as a level carries it, and each one only looks at that world's own local players, so on a listen server a client's pawn walking on
 * the host's copy opens nothing there. The pad checks distance every frame instead of using an overlap, so it needs no collision profile
 * and works with any pawn movement (Mover moves the capsule without overlap events in some modes).
 */
UCLASS(meta = (DisplayName = "Loadout debug pad"))
class GSTNLOADOUTMENU_API AGSTN_LoadoutMenuPad : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_LoadoutMenuPad();

	virtual void Tick(float DeltaSeconds) override;

	/** The bare debug list (true) or the normal loadout screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Pad")
	bool bDebug = true;

	/** A pawn whose feet are this close to the pad's centre (horizontally) stands on it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Pad", meta = (ClampMin = "10", ForceUnits = "cm"))
	float Radius = 120.f;

	/** And no higher above it than this (a jump over the pad does not open the screen) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Pad", meta = (ClampMin = "10", ForceUnits = "cm"))
	float MaxHeight = 150.f;

	/** The words on the label above the plate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Pad")
	FText Label;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Loadout|Pad")
	TObjectPtr<UStaticMeshComponent> Plate;

	UPROPERTY(VisibleAnywhere, Category = "Loadout|Pad")
	TObjectPtr<UTextRenderComponent> Text;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	/** The local players standing on the pad last frame: the screen opens on stepping on, not every frame */
	TSet<TWeakObjectPtr<const class ULocalPlayer>> Standing;
};

/**
 * Puts the rules' loadout debug pads (UGSTN_LoadoutMenuRules::Pads) in a game world when Play starts, so a test level gets them without
 * being saved with them. Game and PIE worlds only.
 */
UCLASS()
class GSTNLOADOUTMENU_API UGSTN_LoadoutMenuPadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};

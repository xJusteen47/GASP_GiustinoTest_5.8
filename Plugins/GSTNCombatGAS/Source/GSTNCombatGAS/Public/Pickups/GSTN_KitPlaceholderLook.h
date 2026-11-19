// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "GSTN_KitPlaceholderLook.generated.h"

class APawn;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Stand-in look of a kit until skins are applied (owner, 2026-11-06: "a cube behind the back is fine for now"). Every player
 * whose applied loadout has a kit and no skin carries a coloured box on the back (on the back bone of the mesh that draws the body, so
 * it follows the animation, GASP's visual override included); a kit pickup with no mesh of its own shows the
 * same box on the ground. The colour comes from the kit's name only, so it is the same on every machine and for the same kit on
 * the ground and on a back.
 *
 * Presentation only, outside the simulation: a 4 Hz scan of the game state's player states on every machine that draws (not on a
 * dedicated server). It reads the replicated loadout and adds a transient, collision-free component to the pawn; nothing replicates
 * and nothing in the simulation sees it. Turned off with gstn.Combat.KitPlaceholderLook 0. A kit with a skin keeps the skin's look.
 *
 * The same scan gives every player pawn without a UGSTN_SkinComponent an equipment-only one (no skin, no kit or weapon Visual), so the
 * attachment providers' pieces show: a worn backpack's Visual on the back, removed when it is dropped or swapped. Not turned off by
 * the cvar: it is the equipment's look, not the stand-in.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_KitPlaceholderLookSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Bright colour of a kit, from its name (stable across machines and runs) */
	static FLinearColor ColourForKit(const FPrimaryAssetId& Kit);

	/** Puts the engine cube with the kit's colour on Mesh (its own dynamic material). An invalid kit hides it */
	static void ApplyPlaceholder(UStaticMeshComponent* Mesh, const FPrimaryAssetId& Kit);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void Refresh();

	struct FMarker
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		FPrimaryAssetId Kit;
		/** The skeletal mesh the box rides (the one that draws the body); null when it sits on the root */
		TWeakObjectPtr<USkeletalMeshComponent> Body;
	};

	TMap<TWeakObjectPtr<APawn>, FMarker> Markers;
	FTimerHandle Timer;
};

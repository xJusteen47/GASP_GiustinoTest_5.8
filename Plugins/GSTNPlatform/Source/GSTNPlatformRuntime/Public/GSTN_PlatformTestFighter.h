// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_PlatformSpec.h"
#include "GSTN_PlatformTestFighter.generated.h"

class UGSTN_ScriptedPlatformComponent;

/**
 * The scripted test fighter of the traversal scenarios (plan T3, scenario_surface_wallslide_enemy): an enemy on rails with a
 * wall-slideable flank and a top to stand on, moving back and forth at 300 cm/s. Test content: it has no mesh (no content
 * reference from C++); turn on gstn.Platform.Draw 1 to see its shapes. Spawn it in a running game with
 * gstn.Platform.SpawnTestFighter X Y Z [Yaw] (on the server; it replicates).
 *
 * Shapes, in its frame (the actor's location is on the floor, centre of the body):
 *  - 0 flank: box 50 x 800 x 500 cm whose -X face (x = -175) is the wall: WallSlide only (so the surface-driven wall entry has one
 *    choice), bottom 50 cm above the floor;
 *  - 1 top: box 300 x 800 x 30 cm whose upper face (z = 580) is the floor to stand on: Stand.
 * The path runs along its local Y (along the flank's face), +-300 cm at 300 cm/s: shorter than the half width of the shapes, so a
 * character sent at where the enemy is now still meets a shape after a second of flight. The spec is built in code on every machine
 * (PreRegisterAllComponents), so it is identical everywhere without being replicated.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "GSTN Platform Test Fighter"))
class GSTNPLATFORMRUNTIME_API AGSTN_PlatformTestFighter : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_PlatformTestFighter();

	/** The spec described above */
	static FGSTN_PlatformSpec MakeTestFighterSpec();

	UGSTN_ScriptedPlatformComponent* GetPlatform() const { return Platform; }

	virtual void PreRegisterAllComponents() override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Platform")
	TObjectPtr<UGSTN_ScriptedPlatformComponent> Platform;
};

// Steering arrows of a move's startup (presentation only, owner 2026-11-14).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_SteerIndicatorComponent.generated.h"

/**
 * v0 of the attack aiming indicators (handoffs/attack-aiming-research.md §3): on the ground at the feet of the locally controlled pawn,
 *  - arrow A, thin: where the move's stick points (FGSTN_SteerWindow::Source picks the stick),
 *  - arrow B, thick: where the move really goes (the facing, which the cap and the rate hold back),
 *  - the allowed arc: MaxSteerDegrees either side of the move origin, while the window is open.
 * When the direction locks, arrow B flashes white for LockFlashFrames and the drawing ends. Reads the combat sim component only
 * (GetSteerView): nothing here feeds the simulation, so it can be on for one player and off for another.
 *
 * Shown for the moves whose data says so (FGSTN_SteerWindow::bShowIndicator) while bEnabled; the console variable
 * GSTN.Combat.SteerIndicator overrides both (0 off, 1 every steerable move, -1 the data). Not a project setting. Placeholder look (debug
 * lines, absent from Shipping) until real decals and the body cues of the research (head, torso, glint) replace it.
 * UGSTN_CombatSimComponent adds one to its owner at BeginPlay (bAddSteerIndicator).
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_SteerIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_SteerIndicatorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Per pawn: off hides the arrows of every move of this pawn (the console variable still forces them on) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Steer")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Steer", meta = (ClampMin = "20", ForceUnits = "cm"))
	float ArrowLength = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Steer", meta = (ClampMin = "20", ForceUnits = "cm"))
	float ArcRadius = 120.f;

	/** How long arrow B flashes white when the direction locks, in simulation frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Steer", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 LockFlashFrames = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Steer")
	FColor StickColor = FColor(80, 200, 255);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Steer")
	FColor FacingColor = FColor(255, 170, 40);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Steer")
	FColor ArcColor = FColor(200, 200, 200);

	/** Whether the arrows show for a move: the console variable when it is 0 or 1, else bEnabled and the move's data. Pure, for the tests */
	static bool ShouldShow(int32 CVarValue, bool bComponentEnabled, bool bMoveShows);

private:
	/** Simulation frame the flash ends at (exclusive), INDEX_NONE when none runs; the facing yaw it shows */
	int32 FlashUntilFrame = INDEX_NONE;
	float FlashYawDeg = 0.f;
	/** Whether the previous tick drew the open window: its end starts the flash */
	bool bWasSteering = false;
	int32 LastSeenFrame = INDEX_NONE;
};

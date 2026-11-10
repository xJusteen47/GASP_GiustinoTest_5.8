// The precision reticle and the aim arrow (plan precisione-weakpoint 4, slice 3). Presentation only.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "GSTN_PrecisionAimComponent.generated.h"

class UGSTN_CombatSimComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UGSTN_MatchModeSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * What a match says its mode is (a Combat.Mode.* tag: PvE, TDM, Duel...). Implemented by the game state of a game that has modes; the precision
 * reticle asks it whether it may exist (UGSTN_CombatSettings::PrecisionReticleModes). Without one the settings' FallbackMatchMode applies
 */
class GSTNCOMBATNP_API IGSTN_MatchModeSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Precision")
	FGameplayTag GetMatchMode() const;
};

/**
 * The precision tools a player sees (plan precisione-weakpoint 4): a reticle the right stick click turns on and off, and an arrow at the feet
 * pointing where the aim stick aims, shown with or without the reticle. Local only: never replicated, inert on a dedicated server and on a pawn
 * this machine does not control, never read by the simulation. The reticle toggle is a local button (UGSTN_InputConfig::ReticleToggleAction),
 * not a command: what a move does with the aim reaches the simulation through the predicted command (the aim stick, the camera aim), never
 * through this component.
 *
 * The reticle exists only in the match modes UGSTN_CombatSettings::PrecisionReticleModes lists (PvE and TDM by default; Duel and any mode not
 * listed: no). The arrow is not gated: steering exists in every mode, the arrow only shows it.
 *
 * UGSTN_CombatSimComponent adds one to its owner at BeginPlay (bAddPrecisionAim), so nothing has to be placed. Placeholder look (debug lines,
 * absent from Shipping) until the owner's art pass.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_PrecisionAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_PrecisionAimComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True while the reticle is on and the match mode allows it, or while an aiming move holds it on (SetHoldAimShown) */
	UFUNCTION(BlueprintPure, Category = "Combat|Precision")
	bool IsReticleShown() const { return (bReticleOn && bModeAllowsReticle) || bHoldAimShown; }

	/**
	 * Shows the reticle while a held button aims (M10: the strong button held with an object in hand aims the throw along the camera), in every
	 * match mode, without touching the toggle. Local presentation: the move that sets it clears it when it ends. It also sets the command's
	 * bPrecisionAim, so a simulation that reads it sees the same frames the player saw the reticle
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Precision")
	void SetHoldAimShown(bool bShown) { bHoldAimShown = bShown; }

	/** Turns the reticle on or off as the button does (ignored, and off, where the mode has no reticle) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Precision")
	void SetReticleOn(bool bOn);

	/** The mode the reticle gate reads this frame: the game state's (IGSTN_MatchModeSource), else the settings' fallback */
	UFUNCTION(BlueprintPure, Category = "Combat|Precision")
	FGameplayTag ResolveMatchMode() const;

	/** Whether Mode lists among UGSTN_CombatSettings::PrecisionReticleModes. A mode not listed (or none) has no reticle */
	static bool IsReticleAllowedIn(const FGameplayTag& Mode);

	/** World yaw of the aim stick (+Y up) seen from a camera at CameraYawDeg: the direction the arrow shows */
	static float AimStickWorldYaw(const FVector2D& AimStick, float CameraYawDeg);

	/** How far the reticle looks along the camera for the point it marks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Precision", meta = (ClampMin = "100", ForceUnits = "cm"))
	float ReticleTraceDistance = 5000.f;

	/** Size of the reticle mark at the aimed point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Precision", meta = (ClampMin = "1", ForceUnits = "cm"))
	float ReticleSize = 12.f;

	/** Length of the aim arrow at the feet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Precision", meta = (ClampMin = "10", ForceUnits = "cm"))
	float ArrowLength = 120.f;

private:
	bool IsLocallyControlledPawn() const;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CombatSimComponent> Combat;

	bool bReticleOn = false;
	bool bModeAllowsReticle = false;
	bool bToggleWasHeld = false;
	bool bHoldAimShown = false;
};

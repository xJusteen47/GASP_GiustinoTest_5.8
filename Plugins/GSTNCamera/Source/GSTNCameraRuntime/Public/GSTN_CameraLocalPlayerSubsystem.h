// Per local player camera state (camera.md §4.3, §7.3): the orbit, the frame and the options survive a respawn and carry across mode changes.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GSTN_CameraLocalPlayerSubsystem.generated.h"

class APlayerController;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnCameraReferenceFrameChanged, const FGSTN_CameraFrameChange&, Change);

/** The state that belongs to a player rather than to a pawn. Plain data; the component keeps one of its own when there is no local player (tests, spectator rigs) */
struct FGSTN_CameraPlayerState
{
	FGSTN_CameraFrame Frame;
	bool bFrameInit = false;
	FGSTN_CameraOrbitState Orbit;
	bool bOrbitInit = false;
	/** Seconds since the last look input, for the section recenter */
	float SecondsSinceLookInput = 0.f;
	FGSTN_CameraRuleChoiceState RuleState;
	/** Heading of the last published reference, for the frame-change event */
	FVector LastForward = FVector::ForwardVector;
};

/**
 * One per ULocalPlayer, so split screen has an independent camera per player. Never exists on a dedicated server (there is no local player
 * there). Raises OnReferenceFrameChanged when the camera decides to turn the stick reference (section cut, mode change, respawn);
 * a bridge plugin binds it to the input source (camera.md §5). Manual orbit never raises it.
 */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** The rule behind ShouldCreateSubsystem on plain values, so it is testable: nothing on a dedicated server, in a non game world or disabled */
	static bool ShouldCreateFor(ENetMode NetMode, bool bGameWorld, bool bEnabled);

	static UGSTN_CameraLocalPlayerSubsystem* Get(const APlayerController* Controller);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	FGSTN_CameraPlayerState& GetState() { return State; }

	/** Per player options (user settings, not pawn data) */
	UPROPERTY(BlueprintReadWrite, Category = "Camera|Options")
	bool bInvertLookY = false;

	UPROPERTY(BlueprintReadWrite, Category = "Camera|Options", meta = (ClampMin = "0"))
	float LookSensitivityMultiplier = 1.f;

	UPROPERTY(BlueprintAssignable, Category = "Camera")
	FGSTN_OnCameraReferenceFrameChanged OnReferenceFrameChanged;

	/** Add the settings look mapping context to this player Enhanced Input once. No-op without one or without a controller */
	void EnsureInputContext(APlayerController* Controller);

private:
	FGSTN_CameraPlayerState State;
	TWeakObjectPtr<const UInputMappingContext> AddedContext;
};

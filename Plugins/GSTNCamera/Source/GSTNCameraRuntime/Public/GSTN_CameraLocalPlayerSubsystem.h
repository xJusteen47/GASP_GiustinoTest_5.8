// Per local player camera state (camera.md §4.3, §7.3): the orbit, the frame and the options survive a respawn and carry across mode changes.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GSTN_CameraLocalPlayerSubsystem.generated.h"

class APlayerController;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnCameraReferenceFrameChanged, const FGSTN_CameraFrameChange&, Change);

/** Player camera options (user settings, never pawn or designer data). Saved per local player by SaveUserOptions */
USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraUserOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Options")
	bool bInvertLookY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Options")
	bool bInvertLookX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Options", meta = (ClampMin = "0"))
	float LookSensitivityMultiplier = 1.f;

	/** Distance from the character: Near / Medium / Far presets of the profile, or Custom (the slider below) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Options")
	EGSTN_CameraDistancePreset DistancePreset = EGSTN_CameraDistancePreset::Medium;

	/** Used when DistancePreset is Custom; clamped to the profile slider range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Options", meta = (ClampMin = "0.1"))
	float CustomDistanceScale = 1.f;

	/** The gentle follow behind in a sustained run (decision 2A: on, the player can turn it off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Options")
	bool bAutoFollow = true;

	/** Accessibility: hold recenter and the move stick turns the camera (Astro Bot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Options")
	bool bCameraAssist = false;
};

UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraOptionsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGSTN_CameraUserOptions Options;
};

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
	/** The player distance scale, eased between presets and section limits */
	FGSTN_CameraEasedFloat Distance;
	FGSTN_CameraAutoFollowState AutoFollow;
	FGSTN_CameraStickLatchState StickLatch;
	double LastStickResolveClock = -1.0;
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

	/** Per player options (user settings, not pawn data). Loaded from the save slot when the subsystem starts */
	UPROPERTY(BlueprintReadWrite, Category = "Camera|Options")
	FGSTN_CameraUserOptions Options;

	/** Write the options to this player's save slot (GSTNCamera_Options_<controller id>). True on success */
	UFUNCTION(BlueprintCallable, Category = "Camera|Options")
	bool SaveUserOptions();

	/** Read them back; false (options untouched) when there is no save yet */
	UFUNCTION(BlueprintCallable, Category = "Camera|Options")
	bool LoadUserOptions();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Camera")
	FGSTN_OnCameraReferenceFrameChanged OnReferenceFrameChanged;

	/** Add the profile mapping context to this player Enhanced Input once. No-op without one or without a controller */
	void EnsureInputContext(APlayerController* Controller, const class UGSTN_CameraProfile& Profile);

private:
	FString GetSlotName(int32& OutUserIndex) const;

	FGSTN_CameraPlayerState State;
	TWeakObjectPtr<const UInputMappingContext> AddedContext;
};

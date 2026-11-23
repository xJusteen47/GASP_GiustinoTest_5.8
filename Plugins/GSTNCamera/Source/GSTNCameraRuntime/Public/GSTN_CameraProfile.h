// Camera profile (design.md §3.1, owner rule "no Project Settings, everything a Data Asset"). Replaces the old UGSTN_CameraSettings.
// One profile for every kit (owner rule "all kits the same"); a pawn picks it on its UGSTN_CameraComponent, null = the code defaults below.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraTypes.h"
#include "GameplayTagContainer.h"
#include "GSTN_CameraProfile.generated.h"

class UGSTN_CameraRuleSet;
class UInputAction;
class UInputMappingContext;

/**
 * Pitch curves of the free camera (Nesky: a single distance for every angle is a mistake). X = view pitch in degrees (negative looks down),
 * Y = arm multiplier / FOV offset in degrees. An empty curve is neutral (1 / 0).
 */
USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraPitchCurves
{
	GENERATED_BODY()

	/** Arm multiplier by pitch: looking down from high a bit farther, from below closer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pitch")
	FRuntimeFloatCurve DistanceScaleByPitch;

	/** FOV offset (degrees) by pitch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pitch")
	FRuntimeFloatCurve FovOffsetByPitch;

	float EvalDistanceScale(float PitchDeg) const;
	float EvalFovOffset(float PitchDeg) const;
};

/** Accessibility "Camera Assist" (Astro Bot): while the recenter button is held, the move stick turns the camera */
USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraAssistTuning
{
	GENERATED_BODY()

	/** Hold the recenter button this long before the move stick drives the camera (a tap stays a recenter) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Assist", meta = (ClampMin = "0", ForceUnits = "s"))
	float HoldSeconds = 0.2f;

	/** Camera turn speed at full move stick */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Assist", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float DegPerSecond = 120.f;
};

UCLASS(BlueprintType, meta = (DisplayName = "GSTN Camera Profile"))
class GSTNCAMERARUNTIME_API UGSTN_CameraProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_CameraProfile();

	/** The code defaults (the class default object): what a component without a profile uses */
	static const UGSTN_CameraProfile& GetDefaultProfile() { return *GetDefault<UGSTN_CameraProfile>(); }

	// ---- Rules ----

	/** Rule set when neither the component nor the active section names one. Empty = the built-in table (Aim to Shoulder, opponents to Duel or Arena, InSection to the section mode, else the default mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TSoftObjectPtr<UGSTN_CameraRuleSet> DefaultRuleSet;

	/** Mode of the built-in table when nothing else is chosen. Empty = Camera.Mode.Free (owner 2026-11-21: the camera is free by default) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FGameplayTag DefaultModeTag;

	// ---- Input (soft references: the plugin ships no content) ----

	/** The look input (2D axis). Empty = no plugin look input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> LookAction;

	/** A button: tap = ease the heading behind the character; hold with Camera Assist on = the move stick turns the camera */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> RecenterAction;

	/** A button: swap the shoulder of the Shoulder mode. Empty = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> SwapShoulderAction;

	/** A 1D axis: +1 farther preset, -1 nearer (design.md §3.3 proposes d-pad up/down; never hard-wired). Empty = menu only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> DistanceAction;

	/** The 2D move action, read only for Camera Assist. Empty = no Camera Assist from the plugin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> AssistMoveAction;

	/** Added to the local player Enhanced Input so the actions above have bindings. Empty = the project binds them itself */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> LookMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 MappingContextPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	EGSTN_CameraLookInputSource LookInputSource = EGSTN_CameraLookInputSource::Plugin;

	/** Degrees per unit of look input per frame (a mouse delta). For a stick give the action a Scale by Delta Time modifier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0", ForceUnits = "deg"))
	float LookDegreesPerUnit = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	EGSTN_CameraControlRotationPolicy ControlRotationPolicy = EGSTN_CameraControlRotationPolicy::WriteBack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FGSTN_CameraStickLatchTuning StickLatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FGSTN_CameraAssistTuning CameraAssist;

	// ---- General ----

	/** Initial heading about Up from the reference forward of a fresh orbit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General", meta = (ForceUnits = "deg"))
	float DefaultYawDeg = 0.f;

	/** Cap of the hold on a lost or suppressed subject. 0 = hold until the subject comes back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General", meta = (ClampMin = "0", ForceUnits = "s"))
	float KOHoldSeconds = 0.f;

	/** A camera-made turn of the reference at least this large raises OnReferenceFrameChanged and latches the stick (section values may override) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float HoldMinYawDeltaDeg = 20.f;

	// ---- Free camera (the default) ----

	/** Arm, pitch, FOV, pitch range of the free camera (Astro Bot measured: pitch 15-26 down, horizontal FOV 80-95). Tune on video in P1b */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Free")
	FGSTN_CameraFollowTuning FreeView;

	/** Vertical band, asymmetric pivot, look-ahead, fall pitch, recenter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Free")
	FGSTN_CameraFreeTuning Free;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Free")
	FGSTN_CameraPitchCurves FreePitchCurves;

	/** Player distance presets (Near/Medium/Far/Custom); the player choice lives in the user options */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Free")
	FGSTN_CameraDistancePresets DistancePresets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Free")
	FGSTN_CameraAutoFollowTuning AutoFollow;

	/** Any of these on the pawn (tag source or SetRequestTags) stops the auto follow, e.g. a combat state. Aim and opponents always stop it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Free")
	FGameplayTagContainer AutoFollowBlockTags;

	// ---- Other modes ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FGSTN_CameraStabilityTuning Stability;

	/** The far follow mode (Camera.Mode.Follow) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FGSTN_CameraFollowTuning Follow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FGSTN_CameraShoulderTuning Shoulder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FGSTN_CameraDuelTuning Duel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FGSTN_CameraArenaTuning Arena;

	// ---- Occlusion ----

	/** Probe channel of the occlusion sweep. Walls must Block it; pawns should Ignore it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Camera;

	/** Component or actor tag a level designer puts on props that may fade */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion")
	FName FadeTag = TEXT("GSTN.CameraFade");

	/** Custom primitive data slot a fadeable material reads as its opacity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion", meta = (ClampMin = "0", ClampMax = "35"))
	int32 FadeCustomDataIndex = 0;

	/** Occlusion of Follow, Free, Section, Duel and Arena */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion")
	FGSTN_CameraOcclusionTuning Occlusion;

	/** Occlusion of Shoulder: the camera is close, so plain pull-in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion")
	FGSTN_CameraOcclusionTuning ShoulderOcclusion;

	// ---- Blends ----

	/** Blend into Follow and Free */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blends")
	FGSTN_CameraBlendTuning FollowBlend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blends")
	FGSTN_CameraBlendTuning ShoulderBlend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blends")
	FGSTN_CameraBlendTuning DuelBlend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blends")
	FGSTN_CameraBlendTuning ArenaBlend;

	/** Blend into a section when the section itself names none (BlendInSeconds is on the section) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blends")
	EGSTN_CameraBlendFunction SectionBlendFunction = EGSTN_CameraBlendFunction::EaseInOut;
};

// Camera project settings (camera.md §10).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraTypes.h"
#include "GSTN_CameraSettings.generated.h"

class UGSTN_CameraRuleSet;
class UInputAction;
class UInputMappingContext;

/**
 * Project Settings > Plugins > GSTN Camera. Defaults ship in the plugin Config/DefaultGSTNCamera.ini; a project overrides them in its own
 * Config/DefaultGSTNCamera.ini. Presentation only: every machine may use different values without desynchronising anything. Input assets are
 * soft references so the plugin carries no content: empty = no plugin look input (use AddLookInput or LookInputSource = ControlRotation).
 */
UCLASS(config = GSTNCamera, defaultconfig, meta = (DisplayName = "GSTN Camera"))
class GSTNCAMERARUNTIME_API UGSTN_CameraSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_CameraSettings();

	static const UGSTN_CameraSettings& Get() { return *GetDefault<UGSTN_CameraSettings>(); }

	/** Master switch of the camera component and subsystems (also console variable gstn.Camera.Enable) */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	/** The rule set when neither the component nor the active section names one. Empty = the built-in table (Aim to Shoulder, opponents to Duel or Arena, InSection to the section mode, else Follow) */
	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	TSoftObjectPtr<UGSTN_CameraRuleSet> DefaultRuleSet;

	/** Mode of the built-in table when nothing else is chosen */
	UPROPERTY(Config, EditAnywhere, Category = "Rules")
	FGameplayTag DefaultModeTag;

	/** The look input (2D axis). Empty = no plugin look input */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> LookAction;

	/** A button: ease the heading behind the character facing (camera.md §2.4). Empty = none */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> RecenterAction;

	/** A button: swap the shoulder of the Shoulder mode (CAM.2). Empty = none (use SetShoulderSide / ToggleShoulderSide) */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> SwapShoulderAction;

	/** Added to the local player Enhanced Input subsystem so LookAction has a binding. Empty = the project binds the actions itself */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> LookMappingContext;

	UPROPERTY(Config, EditAnywhere, Category = "Input")
	int32 MappingContextPriority = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Input")
	EGSTN_CameraLookInputSource LookInputSource = EGSTN_CameraLookInputSource::Plugin;

	/** Degrees per unit of look input per frame (a mouse delta). For a stick give the action a Scale by Delta Time modifier */
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "0", ForceUnits = "deg"))
	float LookDegreesPerUnit = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Input")
	bool bInvertLookY = false;

	UPROPERTY(Config, EditAnywhere, Category = "Control Rotation")
	EGSTN_CameraControlRotationPolicy ControlRotationPolicy = EGSTN_CameraControlRotationPolicy::WriteBack;

	/** Initial heading about Up from the reference forward of a fresh orbit */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ForceUnits = "deg"))
	float DefaultYawDeg = 0.f;

	/** Cap of the hold on a lost or suppressed subject. 0 = hold until the subject comes back */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0", ForceUnits = "s"))
	float KOHoldSeconds = 0.f;

	/** A turn of the reference at least this large raises OnReferenceFrameChanged (section values may override) */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float HoldMinYawDeltaDeg = 20.f;

	/** Probe channel of the occlusion sweep. Walls must Block it; pawns should Ignore it */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Camera;

	/** Component or actor tag a level designer puts on walls and ceilings that may fade */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion")
	FName FadeTag = TEXT("GSTN.CameraFade");

	/** Custom primitive data slot a fadeable material reads as its opacity (plugin material function) */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion", meta = (ClampMin = "0", ClampMax = "35"))
	int32 FadeCustomDataIndex = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraStabilityTuning Stability;

	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraFollowTuning Follow;

	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraFreeTuning Free;

	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraShoulderTuning Shoulder;

	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraDuelTuning Duel;

	/** Shared Power Stone style framing for 2 or more opponents (3-4 players) */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraArenaTuning Arena;

	/** Occlusion of Follow, Free, Section, Duel and Arena */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraOcclusionTuning Occlusion;

	/** Occlusion of Shoulder: the camera is close, so plain pull-in */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraOcclusionTuning ShoulderOcclusion;

	/** Blend into Follow and Free */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraBlendTuning FollowBlend;

	/** Blend into Shoulder */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraBlendTuning ShoulderBlend;

	/** Blend into Duel */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraBlendTuning DuelBlend;

	/** Blend into Arena */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	FGSTN_CameraBlendTuning ArenaBlend;

	/** Blend into a section when the section itself names none (BlendInSeconds is on the section) */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning")
	EGSTN_CameraBlendFunction SectionBlendFunction = EGSTN_CameraBlendFunction::EaseInOut;
};

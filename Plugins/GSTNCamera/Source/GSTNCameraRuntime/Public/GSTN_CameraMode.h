// Camera modes, mode definitions and the rule table (camera.md §2, §3).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "GSTN_CameraMode.generated.h"

class AGSTN_CameraSection;

/** Everything a mode may read to compute its view. Built once per frame by the camera component */
USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraModeContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	FGSTN_CameraFrame Frame;

	/** Presented state of the view target */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	FGSTN_CameraSubjectState Subject;

	/** The player orbit, in the frame (already clamped for the active section) */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	FGSTN_CameraOrbitState Orbit;

	/** Real time, never combat frames */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	float DeltaSeconds = 0.f;

	/** The active section, or null */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	TObjectPtr<AGSTN_CameraSection> Section = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	bool bHasOpponent = false;

	/** Presented state of the opponent (Duel) */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	FGSTN_CameraSubjectState Opponent;

	/** Every registered opponent that could be read, in registration order (Arena frames them all); the first one is also Opponent */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	TArray<FGSTN_CameraSubjectState> Opponents;

	/** Heading of the arena base (Arena): the facing of the active arena section about Up, 0 without one */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	float ArenaBaseHeadingDeg = 0.f;

	/** The active section volume as a world box that clamps the Arena focus; used when bHasArenaBounds */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	bool bHasArenaBounds = false;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	FBox ArenaBounds = FBox(ForceInit);

	/** Per player shoulder override (SetShoulderSide / ToggleShoulderSide / the swap action); when false the tuning side applies (Shoulder) */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	bool bHasShoulderSideOverride = false;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	EGSTN_ShoulderSide ShoulderSideOverride = EGSTN_ShoulderSide::Right;

	/** Both fighters stand on ground (Duel side swap) */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	bool bBothGrounded = true;

	/** Aspect ratio of the view, for framing */
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Mode")
	float AspectRatio = 16.f / 9.f;
};

/**
 * One camera behaviour. Each stack entry owns a RUNTIME COPY of the mode of its definition, so a mode may keep smoothing state (a damped
 * pivot, a ground height) without touching the asset. Presentation only: Blueprint is free, no determinism contract (camera.md §2.3).
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNCAMERARUNTIME_API UGSTN_CameraMode : public UObject
{
	GENERATED_BODY()

public:
	/** The view for this frame. A Blueprint child may call the parent */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera|Mode")
	FGSTN_CameraView ComputeView(const FGSTN_CameraModeContext& Context);
	virtual FGSTN_CameraView ComputeView_Implementation(const FGSTN_CameraModeContext& Context);

	/** Pitch the orbit starts at when this mode is the first one used */
	virtual float GetDefaultPitchDeg() const { return -25.f; }

	/** Pitch limits of the manual orbit in this mode */
	virtual void GetPitchRange(float& OutMinDeg, float& OutMaxDeg) const
	{
		OutMinDeg = -80.f;
		OutMaxDeg = 80.f;
	}

	/** Forget the smoothing state: a cut, a respawn or a first use */
	virtual void ResetState() {}
};

/** The far follow camera (Mario 3D World style, camera.md §2.3): damped pivot, free orbit, never turns with the character */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraMode_Follow : public UGSTN_CameraMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow")
	FGSTN_CameraFollowTuning Tuning;

	virtual FGSTN_CameraView ComputeView_Implementation(const FGSTN_CameraModeContext& Context) override;
	virtual float GetDefaultPitchDeg() const override { return Tuning.PitchDeg; }
	virtual void GetPitchRange(float& OutMinDeg, float& OutMaxDeg) const override
	{
		OutMinDeg = Tuning.MinPitchDeg;
		OutMaxDeg = Tuning.MaxPitchDeg;
	}
	virtual void ResetState() override;

protected:
	/** Free camera extension point: adjust the pivot, add to the pitch and the arm before the damping */
	virtual void AdjustFollow(const FGSTN_CameraModeContext& Context, FVector& InOutPivot, float& OutPitchExtraDeg, float& OutArmExtraCm) {}

	FVector SmoothedPivot = FVector::ZeroVector;
	bool bHasPivot = false;
	float AutoAlignOffsetDeg = 0.f;
};

/** The free camera where no section guides the view, Astro Bot style (camera.md §2.4): calm vertical follow, look-ahead, pitch help near drops */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraMode_Free : public UGSTN_CameraMode_Follow
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow")
	FGSTN_CameraFreeTuning FreeTuning;

	virtual void ResetState() override;

protected:
	virtual void AdjustFollow(const FGSTN_CameraModeContext& Context, FVector& InOutPivot, float& OutPitchExtraDeg, float& OutArmExtraCm) override;

private:
	float GroundHeight = 0.f;
	bool bHasGround = false;
	FVector LookAhead = FVector::ZeroVector;
};

/** Over the shoulder (camera.md §2.3, §6): close arm, shoulder offset, look input becomes aim input */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraMode_Shoulder : public UGSTN_CameraMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder")
	FGSTN_CameraShoulderTuning Tuning;

	virtual FGSTN_CameraView ComputeView_Implementation(const FGSTN_CameraModeContext& Context) override;
	virtual float GetDefaultPitchDeg() const override { return -8.f; }
	virtual void GetPitchRange(float& OutMinDeg, float& OutMaxDeg) const override
	{
		OutMinDeg = Tuning.MinPitchDeg;
		OutMaxDeg = Tuning.MaxPitchDeg;
	}
	virtual void ResetState() override { bHasPivot = false; bHasSide = false; }

	/** Side blend in [-1, 1] (+1 right): what the last view used, for the tests and the debug draw */
	float GetSideBlend() const { return SideBlend; }

private:
	FVector SmoothedPivot = FVector::ZeroVector;
	bool bHasPivot = false;
	float SideBlend = 1.f;
	bool bHasSide = false;
};

/** The designer angle of the active section (camera.md §4.1): the Orbit, FixedPoint and Rail paths */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraMode_Section : public UGSTN_CameraMode
{
	GENERATED_BODY()

public:
	/** Pivot damping, so a section camera does not follow every step */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ForceUnits = "s"))
	float PivotDampingSeconds = 0.25f;

	virtual FGSTN_CameraView ComputeView_Implementation(const FGSTN_CameraModeContext& Context) override;
	virtual void ResetState() override { bHasPivot = false; }

private:
	FVector SmoothedPivot = FVector::ZeroVector;
	bool bHasPivot = false;
};

/** PvP framing, Tekken style (camera.md §2.5): the camera centre between the two fighters, both parallel to the screen */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraMode_Duel : public UGSTN_CameraMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel")
	FGSTN_CameraDuelTuning Tuning;

	virtual FGSTN_CameraView ComputeView_Implementation(const FGSTN_CameraModeContext& Context) override;
	virtual float GetDefaultPitchDeg() const override { return Tuning.PitchDeg; }
	virtual void ResetState() override { DuelState = FGSTN_DuelCameraState(); bHasPivot = false; }

	const FGSTN_DuelCameraState& GetDuelState() const { return DuelState; }

private:
	FGSTN_DuelCameraState DuelState;
	FVector SmoothedPivot = FVector::ZeroVector;
	bool bHasPivot = false;
};

/**
 * Shared framing camera for 3-4 players, Power Stone style (CAM.7): the local pawn and every registered opponent in one elevated 3/4 view.
 * The yaw is arena relative (the arena section facing, else the frame reference forward, plus Tuning.YawDeg) and never turns with a player;
 * the look-at point is a damped weighted centroid clamped to the section volume; the distance fits every fighter with screen margins, zooming
 * out faster than in; the pitch rises with the distance. Under any gravity everything is in the local Up frame.
 */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraMode_Arena : public UGSTN_CameraMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Arena")
	FGSTN_CameraArenaTuning Tuning;

	virtual FGSTN_CameraView ComputeView_Implementation(const FGSTN_CameraModeContext& Context) override;
	virtual float GetDefaultPitchDeg() const override { return Tuning.PitchDeg; }
	virtual void ResetState() override { bHasFocus = false; bHasArm = false; }

	/** The damped arm of the last view, for the tests and the debug draw */
	float GetSmoothedArm() const { return SmoothedArm; }

private:
	FVector SmoothedFocus = FVector::ZeroVector;
	bool bHasFocus = false;
	float SmoothedArm = 0.f;
	bool bHasArm = false;
};

/**
 * A mode as data (camera.md §2.3): identity tag, the instanced mode, how it blends in, and what the player may do in it. A designer adds a mode
 * with a Blueprint child of UGSTN_CameraMode, a definition and a Camera.Mode.<Name> tag; replacing Follow is another definition under the same tag.
 */
UCLASS(BlueprintType)
class GSTNCAMERARUNTIME_API UGSTN_CameraModeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Exactly one Camera.Mode.* leaf */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (Categories = "Camera.Mode"))
	FGameplayTag ModeTag;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Camera")
	TObjectPtr<UGSTN_CameraMode> Mode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Blend")
	FGSTN_CameraBlendTuning BlendIn;

	/** Look input moves the orbit in this mode (otherwise it is consumed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bAllowManualControl = true;

	/** Look input is aim input (Shoulder): ignores a section manual-control flag and uses the shoulder limits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bLookIsAim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0"))
	float LookSensitivityScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bShowCrosshair = false;

	/** Converge the aim on the point under the view centre while this mode is on top */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bConvergeAim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion")
	FGSTN_CameraOcclusionTuning Occlusion;

	/** Valid: a Camera.Mode.* tag and a mode */
	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsValidDefinition() const;
};

/** One row of the rule table: the highest priority whose query matches wins (camera.md §3.1) */
USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraModeRule
{
	GENERATED_BODY()

	/** Over the pawn tags plus the synthetic ones (Camera.State.InSection, Camera.Section.<Name>). Empty = always */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rule")
	FGameplayTagQuery Query;

	/** The mode, as a definition asset. Empty = use ModeTag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rule")
	TSoftObjectPtr<UGSTN_CameraModeDefinition> Mode;

	/** A built-in mode by tag (Follow, Free, Shoulder, Section, Duel) when no definition is given */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rule", meta = (Categories = "Camera.Mode"))
	FGameplayTag ModeTag;

	/** The mode is the one the active section names */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rule")
	bool bModeFromSection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rule")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rule", meta = (ClampMin = "0", ForceUnits = "s"))
	float EnterDelaySeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rule", meta = (ClampMin = "0", ForceUnits = "s"))
	float ExitDelaySeconds = 0.15f;
};

/** The table that chooses the mode (camera.md §3.1). Lowest to highest: plugin default, project settings, pawn data, the active section override */
UCLASS(BlueprintType)
class GSTNCAMERARUNTIME_API UGSTN_CameraRuleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rules")
	TArray<FGSTN_CameraModeRule> Rules;

	/** Seconds a chosen rule must stay chosen before another replaces it (camera.md §3.4) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rules", meta = (ClampMin = "0", ForceUnits = "s"))
	float MinDwellSeconds = 0.3f;

	/** The built-in table: Aim to Shoulder (100) unless the section forbids it, one opponent to Duel and two or more to Arena (90), InSection to the section mode (50), else the default mode (0) */
	static UGSTN_CameraRuleSet* MakeDefault(UObject* Outer, const FGameplayTag& DefaultModeTag, float ExitDelaySeconds, float MinDwellSeconds);
};

// Designer camera sections (camera.md §4): the spec, the volume actor, the registry and the presentation actions.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "GSTN_CameraTypes.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_CameraSection.generated.h"

class AGSTN_CameraSection;
class UGSTN_CameraComponent;
class UGSTN_CameraRuleSet;
class USplineComponent;

/** How the section camera is placed (camera.md §4.1) */
UENUM(BlueprintType)
enum class EGSTN_CameraSectionPath : uint8
{
	/** The far follow camera at the designer angle, pitch and distance */
	Orbit,
	/** The camera sits at the actor transform (plus offset) and turns to look at the pawn */
	FixedPoint,
	/** The camera slides on the spline to the point nearest the pawn, looking at the pawn */
	Rail
};

UENUM(BlueprintType)
enum class EGSTN_CameraShoulderPolicy : uint8
{
	Allow,
	Forbid
};

UENUM(BlueprintType)
enum class EGSTN_CameraUpSource : uint8
{
	/** The subject up (gravity) */
	Gravity,
	/** The section actor up: the section decides the frame */
	SectionActor
};

/** Whether the stick reference is held across the camera change this section causes (camera.md §5) */
UENUM(BlueprintType)
enum class EGSTN_CameraStickHold : uint8
{
	None,
	OnCut,
	OnAnyChange
};

/** What a section action receives */
USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraSectionEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Section")
	TObjectPtr<AGSTN_CameraSection> Section = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Section")
	TObjectPtr<AActor> ViewTarget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Section")
	TObjectPtr<UGSTN_CameraComponent> Camera = nullptr;
};

/**
 * Local presentation run when the local view target enters or leaves a section (a depth marker style push, a vignette, a sound). Runs on
 * the machine that owns the camera only, never replicated. A bridge plugin can ship a subclass without this plugin knowing it.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNCAMERARUNTIME_API UGSTN_CameraSectionAction : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Camera|Section")
	void OnSectionEntered(const FGSTN_CameraSectionEvent& Event);
	virtual void OnSectionEntered_Implementation(const FGSTN_CameraSectionEvent& Event) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Camera|Section")
	void OnSectionExited(const FGSTN_CameraSectionEvent& Event);
	virtual void OnSectionExited_Implementation(const FGSTN_CameraSectionEvent& Event) {}
};

/** What a section asks of the camera (camera.md §4.1). Angles are relative to Up and to the actor forward projected on the plane normal to Up */
USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraSectionSpec
{
	GENERATED_BODY()

	// ---- Identity ----

	/** Camera.Section.* identity: added to the rule query container while this section is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (Categories = "Camera.Section"))
	FGameplayTag SectionTag;

	/** The mode the section runs. Camera.Mode.Section (the section path) unless a designer mode replaces it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (Categories = "Camera.Mode"))
	FGameplayTag ModeTag;

	/** Overlaps: the highest wins; equal: the one entered last; still tied: the smaller volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	int32 Priority = 0;

	/** Replaces the rule table while this section is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	TSoftObjectPtr<UGSTN_CameraRuleSet> RuleSetOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	EGSTN_CameraShoulderPolicy ShoulderPolicy = EGSTN_CameraShoulderPolicy::Allow;

	// ---- Angle ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	EGSTN_CameraSectionPath Path = EGSTN_CameraSectionPath::Orbit;

	/** Heading about Up from the actor forward (projected on the plane normal to Up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ForceUnits = "deg"))
	float DesiredYawDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float DesiredPitchDeg = -25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DistanceCm = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "5", ClampMax = "170", ForceUnits = "deg"))
	float FovDeg = 65.f;

	/** Offset of the look target from the pawn pivot, in the section actor space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ForceUnits = "cm"))
	FVector FocalOffset = FVector::ZeroVector;

	/** FixedPoint and Rail: offset of the camera from the actor location or the rail point, in the section actor space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ForceUnits = "cm"))
	FVector CameraOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	EGSTN_CameraUpSource UpSource = EGSTN_CameraUpSource::Gravity;

	// ---- Manual control ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	bool bAllowManualControl = true;

	/** Limits around the desired angle, used when manual control is allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (EditCondition = "bAllowManualControl"))
	FGSTN_CameraOrbitLimits Limits;

	// ---- Recenter ----

	/** Seconds without look input before the orbit eases back to the desired angle. 0 = never */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ForceUnits = "s"))
	float RecenterDelaySeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float RecenterDegPerSecond = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	bool bOnlyWhileMoving = false;

	// ---- Transition ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendInSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendOutSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	EGSTN_CameraBlendFunction BlendFunction = EGSTN_CameraBlendFunction::EaseInOut;

	/** Cut instead of blending on enter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	bool bCutOnEnter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section")
	EGSTN_CameraStickHold StickHold = EGSTN_CameraStickHold::OnCut;

	/** A turn of the reference below this does not raise the frame event; negative = the settings value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ForceUnits = "deg"))
	float HoldMinYawDeltaDeg = -1.f;

	// ---- Presentation ----

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Camera|Section")
	TArray<TObjectPtr<UGSTN_CameraSectionAction>> Actions;
};

/**
 * A volume that tells the camera how to look while the local view target is inside it. Registered on every machine that has a world
 * subsystem (never a dedicated server); membership is a point-in-volume test of the presented pivot each frame, never an overlap
 * event, so remote pawns, corrections and rollbacks cannot trigger one. Optional spline for the Rail path.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Camera Section"))
class GSTNCAMERARUNTIME_API AGSTN_CameraSection : public AVolume
{
	GENERATED_BODY()

public:
	AGSTN_CameraSection();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ShowOnlyInnerProperties))
	FGSTN_CameraSectionSpec Spec;

	/** Half extents of the box used when the volume has no brush (a section made in code or a test) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ForceUnits = "cm"))
	FVector FallbackExtent = FVector(500.0, 500.0, 500.0);

	/** Point in the volume (brush, else the fallback box) */
	UFUNCTION(BlueprintPure, Category = "Camera|Section")
	bool ContainsPoint(const FVector& Point) const;

	/** Volume measure used to break the last priority tie: smaller wins */
	double GetVolumeSize() const;

	/** The heading of the actor forward about Up, in the frame (the base the desired yaw is added to) */
	float GetBaseHeading(const FGSTN_CameraFrame& Frame) const;

	/** The Rail spline sampled into a polyline (empty without spline points) */
	void GetRailPoints(TArray<FVector>& OutPoints, int32 SamplesPerSegment = 12) const;

	USplineComponent* GetRailSpline() const { return Rail; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera|Section")
	TObjectPtr<USplineComponent> Rail;
};

/**
 * Registry of the sections of one world. Local presentation: not created on a dedicated server. Sections inside streamed modules
 * register on load and unregister on unload through BeginPlay and EndPlay.
 */
UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraSectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Nothing on a dedicated server, in a non game world or with the camera disabled */
	static bool ShouldCreateFor(ENetMode NetMode, bool bGameWorld, bool bEnabled);

	static UGSTN_CameraSectionSubsystem* Get(const UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	void RegisterSection(AGSTN_CameraSection* Section);
	void UnregisterSection(AGSTN_CameraSection* Section);
	int32 GetNumSections() const { return Sections.Num(); }

	/** Every registered section whose volume contains the point */
	void GatherContaining(const FVector& Point, TArray<AGSTN_CameraSection*>& OutSections) const;

private:
	TArray<TWeakObjectPtr<AGSTN_CameraSection>> Sections;
};

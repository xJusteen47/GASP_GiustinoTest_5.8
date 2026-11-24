// The G5 pawn rig (M12 G5): one component that makes a real pawn run GSTNInteract (focus probe + prompt) and GSTNCamera (far follow, sections,
// over-the-shoulder aim) without editing the pawn Blueprint. Local and presentation only: nothing here is read by the simulation or replicated,
// and on a dedicated server the rig adds nothing and never ticks.
//
// Why it exists. The two plugins were only ever run headless. A real pawn needs three things they cannot know about: (1) the camera must read
// the PRESENTED pawn, not the raw actor: the actor root steps at the 62.5 Hz simulation rate, snaps on a rollback correction and jumps when the
// body capsule resizes, while UGSTN_CameraAnchorComponent already turns the drawn, Mover smoothed body into a damped, capsule independent
// anchor (spike C1: the camera is evaluated once per frame after the pawn's presentation update, and reads that anchor); (2) the aim request
// ("Camera.Request.Aim") comes from the project's input, and the plugin knows no GAS; (3) the Interact button has to become a tap or a hold.
//
// Relation to UGSTN_PlaytestCameraSubsystem (cvar P1a): it installs the same camera component on the viewed pawn and feeds it the same presented
// pivot through SetSubjectOverride. The two coexist: the rig finds the camera that exists, and an override, when set, wins over this subject.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_PawnRigComponent.generated.h"

class UGSTN_CameraComponent;
class UGSTN_CameraProfile;
class UGSTN_InteractorComponent;
class UGSTN_CameraAnchorComponent;

/**
 * Tap or hold from one button, pure (no world): press starts it, the hold verb fires once when the hold progress reaches 1, and a release
 * before that is a tap. A release after the hold fired does nothing more.
 */
struct GASP_GIUSTINO2_API FGSTN_InteractPressTracker
{
	bool bDown = false;
	bool bHoldFired = false;

	void Press()
	{
		bDown = true;
		bHoldFired = false;
	}

	/** Call every frame with the interactor's hold progress; true exactly once per press, the frame the hold completes */
	bool TickHold(float HoldProgress)
	{
		if (bDown && !bHoldFired && HoldProgress >= 1.f)
		{
			bHoldFired = true;
			return true;
		}
		return false;
	}

	/** True when this release is a tap (the button was down and the hold never fired) */
	bool Release()
	{
		const bool bTap = bDown && !bHoldFired;
		bDown = false;
		bHoldFired = false;
		return bTap;
	}
};

UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent, DisplayName = "GSTN Pawn Rig (Interact + Camera)"))
class GASP_GIUSTINO2_API UGSTN_PawnRigComponent : public UActorComponent, public IGSTN_CameraSubject, public IGSTN_CameraTagSource
{
	GENERATED_BODY()

public:
	UGSTN_PawnRigComponent();

	/** Adds a UGSTN_InteractorComponent when the pawn has none (the focus probe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig")
	bool bAddInteractor = true;

	/** Adds a UGSTN_CameraComponent when the pawn has none and makes it the pawn's view */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig")
	bool bAddCamera = true;

	/** Switches off the pawn's other camera components when this rig added or found the GSTN camera, so only one drives the view */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig")
	bool bDeactivateOtherCameras = true;

	/**
	 * Camera tuning for a pawn that has no profile on its camera yet. For a pawn whose look comes from the sample's control rotation code, set
	 * LookInputSource = ControlRotation in it (spike C4: one writer of the control rotation)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig")
	TObjectPtr<UGSTN_CameraProfile> CameraProfile;

	// ---- Input glue (call from the pawn's input, a Blueprint or an Enhanced Input binding) ----

	/** Interact pressed: the hold lock and the local hold progress start */
	UFUNCTION(BlueprintCallable, Category = "Rig|Interact")
	void InteractPressed();

	/** Interact released: a tap when the hold did not complete first */
	UFUNCTION(BlueprintCallable, Category = "Rig|Interact")
	void InteractReleased();

	/** Over-the-shoulder aim on or off: raises Camera.Request.Aim for the rule table (the plugin's Shoulder mode) */
	UFUNCTION(BlueprintCallable, Category = "Rig|Camera")
	void SetAimRequested(bool bAim) { bAimRequested = bAim; }

	UFUNCTION(BlueprintPure, Category = "Rig|Camera")
	bool IsAimRequested() const { return bAimRequested; }

	/** KO or shatter: the camera holds its last view while the subject is suppressed */
	UFUNCTION(BlueprintCallable, Category = "Rig|Camera")
	void SetSubjectSuppressed(bool bSuppressed) { bSubjectSuppressed = bSuppressed; }

	UFUNCTION(BlueprintPure, Category = "Rig")
	UGSTN_InteractorComponent* GetInteractor() const { return Interactor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Rig")
	UGSTN_CameraComponent* GetCamera() const { return Camera.Get(); }

	// ---- Pure rules (public for the tests) ----

	/** The rig does nothing on a dedicated server or outside a game world: no presentation exists there */
	static bool ShouldRigRunFor(ENetMode NetMode, bool bGameWorld);

	/**
	 * The subject the camera reads (spike C1). Everything but the pivot comes from Raw (Up, velocity, facing, grounded); the pivot is the
	 * presented one (the standing root of the drawn body) when there is one, so a simulation step, a rollback correction or a capsule resize
	 * of the actor root never reaches the view
	 */
	static FGSTN_CameraSubjectState BuildPresentedSubject(const FGSTN_CameraSubjectState& Raw, bool bHasPresentedPivot, const FVector& PresentedPivot, bool bSuppressed);

	// IGSTN_CameraSubject
	virtual bool GetCameraSubjectState_Implementation(FGSTN_CameraSubjectState& OutState) const override;
	// IGSTN_CameraTagSource
	virtual void GetCameraTags_Implementation(FGameplayTagContainer& OutTags) const override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void BuildRig();
	/** The camera ticks after the anchor so a frame's view is built from that frame's damped pivot (it may appear after BeginPlay) */
	void WireTickOrder();

	TWeakObjectPtr<UGSTN_InteractorComponent> Interactor;
	TWeakObjectPtr<UGSTN_CameraComponent> Camera;
	TWeakObjectPtr<UGSTN_CameraAnchorComponent> WiredAnchor;

	FGSTN_InteractPressTracker Press;
	bool bAimRequested = false;
	bool bSubjectSuppressed = false;
	bool bAddedCamera = false;
};

/**
 * Puts a UGSTN_PawnRigComponent on the pawn each local player views, when it has none, so a test map or a playtest needs no Blueprint edit.
 * Off by default (console gstn.G5.AutoRig 1 turns it on): the GASP camera stays the play view until the owner asks. Never on a dedicated server
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_PawnRigSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
};

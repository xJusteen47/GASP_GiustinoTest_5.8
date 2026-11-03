// The camera component (camera.md §2, §10): the mode stack, the view, the control rotation write-back, the aim data.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GSTN_CameraLocalPlayerSubsystem.h"
#include "GSTN_CameraMode.h"
#include "GSTN_CameraTypes.h"
#include "GameplayTagContainer.h"
#include "GSTN_CameraComponent.generated.h"

class AGSTN_CameraSection;
class UPrimitiveComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnReferenceFrameChangedNative, const FGSTN_CameraFrameChange&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnCrosshairUpdated, const FGSTN_CrosshairView&, Crosshair);

/**
 * The camera of one local player, on the pawn it follows. LOCAL and PRESENTATION ONLY: it is never replicated, runs no code on a dedicated
 * server, reads the PRESENTED state of the view target (IGSTN_CameraSubject or an adapter) and writes nothing the simulation reads, except the
 * control rotation the input source samples CameraYaw and aim from (camera.md §1, §5).
 *
 * Each frame, after movement (TG_PostUpdateWork): read the subject, step the gravity frame, find the section, evaluate the rule table over the
 * pawn tags (IGSTN_CameraTagSource) plus the synthetic ones, push the chosen mode on the stack, apply the look input to the orbit, compute and
 * blend the views, resolve occlusion, fill the view and write the control rotation back.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent, DisplayName = "GSTN Camera Component"))
class GSTNCAMERARUNTIME_API UGSTN_CameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UGSTN_CameraComponent();

	/** Pawn rule set: replaces the settings one, replaced in turn by the active section override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	TSoftObjectPtr<UGSTN_CameraRuleSet> RuleSetOverride;

	/** The rule behind "runs here": nothing on a dedicated server or when disabled */
	static bool ShouldRunFor(ENetMode NetMode, bool bGameWorld, bool bEnabled);

	// ---- Per frame ----

	/** One frame of work; called from the component tick, public for the tests and for projects that drive the camera themselves */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void EvaluateCamera(float DeltaSeconds);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	// ---- Inputs ----

	/** Look input in input units (x right, y up); scaled by the settings, the player options and the mode. Consumed when the mode or section forbids manual control */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void AddLookInput(FVector2D Delta);

	/** Ease the heading behind the facing of the subject (camera.md §2.4) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void RequestRecenter();

	/** The opponent of the Duel mode (project glue chooses it); null = none. Same as SetOpponents with one entry */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetOpponent(AActor* Opponent);

	/**
	 * Every opponent of the local player (project glue chooses them). The default rules pick Duel for exactly one and Arena (shared framing of everybody) for two or more
	 * (the tags Camera.State.OneOpponent / ManyOpponents are computed locally from this list)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetOpponents(const TArray<AActor*>& Opponents);

	/** Per player shoulder override of the Shoulder mode; the camera slides across in ShoulderSwapBlendTime */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetShoulderSide(EGSTN_ShoulderSide Side);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ToggleShoulderSide();

	/** The side in force: the override, else the settings default */
	UFUNCTION(BlueprintPure, Category = "Camera")
	EGSTN_ShoulderSide GetShoulderSide() const;

	/** Tags the project feeds without implementing IGSTN_CameraTagSource (merged with them) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetRequestTags(const FGameplayTagContainer& Tags) { RequestTags = Tags; }

	/** Project glue supplies the subject state itself (a link derived transform, a piggyback anchor, camera.md §7.3) instead of the adapters */
	void SetSubjectOverride(const FGSTN_CameraSubjectState& State);
	void ClearSubjectOverride();

	/** Drop the stack to its top entry and reset the smoothing state: a respawn, a teleport. Raises the frame event when the heading turns */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void Cut();

	// ---- Queries ----

	UFUNCTION(BlueprintPure, Category = "Camera")
	FGameplayTag GetActiveModeTag() const { return ActiveModeTag; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	int32 GetStackDepth() const { return Stack.Num(); }

	UFUNCTION(BlueprintPure, Category = "Camera")
	AGSTN_CameraSection* GetActiveSection() const { return ActiveSection.Get(); }

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsHoldingView() const { return bHolding; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	FGSTN_CameraOrbitState GetOrbit();

	UFUNCTION(BlueprintPure, Category = "Camera")
	FGSTN_CameraFrame GetFrame();

	UFUNCTION(BlueprintPure, Category = "Camera")
	FGSTN_CameraView GetBlendedView() const { return LastView; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	FGSTN_CameraPose GetPose() const { return LastPose; }

	/** The direction to send as AimYaw/AimPitch while a converging mode is on top (camera forward otherwise) */
	UFUNCTION(BlueprintPure, Category = "Camera")
	FVector GetAimDirection() const { return AimDirection; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	FGSTN_CrosshairView GetCrosshair() const { return Crosshair; }

	/** The runtime copy of the mode on top of the stack (per entry state), or null */
	UGSTN_CameraMode* GetTopModeInstance() const;

	/** The definition on top of the stack, or null */
	UGSTN_CameraModeDefinition* GetTopDefinition() const;

	/** Fadeable: the settings fade tag on the component or its actor */
	static bool IsFadeable(const UPrimitiveComponent* Component);

	/** Components currently fading or faded, for the tests and the debug view */
	int32 GetNumFadedComponents() const { return Fades.Num(); }

	UPROPERTY(BlueprintAssignable, Category = "Camera")
	FGSTN_OnCrosshairUpdated OnCrosshairUpdated;

	/** Raised when the camera decides to turn the stick reference (camera.md §5); the local player subsystem relays the same event */
	UPROPERTY(BlueprintAssignable, Category = "Camera")
	FGSTN_OnCameraReferenceFrameChanged OnReferenceFrameChanged;

	/** The same event for C++ listeners (bridges, tests) */
	FGSTN_OnReferenceFrameChangedNative OnReferenceFrameChangedNative;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FGSTN_CameraPlayerState& State();
	bool ReadSubject(FGSTN_CameraSubjectState& Out) const;
	UGSTN_CameraModeDefinition* GetBuiltinDefinition(const FGameplayTag& Tag);
	UGSTN_CameraModeDefinition* ResolveDefinition(const FGSTN_CameraModeRule& Rule);
	int32 KeyFor(UGSTN_CameraModeDefinition* Definition, AGSTN_CameraSection* Section);
	UGSTN_CameraRuleSet* ChooseRuleSet();
	void UpdateSection(const FGSTN_CameraSubjectState& Subject);
	void RaiseFrameChange(const FVector& From, const FVector& To, EGSTN_CameraFrameChangeReason Reason, bool bCut, float MinDeltaDeg);
	void ApplyLook(const FGSTN_CameraSubjectState& Subject, FGSTN_CameraPlayerState& Player, float DeltaSeconds);
	void ResolveOcclusion(const FGSTN_CameraView& View, const FGSTN_CameraOcclusionTuning& Tuning, float DeltaSeconds, FGSTN_CameraPose& InOutPose);
	void UpdateAim(const FGSTN_CameraSubjectState& Subject, const FGSTN_CameraPose& Pose, UGSTN_CameraModeDefinition* Top);
	void ReleaseFades();
	void DropSlots(const TArray<int32>& Keys);

	FGSTN_CameraPlayerState FallbackState;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UGSTN_CameraModeDefinition>> BuiltinDefinitions;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CameraRuleSet> BuiltinRuleSet;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CameraRuleSet> CurrentRuleSet;

	/** Last view of every stack entry: a covered entry blends from the view it had when it stopped being on top */
	TMap<int32, FGSTN_CameraView> LastViews;

	/** Per entry runtime copies of the modes (state lives here, never in the asset) and their definitions, by stack key */

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UGSTN_CameraMode>> SlotInstances;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UGSTN_CameraModeDefinition>> SlotDefinitions;

	/** An entry is a definition, and for section entries the section it was made for: moving between two sections blends between them */
	using FEntryKey = TTuple<TWeakObjectPtr<const UGSTN_CameraModeDefinition>, TWeakObjectPtr<const AGSTN_CameraSection>>;
	TMap<FEntryKey, int32> Keys;

	/** The section of each section entry */
	TMap<int32, TWeakObjectPtr<AGSTN_CameraSection>> SlotSections;
	int32 NextKey = 1;

	FGSTN_CameraModeStack Stack;

	FGameplayTag ActiveModeTag;
	FGameplayTagContainer RequestTags;

	TWeakObjectPtr<AGSTN_CameraSection> ActiveSection;
	/** When the target entered each section it is inside (camera clock), for the equal-priority tie */
	TMap<TWeakObjectPtr<AGSTN_CameraSection>, double> EnterTimes;
	float LastSectionBlendOut = 0.8f;
	EGSTN_CameraBlendFunction LastSectionBlendFunction = EGSTN_CameraBlendFunction::EaseInOut;

	TArray<TWeakObjectPtr<AActor>> OpponentActors;
	bool bHasShoulderSideOverride = false;
	EGSTN_ShoulderSide ShoulderSideOverride = EGSTN_ShoulderSide::Right;
	bool bSwapShoulderWasDown = false;
	FGSTN_CameraSubjectState SubjectOverride;
	bool bHasSubjectOverride = false;

	FVector2D PendingLook = FVector2D::ZeroVector;
	bool bRecentering = false;
	float RecenterRate = 0.f;
	float RecenterTargetYaw = 0.f;
	bool bRecenterWasDown = false;

	bool bHolding = false;
	float HoldSeconds = 0.f;
	bool bPendingCut = false;
	bool bHasView = false;
	double Clock = 0.0;

	FGSTN_CameraView LastView;
	FGSTN_CameraPose LastPose;
	FVector LastPublishedForward = FVector::ForwardVector;
	bool bHasPublishedForward = false;

	FVector AimDirection = FVector::ForwardVector;
	FGSTN_CrosshairView Crosshair;

	/** Fraction of the arm kept by the pull-in occlusion, smoothed */
	float PullInFraction = 1.f;
	struct FFade
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		float Opacity = 1.f;
		bool bBlockingNow = false;
	};
	TArray<FFade> Fades;
};

// Combat presentation (GAS-NPP fork): the simple locomotion anim graph of the greybox (greybox-capsula, owner 2026-11-24 2A). Never read by
// the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_SimpleLocoAnimInstance.generated.h"

class UMoverComponent;
class UGSTN_RunPresentationComponent;

/** The states of the simple locomotion graph; its state machine switches on this, its choosers filter on it */
UENUM(BlueprintType)
enum class EGSTN_SimpleLocoState : uint8
{
	Idle,
	RunStart,
	Run,
	RunStop,
	Skid,
	Air,
	Crouch
};

/** What the state logic reads for one frame (filled on the game thread from Mover and the run presentation) */
struct GSTNCOMBATNP_API FGSTN_SimpleLocoInputs
{
	float Speed2D = 0.f;
	/** Stick along the velocity, -1..1 (UGSTN_RunPresentationComponent::GetAnimOwnerStickAlong) */
	float StickAlong = 0.f;
	bool bOnGround = true;
	bool bCrouching = false;
	bool bSkidding = false;
	/** The run presentation's own run stop is playing (its clip timing) */
	bool bRunStopping = false;
};

/** Tuning of the state logic, on the anim Blueprint's class defaults */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_SimpleLocoSettings
{
	GENERATED_BODY()

	/** Ground speed under which the character counts as still */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple Loco", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MoveSpeed = 10.f;

	/** How long RunStart lasts after leaving rest before Run takes over (the start clip's length is a chooser row choice) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple Loco", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float RunStartSeconds = 0.35f;

	/** Stick along the velocity at or under this, while moving on the ground = stopping (RunStop until still) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple Loco", meta = (ClampMin = "-1", ClampMax = "1"))
	float StopStickAlong = 0.1f;

	/** How fast the warping alphas follow the chosen row's targets (1/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simple Loco|Warping", meta = (ClampMin = "0.1"))
	float WarpBlendSpeed = 8.f;
};

/** Pure state logic of the simple locomotion graph (the tests call it) */
struct GSTNCOMBATNP_API FGSTN_SimpleLocoLogic
{
	/** The state for this frame, from the inputs, the previous state and how long it lasted */
	static EGSTN_SimpleLocoState NextState(const FGSTN_SimpleLocoInputs& In, EGSTN_SimpleLocoState Previous, float TimeInPrevious,
		const FGSTN_SimpleLocoSettings& Settings);
};

/**
 * Parent class of ABP_GSTN_SimpleLoco: a state machine that only passes variables to Chooser Tables, no motion matching (owner 2026-11-24:
 * "chooser + state machine that only gives the variables; putting things in the choosers is easy and scalable").
 *
 * The C++ decides the state (LocoState) from Mover and the run presentation; the Blueprint's state machine switches on it, and every state is
 * one GSTN Chooser Player on the same chooser, whose first column is LocoState. So a new clip, a variant for a stage, a landing after Air
 * (column PreviousLocoState) is one chooser row. Montages play through the DefaultSlot after the machine. Selected with gstn.Loco.Base 2.
 *
 * Warping (owner 2026-11-24: rotation and location warping in some phases, e.g. the skid): the rows write OrientationWarpTarget /
 * StrideWarpTarget through output columns; the alphas below follow them and drive the Orientation / Stride Warping nodes after the machine
 * (Graph mode, LocomotionAngle and LocomotionSpeed from here). Targets reset to 0 on every state change, so warping is on only where a row
 * asks for it. Distance matching is not used yet.
 *
 * Presentation only: Mover moves the character, the clips only pose it (no root motion).
 */
UCLASS()
class GSTNCOMBATNP_API UGSTN_SimpleLocoAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Simple Loco")
	FGSTN_SimpleLocoSettings Settings;

	//~ Chooser inputs (read by the chooser's columns, parameter 0)

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State")
	EGSTN_SimpleLocoState LocoState = EGSTN_SimpleLocoState::Idle;

	/** The state before the current one (e.g. Idle after Air = a landing row) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State")
	EGSTN_SimpleLocoState PreviousLocoState = EGSTN_SimpleLocoState::Idle;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State", meta = (ForceUnits = "s"))
	float TimeInState = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State", meta = (ForceUnits = "cm/s"))
	float Speed2D = 0.f;

	/** Horizontal acceleration along the velocity (signed: negative = braking), smoothed */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State")
	float Acceleration2D = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State", meta = (ForceUnits = "cm/s"))
	float VelocityZ = 0.f;

	/** Velocity direction relative to the actor's facing, degrees -180..180 (0 = forward) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State")
	float MoveAngle = 0.f;

	/** The presented run stage, 1-based (1 run, 2 sprint 1, 3 sprint 2); 0 = none */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|State")
	int32 RunStage = 0;

	//~ Warping (rows write the targets; the warping nodes read the rest)

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Simple Loco|Warping")
	float OrientationWarpTarget = 0.f;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Simple Loco|Warping")
	float StrideWarpTarget = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|Warping")
	float OrientationWarpAlpha = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|Warping")
	float StrideWarpAlpha = 0.f;

	/** Orientation warping input (= MoveAngle) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|Warping")
	float LocomotionAngle = 0.f;

	/** Stride warping input (= Speed2D) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Simple Loco|Warping")
	float LocomotionSpeed = 0.f;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	TWeakObjectPtr<UMoverComponent> Mover;
	TWeakObjectPtr<UGSTN_RunPresentationComponent> RunPresentation;
	FVector PreviousVelocity = FVector::ZeroVector;
	bool bHasPreviousVelocity = false;
};

// The momentum slide of a ground move started while running (owner 2026-11-13, FGSTN_MomentumSlide).

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "LayeredMove.h"
#include "Movement/GSTN_MomentumSlide.h"
#include "GSTN_LayeredMove_MomentumSlide.generated.h"

/** Life of a momentum slide move (serialized as a byte) */
enum class EGSTN_MomentumSlidePhase : uint8
{
	/** Queued, the first frame not generated yet */
	Undecided,
	Sliding,
	/** Finished, waiting for the flush to remove it */
	Done
};

/**
 * Keeps a character that started a ground move while running sliding on its feet along the running direction, slowed by the friction of
 * the move's data (FGSTN_MomentumSlide on the timeline), with the montage's root motion added on top.
 *
 * Queued (empty) by the combat component when a timeline with the slide enabled begins, the move decides everything on its own first
 * frame, inside Mover's tick: the running velocity is the start velocity of that frame (the end of the previous one), the facing is the
 * one of that frame after its instant effects (a snap turn of the move's first frame counts), the data is the running timeline's, read
 * through UGSTN_CombatSimComponent::ReadRunningSlide (the combat model ticked before Mover in the same frame). A stopped character, an air
 * move or a Stop response ends it there.
 *
 * Then every frame, until it finishes itself (DurationMs set so the next flush removes it):
 *   - the timeline it started with must still run (same start frame, no recoil) and the character must stay in the ground mode: a hit,
 *     a cancel into another move, the end of the move or leaving the ground end the slide, and whatever replaces it takes over the
 *     same frame (a knockback, the next move's own slide, the walking mode with the slide's last speed);
 *   - a hitstop freeze (a move carrying Combat.Mover.Frozen) wins over it anyway; the slide does not count the frozen frames, so it resumes
 *     at the speed it stopped with;
 *   - at a ledge (no floor under the next step) it ends, when the data says so.
 *
 * Mixing: OverrideVelocity at priority 0, queued before the montage starts, so it wins over the montage's root motion move (same priority,
 * earlier start) and loses to the freeze (priority 1). The root motion is not lost: the move finds this frame's root motion move among the
 * queued moves of its start state and adds its ground velocity. Vertical velocity is left to the walking mode's ground snap, like the push back.
 *
 * Rollback: every latched value is a serialized field of the move, so a restored sync state replays it. A step is counted once per
 * simulation time (a refunded substep re-generates with the same time), and the speed of a live frame is recomputed from the start speed.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_LayeredMove_MomentumSlide : public FLayeredMoveBase
{
	GENERATED_BODY()

	FGSTN_LayeredMove_MomentumSlide();
	virtual ~FGSTN_LayeredMove_MomentumSlide() = default;

	/** EGSTN_MomentumSlidePhase */
	UPROPERTY()
	uint8 Phase = static_cast<uint8>(EGSTN_MomentumSlidePhase::Undecided);

	/** The response picked was the Against one (data for the trace and the debug display) */
	UPROPERTY()
	bool bAgainst = false;

	/** Unit running direction in the ground plane, latched on the first frame */
	UPROPERTY()
	FVector Direction = FVector::ZeroVector;

	/** Speed of the first live frame, cm/s */
	UPROPERTY()
	float StartSpeed = 0.f;

	/** Resolved friction of the picked response */
	UPROPERTY()
	float Deceleration = 0.f;

	UPROPERTY()
	float Friction = 0.f;

	/** Live frames the slide may last, 0 = no limit */
	UPROPERTY()
	int32 MaxFrames = 0;

	UPROPERTY()
	bool bStopAtLedge = true;

	UPROPERTY()
	bool bAddRootMotion = true;

	/** Simulation frame the timeline the slide belongs to started on (UGSTN_CombatSimComponent::ReadRunningSlide) */
	UPROPERTY()
	int32 TimelineStartFrame = INDEX_NONE;

	/** Live frames started before the current step */
	UPROPERTY()
	int32 LiveFrames = 0;

	/** Index of the live frame of the step at LastStepTimeMs, INDEX_NONE when that step was frozen */
	UPROPERTY()
	int32 CurrentLiveFrame = INDEX_NONE;

	/** Simulation time of the last step counted */
	UPROPERTY()
	double LastStepTimeMs = -1.0;

	/** How far under the feet the ledge probe looks, cm (as the push back) */
	static constexpr double LedgeProbeDepth = 60.0;

	/** Iteration cap of the speed recomputation: no slide lasts this many frames at 62.5 Hz (about 10 s) */
	static constexpr int32 MaxSimulatedFrames = 625;

	/** The speed of a live frame of this slide */
	float SpeedAtLiveFrame(int32 LiveFrame, float DeltaSeconds) const;

	virtual bool SupportsAsync() const override { return false; }
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

private:
	/** Marks the move done: the next flush removes it */
	void Finish(const FMoverTimeStep& TimeStep);

	/** First frame: reads the run, the facing and the data; false when there is nothing to slide */
	bool Decide(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp, const FGSTN_MomentumSlide& Data, int32 RunningStartFrame);

	/** The ground velocity and the turn of this frame's montage root motion, when the montage has some */
	static bool FindRootMotion(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard,
		const FVector& Up, FVector& OutGroundVelocity, FVector& OutAngularVelocityDegrees);

	/** True while a hitstop freeze holds the pawn (a move with Combat.Mover.Frozen, active or queued this frame) */
	static bool IsFrozen(const FMoverTickStartData& StartState);

	/** True when the step at Velocity keeps floor under the feet */
	static bool HasFloorAhead(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp, const FVector& Velocity, double StepMs);
};

template<>
struct TStructOpsTypeTraits<FGSTN_LayeredMove_MomentumSlide> : public TStructOpsTypeTraitsBase2<FGSTN_LayeredMove_MomentumSlide>
{
	enum { WithCopy = true };
};

/** Queues an undecided momentum slide on the target's Mover once */
UCLASS(DisplayName = "GSTN Momentum Slide Move Creator")
class GSTNCOMBATNP_API UGSTN_MomentumSlideMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: starts the momentum slide of the move that begins this frame (UGSTN_CombatSimComponent::ExecuteMomentumSlide) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_MomentumSlide : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_MomentumSlide();
};

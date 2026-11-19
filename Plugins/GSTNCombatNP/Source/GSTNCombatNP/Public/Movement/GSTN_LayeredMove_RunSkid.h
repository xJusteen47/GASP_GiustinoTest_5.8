// The run skid of the Mario-style run (docs/notes/corsa-mario-piano.md, P2).

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "LayeredMove.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "GSTN_LayeredMove_RunSkid.generated.h"

/** Life of a run skid move (serialized as a byte) */
enum class EGSTN_RunSkidLife : uint8
{
	/** Queued, the first frame not generated yet */
	Undecided,
	Running,
	/** Finished, waiting for the flush to remove it */
	Done
};

/**
 * The run skid: the stick reversed against a fast run (FGSTN_RunSkidMath::ShouldStart, decided by the combat tick from the end of the
 * previous frame and this frame's stick) brakes the character on its feet along the old direction with the kit's constant deceleration,
 * then turns it to the new direction and pushes it off at ExitSpeed; the walking mode takes over from there.
 *
 * Replaces the prototype's game-thread skid (corsa-mario-piano §1): everything runs inside Mover's tick from the start state, every latched
 * value is a serialized field of the move, the speed of a live frame is a closed form of the start speed. Releasing the stick does not
 * cancel it. It ends itself when:
 *   - the exit is over;
 *   - the character leaves the ground (a jump from the skid, or a SlideOff ledge): the air takes the velocity of the frame;
 *   - the combatant stops being actionable (an attack, which then reads the carried speed of the run state, P3; a hit; a grab).
 * A hitstop freeze pauses it like the momentum slide.
 *
 * Ledge: the kit's rule (FGSTN_RunSkidTuning::Ledge), overridden by the floor's surface (FGSTN_SurfaceLocomotion::SkidLedge), latched on the
 * first frame. The floor's FrictionScale scales the brake (ice). StopAtLedge: no floor under the next step = the skid comes to rest there.
 *
 * Tags: Movement.State.Skid while running, Movement.State.Skid.JumpWindow inside the window. Derived from the move in the sync state, so a
 * rollback restores them; the combat tick copies them into the input context (kit bindings) and the presentation reads the phase.
 *
 * Mixing: OverrideVelocity at priority 0, like the momentum slide (a freeze, HitstopPriority, wins).
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_LayeredMove_RunSkid : public FLayeredMoveBase
{
	GENERATED_BODY()

	FGSTN_LayeredMove_RunSkid();
	virtual ~FGSTN_LayeredMove_RunSkid() = default;

	/** EGSTN_RunSkidLife */
	UPROPERTY()
	uint8 Life = static_cast<uint8>(EGSTN_RunSkidLife::Undecided);

	/** Unit old running direction (the slide) in the ground plane */
	UPROPERTY()
	FVector Direction = FVector::ZeroVector;

	/** Unit new direction (the reversed stick of the first frame) */
	UPROPERTY()
	FVector TurnDirection = FVector::ZeroVector;

	/** Ground speed of the first frame, cm/s */
	/** Side of the exit turn, latched on the first frame: +1 positive yaw, -1 negative, 0 the shorter way (EGSTN_SkidTurnSide) */
	UPROPERTY()
	int8 TurnSign = 0;

	UPROPERTY()
	float StartSpeed = 0.f;

	/** The brake after the surface's friction scale, cm/s^2 */
	UPROPERTY()
	float Brake = 0.f;

	UPROPERTY()
	int32 EnterFrames = 0;

	UPROPERTY()
	int32 ExitFrames = 0;

	UPROPERTY()
	float ExitSpeed = 0.f;

	UPROPERTY()
	int32 JumpWindowStart = 0;

	UPROPERTY()
	int32 JumpWindowEnd = -1;

	/** The resolved ledge rule (kit or surface): true = stop at the edge */
	UPROPERTY()
	bool bStopAtLedge = true;

	/** Live frame the skid comes to rest (from the brake, or earlier at a ledge) */
	UPROPERTY()
	int32 RestFrame = 0;

	/** Live frames started before the current step */
	UPROPERTY()
	int32 LiveFrames = 0;

	/** Index of the live frame of the step at LastStepTimeMs, INDEX_NONE when that step was frozen */
	UPROPERTY()
	int32 CurrentLiveFrame = INDEX_NONE;

	/** Simulation time of the last step counted */
	UPROPERTY()
	double LastStepTimeMs = -1.0;

	/** The skid tuning of this move as latched on its first frame (the phase and window rules) */
	FGSTN_RunSkidTuning MakeTuning() const;

	/** The phase of the current live frame (None before the first frame and after the exit) */
	EGSTN_RunSkidPhase GetPhase() const;

	/** The ground speed of the current live frame along Direction (0 in the exit) */
	float GetSlideSpeed(float DeltaSeconds) const;

	/** The running skid in a sync state's active moves, null when there is none */
	static const FGSTN_LayeredMove_RunSkid* FindActive(const struct FMoverSyncState& SyncState);

	virtual bool SupportsAsync() const override { return false; }
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

private:
	void Finish(const FMoverTimeStep& TimeStep);
	bool Decide(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp);
};

template<>
struct TStructOpsTypeTraits<FGSTN_LayeredMove_RunSkid> : public TStructOpsTypeTraitsBase2<FGSTN_LayeredMove_RunSkid>
{
	enum { WithCopy = true };
};

/** Queues an undecided run skid on the target's Mover once */
UCLASS(DisplayName = "GSTN Run Skid Move Creator")
class GSTNCOMBATNP_API UGSTN_RunSkidMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: starts a run skid this frame (UGSTN_CombatSimComponent::StepRunSkid) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_RunSkid : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_RunSkid();
};

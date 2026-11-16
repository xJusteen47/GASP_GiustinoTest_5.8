// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "Movement/GSTN_DashTypes.h"
#include "Movement/GSTN_MoveTimelines.h"
#include "GSTN_GA_Dash.generated.h"

/**
 * Creates the instant velocity effect of a dash or dodge from its data (FGSTN_DashMoveData) and the local gravity frame of the
 * Mover sim state: Up is the tracker's up direction (-GravityDir), forward is the facing projected onto the plane perpendicular to it,
 * the prior velocity is the tracker's. Nothing reads world Z or a Mover component query, so the impulse is the same on every machine
 * and in every re-simulation, whatever the gravity is.
 */
UCLASS(DisplayName = "GSTN Move Impulse Creator")
class GSTNCOMBATNP_API UGSTN_MoveImpulseCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	FGSTN_DashMoveData Move;

	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Base of the instant move effects: one apply-move component whose creator holds the speeds */
UCLASS(Abstract)
class GSTNCOMBATNP_API UGSTN_GE_MoveBase : public UGameplayEffect
{
	GENERATED_BODY()

protected:
	/** Makes the effect instant with an apply-move component and a creator of Move */
	void Configure(const FGSTN_DashMoveData& Move);
};

/** Instant: the default dash impulse (FGSTN_MoveTimelines::DashMove), a ground burst that keeps the walking mode */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_MoveDash : public UGSTN_GE_MoveBase
{
	GENERATED_BODY()

public:
	UGSTN_GE_MoveDash();
};

/** Instant: the default ground dodge impulse (FGSTN_MoveTimelines::GroundDodgeMove), a low long-jump arc into the air mode */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_MoveGroundDodge : public UGSTN_GE_MoveBase
{
	GENERATED_BODY()

public:
	UGSTN_GE_MoveGroundDodge();
};

/** Instant: the default air dodge impulse (FGSTN_MoveTimelines::AirDodgeMove), the forward and down dive of the dive roll */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_MoveAirDodge : public UGSTN_GE_MoveBase
{
	GENERATED_BODY()

public:
	UGSTN_GE_MoveAirDodge();
};

/**
 * A timeline ability whose movement is one impulse at take-off: dash, ground dodge, air dodge (combat-m3 section 3). Pluggable content,
 * not framework: the ability holds no numbers. The frame data is the timeline (a movement track whose bCancelOnLand and LandingLagFrames
 * this ability reads, a hurtbox modifier for the invincible feet, declared until hits exist in M4), the speeds are the data of MoveEffect,
 * the blocked tags are ActivationBlockedTags, and a kit changes any of them by replacing the class, a Blueprint child or the timeline
 * asset. A kit without a dodge removes the command from its command set instead (CommandSet.RemovedCommands).
 *
 * The ability never sets the character's state: it applies MoveEffect (a gameplay effect with UApplyMoveGameplayEffectComponent) and
 * Mover applies the queued move. It ends the timeline on landing when the movement track says bCancelOnLand, after the track's landing
 * lag, reading the ground tag from the Mover sim state of the frame (never the Mover component's own queries), so the character is
 * actionable again straight away and there is no extra state for it.
 */
UCLASS(Abstract, Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_MoveTimeline : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_MoveTimeline(const FObjectInitializer& ObjectInitializer);

	/** Applied to the owner when the timeline starts. It must move the character (UApplyMoveGameplayEffectComponent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	TSubclassOf<UGameplayEffect> MoveEffect;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

	/** The tags that block the activation (the ability's data: a kit clears or changes them) */
	const FGameplayTagContainer& GetBlockedTags() const { return ActivationBlockedTags; }

protected:
	virtual void OnTimelineBegan(int32 StateStartFrame) override;

	/** Sets the default timeline from Params and the move effect and the blocked tags of a defensive move */
	void SetDefaultMove(const FGSTN_MoveTimelineParams& Params, TSubclassOf<UGameplayEffect> Effect);

	/** Adds Combat.State.Hitstun and Combat.State.Hitstop to ActivationBlockedTags: no dodging in hitstun or hitstop by default (a kit clears them) */
	void BlockInHitReaction();

private:
	/** The movement track of the timeline data this class runs, null if it has none */
	const FGSTN_TimelineTrack_Movement* FindMovementTrack() const;

	/** True once the tracker has shown the character off the ground since the timeline started (a take-off is seen a frame late) */
	UPROPERTY(Transient)
	FSyncBool bLeftGround;

	/** Simulation frame the character was first seen on the ground again after leaving it, INDEX_NONE before */
	UPROPERTY(Transient)
	FSyncInteger LandedFrame;
};

/** The default dash: a ground burst along the facing in the plane perpendicular to the local gravity. Bind to Command.Movement.Dash */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_Dash : public UGSTN_GA_MoveTimeline
{
	GENERATED_BODY()

public:
	UGSTN_GA_Dash(const FObjectInitializer& ObjectInitializer);
};

/**
 * The default ground dodge (Mario long jump / Helldivers dive arc): the character is thrown into a low arc and lands standing, no prone
 * state. The feet are invincible from take-off for 40 % of the airborne frames (a hurtbox modifier track, data). Bind to
 * Command.Defence.Dodge with a grounded requirement. Blocked in hitstun and hitstop by default.
 */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_GroundDodge : public UGSTN_GA_MoveTimeline
{
	GENERATED_BODY()

public:
	UGSTN_GA_GroundDodge(const FObjectInitializer& ObjectInitializer);
};

/**
 * The default air dodge (Mario 3D World dive roll): forward and down, ends on landing with a short landing lag. Bind to Command.Defence.Dodge
 * with an airborne requirement. Blocked in hitstun and hitstop by default.
 */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_AirDodge : public UGSTN_GA_MoveTimeline
{
	GENERATED_BODY()

public:
	UGSTN_GA_AirDodge(const FObjectInitializer& ObjectInitializer);
};

// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "Jump/GSTN_JumpTypes.h"
#include "GSTN_GA_PlatformJump.generated.h"

class UGSTN_CombatSimComponent;

/**
 * Creates Mover's jump impulse (FJumpImpulseEffect, along Mover's up direction, which follows the local gravity) with the upward
 * speed of the combat stats: JumpVelocity, or AirJumpVelocity for an air jump. A pawn whose ability system has no combat attribute set
 * gets Mover's own JumpUpwardsSpeed.
 */
UCLASS(DisplayName = "GSTN Platform Jump Move Creator")
class GSTNCOMBATNP_API UGSTN_PlatformJumpMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	/** Reads AirJumpVelocity instead of JumpVelocity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	bool bAirJump = false;

	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: queues the ground jump impulse of the stats (default JumpEffect of UGSTN_GA_PlatformJump) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_PlatformJump : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_PlatformJump();
};

/** Instant: queues the air jump impulse of the stats (default AirJumpEffect of UGSTN_GA_PlatformJump) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_PlatformAirJump : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_PlatformAirJump();
};

/** What a UGSTN_GA_PlatformJump does when its command fires */
UENUM(BlueprintType)
enum class EGSTN_PlatformJumpRole : uint8
{
	/** A ground jump, a jump inside the coyote window, or an air jump: the combat tick decides which from the movement state */
	Jump,

	/** Fast fall: the combat tick has set the fast fall flag (it is allowed in the air, while descending); the ability only owns the activation */
	FastFall
};

/**
 * The platform jump of the default kit (combat.md 11b, combat-m3 7.9): variable height by hold time, air jumps and fast fall, as a
 * gameplay ability that a kit's command bindings supply as data. Bind it to a jump command (Press of the jump button) with
 * Role Jump and, for fast fall, to a second command with a child (or the same class) with Role FastFall; changing which ability a kit
 * uses for these is a data edit.
 *
 * The framework part is not here. Who may jump (the ground, the coyote window, the air jumps left), the hold phase, the cut, the fall
 * and fast fall gravity and what an external launch does are decided by the combat tick from the Mover sim state and the stats
 * (FGSTN_JumpLogic, written into FGSTN_CombatSyncCore::Jump) and applied to Mover by the stats modifier. The ability executes what the
 * tick decided: on a take-off frame it applies the impulse effect (the ability never sets the character's state: Mover applies the
 * queued move). Activated by anything else than a command that the tick turned into a take-off (a trigger, an effect), it does nothing and ends.
 *
 * Squat frames (JumpSquatFrames) are not simulated yet: the take-off is the frame of the request.
 */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_PlatformJump : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_PlatformJump(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	EGSTN_PlatformJumpRole Role = EGSTN_PlatformJumpRole::Jump;

	/** Applied to the owner on a ground jump. It must move the character (UApplyMoveGameplayEffectComponent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	TSubclassOf<UGameplayEffect> JumpEffect;

	/** Applied to the owner on an air jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	TSubclassOf<UGameplayEffect> AirJumpEffect;

	/**
	 * Per-kit jump data (combat-m3 C15): the kit binds this class (or a Blueprint child of it) to its jump command, and the combat
	 * tick reads the data from the Jump-role binding it finds in the table. Off = the plugin settings (UGSTN_CombatSettings), the
	 * defaults of every kit. Fixed after the table is built, the same on every machine like the bindings.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Kit Data")
	bool bOverrideResetRules = false;

	/** What an external launch and the ground do to the air jumps, the coyote window and the hold phase of this kit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Kit Data", meta = (EditCondition = "bOverrideResetRules"))
	FGSTN_JumpResetRules ResetRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Kit Data")
	bool bOverrideCoyoteFrames = false;

	/** Frames after leaving the ground in which a ground jump is still allowed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump|Kit Data", meta = (EditCondition = "bOverrideCoyoteFrames", ClampMin = "0", ClampMax = "30", ForceUnits = "Frames"))
	int32 CoyoteFrames = 4;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** The fast fall of the default kit: UGSTN_GA_PlatformJump with Role FastFall, to bind to Command.Movement.FastFall */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_PlatformFastFall : public UGSTN_GA_PlatformJump
{
	GENERATED_BODY()

public:
	UGSTN_GA_PlatformFastFall(const FObjectInitializer& ObjectInitializer);
};

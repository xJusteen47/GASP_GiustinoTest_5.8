// The launch of a staggered victim (combat-m4-hits 7, M4 H9).

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "LayeredMove.h"
#include "Movement/GSTN_TumbleContact.h"
#include "GSTN_LayeredMove_Knockback.generated.h"

/**
 * The launch a hit gives its victim after the hitstop: the launch velocity as the first frame's velocity, forcing Falling (Mover honours
 * the preferred mode on the first tick of a move only), and, while the move lives, a drag on the horizontal speed and a scaled gravity. With the defaults (no decay, gravity scale 1) it is the
 * launch move and Falling does the rest.
 *
 * Stateless in the sense Network Prediction needs: every frame is derived from the move's fields, the step time and the start velocity
 * of the frame, so a rolled back Mover re-derives it. ElapsedFrames is written for display and the trace, never read back.
 *
 * Horizontal and vertical are relative to the up direction of the Mover component (minus its gravity), never world Z.
 *
 * The move is also the contact probe of a tumbling victim (combat-m4-hits 7b, M4 H11): every frame it lives it sweeps the victim's
 * collision along the velocity the frame will move with and, on a blocking hit, writes the contact into Contact (face kind, normal, speed
 * into the face, frame, GSTNSurface type index). The combat tick reads the record one frame later and decides the impact from it; the
 * move itself never reacts to a contact, so the physics of an unresolved hit is what Mover does (slide).
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_LayeredMove_Knockback : public FLayeredMoveBase
{
	GENERATED_BODY()

	FGSTN_LayeredMove_Knockback();
	virtual ~FGSTN_LayeredMove_Knockback() = default;

	/** World space launch velocity, cm/s (after DI and the victim's knockback resistance) */
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	/** Fraction of the horizontal speed lost per frame after the first, 0 = none */
	UPROPERTY()
	float DecayPerFrame = 0.f;

	/** 1 = the gravity of the movement mode; the move adds the difference as an extra acceleration */
	UPROPERTY()
	float GravityScale = 1.f;

	/** Frames since the move started, as of the last GenerateMove */
	UPROPERTY()
	int32 ElapsedFrames = 0;

	/** The mode the launch forces on the first frame */
	UPROPERTY()
	FName ForceMovementMode = TEXT("Falling");

	/** The latest contact the probe found; read by the combat tick (UGSTN_CombatSimComponent) from Mover's sync state. Part of the move, so a rollback restores it */
	UPROPERTY()
	FGSTN_TumbleContactSyncState Contact;

	/** Counters of the probe, for diagnosing a missing contact in the harness. Not state: written by every machine and every resimulation */
	struct FProbeStats
	{
		int32 Calls = 0;
		int32 NoSweep = 0;
		int32 NoHit = 0;
		int32 Penetrating = 0;
		int32 Unset = 0;
		int32 Found = 0;
		FString Last;
	};
	static FProbeStats ProbeStats;

	/** Extra distance the probe sweeps past the frame's movement, so a victim resting against a face still registers it */
	static constexpr double ProbeMarginCm = 1.0;

	virtual bool SupportsAsync() const override { return false; }
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

private:
	/** Sweeps the victim from its start-of-frame pose along the frame's velocity and writes Contact when a face blocks it */
	void ProbeContact(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, const FVector& FrameVelocity);
};

template<>
struct TStructOpsTypeTraits<FGSTN_LayeredMove_Knockback> : public TStructOpsTypeTraitsBase2<FGSTN_LayeredMove_Knockback>
{
	enum { WithCopy = true };
};

/**
 * Queues a knockback on the target's Mover once. The numbers travel as SetByCaller magnitudes of the spec (Combat.SetByCaller.Move*),
 * the lifetime as Combat.SetByCaller.MoveSeconds. Without a velocity the move does nothing.
 */
UCLASS(DisplayName = "GSTN Knockback Move Creator")
class GSTNCOMBATNP_API UGSTN_KnockbackMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: launches the target with the SetByCaller velocity (the request the combat tick makes when a launched victim's hitstop ends) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_Knockback : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_Knockback();
};

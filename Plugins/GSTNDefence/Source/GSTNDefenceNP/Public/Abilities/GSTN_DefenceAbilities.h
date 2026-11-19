// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_DefenceAbilities.generated.h"

/**
 * The tech ability (m6 spec 6, a pluggable kit ability): one class per tech kind in the kit's FGSTN_TechSet, started Forced by the model
 * at a successful tech (F8). A thin timeline ability: its frame data (the tech's intangible window, its movement) is data of the class or
 * its Blueprint child; nothing here reads or writes the simulation.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_Tech : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_Tech(const FObjectInitializer& ObjectInitializer);

	/** Length of the timeline (F8 default 20); the class or its Blueprint child sets its own, and its tracks (movement, intangibility) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Timeline", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TotalFrames = 20;

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * The mini ground bounce of a landing from tumble (m6 spec 6, owner revision): started Forced by the model at the contact (F8), with
 * the stick-steered bounce applied by the model through the reaction seam, never by the ability. Thin: frame data only.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_GroundBounce : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_GroundBounce(const FObjectInitializer& ObjectInitializer);

	/** Length of the timeline (F8 default 24); the class or its Blueprint child sets its own, and its tracks (movement, intangibility) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Timeline", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TotalFrames = 24;

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/** Which way a get-up roll travels: the clips are authored in place, the ability moves the capsule */
UENUM(BlueprintType)
enum class EGSTN_GetupTravelDirection : uint8
{
	/** Toward the feet (the forward rock-up) */
	Facing,
	/** Toward the head (the roll back over the shoulder) */
	AgainstFacing,
};

/** One point of a get-up roll's travel: at this frame of the timeline the capsule has covered Fraction of TravelDistanceCm */
USTRUCT(BlueprintType)
struct GSTNDEFENCENP_API FGSTN_GetupTravelKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 Frame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Fraction = 0.f;
};

/**
 * The get-up roll of a downed entity (m6 spec 6): started Forced by the model from the stick (F8); the kit's RollBack class instead when the
 * stick points against the facing. The travel follows the clip (docs/notes/reviews/react-v3/roll-travel.md): TravelKeys are a piecewise-linear
 * distance curve, so each segment between two keys is one push at constant speed over its frames, queued on the segment's first frame. No
 * key or no distance: the roll stays in place, as before.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_GetupRoll : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_GetupRoll(const FObjectInitializer& ObjectInitializer);

	/** Length of the timeline (F8 default 30); the class or its Blueprint child sets its own, and its tracks (movement, intangibility) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Timeline", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TotalFrames = 30;

	/** Distance the roll covers on the ground; 0 = in place */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Travel", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float TravelDistanceCm = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Travel")
	EGSTN_GetupTravelDirection TravelDirection = EGSTN_GetupTravelDirection::Facing;

	/** Ascending frames (simulation frames, 62.5 per second), fractions from 0 to 1. Holds before the first key and after the last */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Travel")
	TArray<FGSTN_GetupTravelKey> TravelKeys;

	/**
	 * The push that starts at timeline frame Frame: true with its speed (cm/s along the travel direction) and length in frames when a segment
	 * with travel starts there. Pure, for the ability and the tests
	 */
	static bool SegmentStartingAt(const TArray<FGSTN_GetupTravelKey>& Keys, float DistanceCm, int32 Frame, float& OutSpeed, int32& OutFrames);

	virtual void PostInitProperties() override;
	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnTimelineBegan(int32 StateStartFrame) override;

private:
	/** The last timeline frame whose segment start was handled (sync var: a resimulation queues the same pushes again) */
	UPROPERTY(Transient)
	FSyncInteger LastTravelFrame;
};

/**
 * The neutral get-up (m6 spec 6): the one get-up a command starts, bound to Command.Movement.Jump while the entity is downed
 * (UGSTN_DefenceLibrary::GetupNeutralRequirements, Combat.State.Knockdown required). Thin: frame data only.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_GetupNeutral : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_GetupNeutral(const FObjectInitializer& ObjectInitializer);

	/** Length of the timeline (F8 default 20); the class or its Blueprint child sets its own, and its tracks (movement, intangibility) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Timeline", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TotalFrames = 20;

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * A counter stance as content (m6 spec 5.4, source Move): a timeline with one FGSTN_ParryWindowTrack over [ParryStartFrame, ParryEndFrame)
 * (profile ProfileName, closes on a parry) and an OnParry cancel window from the window's start to the end into AttackCommands, so a
 * parried hit lets the counter-attack start at once. Works in every mode, with or without a shield. The sample the harness binds
 * (scenario_parry_move) and a starting point for a kit's counter: a Blueprint child sets the frames. Built on the class default object
 * like the shield's timeline; never writes sync state.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_CounterSample : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_CounterSample(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Counter", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ParryStartFrame = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Counter", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 ParryEndFrame = 14;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Counter", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TotalFrames = 30;

	/** A name of the rules' ParryProfiles (profile 0, the shield's, when the rules have no such name) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Counter")
	FName ProfileName = TEXT("Counter");

	/** The commands the OnParry window cancels into */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Counter")
	FGameplayTagContainer AttackCommands;

	void RebuildTimeline();
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

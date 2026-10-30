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

/**
 * The get-up roll of a downed entity (m6 spec 6): started Forced by the model from the stick (F8). Thin: frame data only (the roll's
 * movement track and intangible window are data of the class or its child).
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

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
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

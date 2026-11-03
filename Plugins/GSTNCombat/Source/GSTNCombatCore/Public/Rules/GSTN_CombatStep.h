// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "GSTN_CombatStep.generated.h"

/** What a pipeline step decided about the candidate */
UENUM(BlueprintType)
enum class EGSTN_CombatStepVerdict : uint8
{
	/** Pass the candidate to the next step; after the last step it goes on to be resolved and executed */
	Continue,
	/** Drop the candidate: no later step runs and nothing is activated */
	Reject
};

/**
 * What a step sees of an activation candidate (combat-m3 7.2, steps 5 to 7). Plain values of the frame being simulated,
 * so a step is a pure function of it. Extend it with new fields rather than by giving steps access to the simulation.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_CombatStepView
{
	GENERATED_BODY()

	/** Network Prediction frame being simulated */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step")
	int32 Frame = 0;

	/** Command tag of the candidate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** Index of the candidate's binding in the compiled command table, INDEX_NONE when the candidate has none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step")
	int32 BindingIndex = INDEX_NONE;

	/** The owner may start a move now (not locked, not in the unskippable part of a timeline) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step")
	bool bActionable = true;

	/** A timeline is running, so the candidate would replace or cancel it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step")
	bool bInTimeline = false;
};

/** What a step returns */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_CombatStepResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	EGSTN_CombatStepVerdict Verdict = EGSTN_CombatStepVerdict::Continue;

	/** Why a candidate was rejected, for logs and the debug display. Not simulated state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	FGameplayTag Reason;
};

/**
 * One step of the activation pipeline of UGSTN_CombatRules. The order of the phases (candidate, resolve, execute) is fixed
 * in code; the steps decide, in the order the rules asset lists them, whether the candidate goes on.
 *
 * A step runs inside the simulation, on every machine and again on every re-simulation, so it must be a pure function of its
 * view: no world, actor or component reads, no latent nodes or timers, no state kept between calls (the step object is the
 * class default object, shared by every pawn), seeded random only. A Blueprint step that breaks this desynchronises the
 * match; the same holds for C++ steps.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ShowWorldContextPin = "false"))
class GSTNCOMBATCORE_API UGSTN_CombatStep : public UObject
{
	GENERATED_BODY()

public:
	/** The one entry point. The default passes the candidate on unchanged */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Step")
	FGSTN_CombatStepResult Evaluate(const FGSTN_CombatStepView& View) const;
	virtual FGSTN_CombatStepResult Evaluate_Implementation(const FGSTN_CombatStepView& View) const;
};

/**
 * The built-in activation step. It is a pass-through that stands for the activation checks of the resolver (not locked, a
 * binding holds, the ability can be activated), which stay in code, and it documents the contract a custom step follows:
 * read the view, return Continue or Reject. It is the only step in the default rules, so a game extends the activation
 * by adding steps before or after it.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_CombatStep_Activation : public UGSTN_CombatStep
{
	GENERATED_BODY()

public:
	virtual FGSTN_CombatStepResult Evaluate_Implementation(const FGSTN_CombatStepView& View) const override;
};

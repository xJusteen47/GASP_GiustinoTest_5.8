// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_ReactionTypes.h"

class UGSTN_KnockbackModel;

/** One finding of the reaction validation */
struct GSTNCOMBATCORE_API FGSTN_ReactionMessage
{
	bool bError = false;
	FString Text;
};

/**
 * Pure checks of the reaction data (combat-m4-hits 3, 10). Errors make the data invalid; warnings are for the designer.
 */
struct GSTNCOMBATCORE_API FGSTN_ReactionValidation
{
	/**
	 *  - PushBackThreshold <= LaunchThreshold <= TumbleThreshold (error)
	 *  - a non positive WeightReference or MinWeight (error): the weight driven model would divide by zero
	 *  - MaxHitstop 0 (warning): no hit freezes anyone
	 */
	static void ValidateTuning(const FGSTN_ReactionTuning& Tuning, TArray<FGSTN_ReactionMessage>& OutMessages);

	/**
	 *  - a curve with a sample count other than 64 and other than 0 (error)
	 *  - a model that counts the victim's percent (PercentScaling, CappedPercent) next to an authored, non flat weight curve
	 *    (warning): the damage would count twice. Only the weight driven model should meet a health weight curve.
	 */
	static void ValidateModelWithCurve(const UGSTN_KnockbackModel* Model, const FGSTN_BakedCurve& HealthWeightCurve, TArray<FGSTN_ReactionMessage>& OutMessages);
};

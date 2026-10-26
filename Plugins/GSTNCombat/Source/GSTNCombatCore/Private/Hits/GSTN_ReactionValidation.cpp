// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_ReactionValidation.h"
#include "Hits/GSTN_KnockbackModel.h"

void FGSTN_ReactionValidation::ValidateTuning(const FGSTN_ReactionTuning& Tuning, TArray<FGSTN_ReactionMessage>& OutMessages)
{
	auto Add = [&OutMessages](bool bError, const FString& Text)
	{
		FGSTN_ReactionMessage& Message = OutMessages.AddDefaulted_GetRef();
		Message.bError = bError;
		Message.Text = Text;
	};

	if (Tuning.PushBackThreshold > Tuning.LaunchThreshold)
	{
		Add(true, FString::Printf(TEXT("PushBackThreshold %.1f is above LaunchThreshold %.1f: the push back tier would be empty"), Tuning.PushBackThreshold, Tuning.LaunchThreshold));
	}
	if (Tuning.LaunchThreshold > Tuning.TumbleThreshold)
	{
		Add(true, FString::Printf(TEXT("LaunchThreshold %.1f is above TumbleThreshold %.1f: a launch could never tumble"), Tuning.LaunchThreshold, Tuning.TumbleThreshold));
	}
	if (Tuning.WeightReference <= 0.f || Tuning.MinWeight <= 0.f)
	{
		Add(true, TEXT("WeightReference and MinWeight must be positive: the weight driven knockback divides by the weight"));
	}
	if (Tuning.MaxHitstop == 0)
	{
		Add(false, TEXT("MaxHitstop is 0: no hit freezes anyone"));
	}
}

void FGSTN_ReactionValidation::ValidateModelWithCurve(const UGSTN_KnockbackModel* Model, const FGSTN_BakedCurve& HealthWeightCurve, TArray<FGSTN_ReactionMessage>& OutMessages)
{
	if (HealthWeightCurve.Samples.Num() != 0 && !HealthWeightCurve.IsAuthored())
	{
		FGSTN_ReactionMessage& Message = OutMessages.AddDefaulted_GetRef();
		Message.bError = true;
		Message.Text = FString::Printf(TEXT("A baked curve needs %d samples, this one has %d"), FGSTN_BakedCurve::SampleCount, HealthWeightCurve.Samples.Num());
		return;
	}
	if (Model && Model->UsesPercentTerm() && HealthWeightCurve.IsAuthored() && !HealthWeightCurve.IsFlat())
	{
		FGSTN_ReactionMessage& Message = OutMessages.AddDefaulted_GetRef();
		Message.bError = false;
		Message.Text = TEXT("The knockback model counts the damage percent itself and the health weight curve is not flat: the damage taken counts twice. Use a flat curve, or the weight driven model");
	}
}

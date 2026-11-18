// Ability-system layer of the combat framework: the presentation of a hitstop.

#include "Cosmetics/GSTN_HitstopFeel.h"
#include "GSTN_CombatTags.h"

TFunction<float(const AActor*)> FGSTN_HitstopPresentationHooks::GetHitstopSecondsLeft;
TFunction<bool(const AActor*, const UObject*&, float&)> FGSTN_HitstopPresentationHooks::GetPresentedMontageTime;
TFunction<void(const AActor*, float)> FGSTN_HitstopPresentationHooks::SetMontagePresentationLead;

FGSTN_HitstopAdvance::FGSTN_HitstopAdvance()
	: MoveTag(GSTNCombatTags::Feel_HitstopAdvance)
{
}

bool FGSTN_HitstopAdvance::AppliesTo(const FGameplayTagContainer& OwnerTags) const
{
	switch (Mode)
	{
	case EGSTN_HitstopAdvanceMode::AllMoves:
		return AdvanceFrames > 0.f;
	case EGSTN_HitstopAdvanceMode::TaggedMoves:
		return AdvanceFrames > 0.f && MoveTag.IsValid() && OwnerTags.HasTag(MoveTag);
	default:
		return false;
	}
}

FVector2D FGSTN_HitstopFeelMath::AirShakeAxis(float FrequencyHz, float ElapsedSeconds)
{
	// The golden angle: consecutive axes never line up, and a handful of cycles spread over the whole circle
	constexpr double GoldenAngleRad = 2.399963229728653;
	const int32 Cycle = FMath::Max(FMath::FloorToInt(FMath::Max(FrequencyHz, 0.f) * FMath::Max(ElapsedSeconds, 0.f)), 0);
	const double Angle = GoldenAngleRad * static_cast<double>(Cycle);
	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
}

FVector FGSTN_HitstopFeelMath::GroundShakeDirection(const FVector& ScreenRight, const FVector& OwnerUp)
{
	const FVector Up = OwnerUp.GetSafeNormal();
	const FVector Flat = Up.IsZero() ? ScreenRight : ScreenRight - Up * FVector::DotProduct(ScreenRight, Up);
	// A camera looking straight down the up axis has no flat screen right left: no shake rather than a vertical one
	return Flat.GetSafeNormal();
}

float FGSTN_HitstopFeelMath::AdvanceLead(float MaxLeadSeconds, float Alpha)
{
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	const float Inverse = 1.f - A;
	return FMath::Max(MaxLeadSeconds, 0.f) * (1.f - Inverse * Inverse);
}

float FGSTN_HitstopFeelMath::AbsorbLead(float PeakTime, float ShownTime)
{
	return FMath::Max(PeakTime - ShownTime, 0.f);
}

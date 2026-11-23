// Ability-system layer of the combat framework.

#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "Animation/AnimSequenceBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool FGSTN_ReactionAnimRule::Matches(const FGameplayTagContainer& OwnerTags) const
{
	return !RequiredTags.IsEmpty() && OwnerTags.HasAll(RequiredTags) && !OwnerTags.HasAny(BlockedTags);
}

int32 UGSTN_ReactionAnimSet::SelectRule(const FGameplayTagContainer& OwnerTags) const
{
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		// Strictly greater: of equal priorities the first listed stays
		if (Rules[Index].Matches(OwnerTags) && (Best == INDEX_NONE || Rules[Index].Priority > Rules[Best].Priority))
		{
			Best = Index;
		}
	}
	return Best;
}

FPrimaryAssetId UGSTN_ReactionAnimSet::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GSTN_ReactionAnimSet"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_ReactionAnimSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		const FGSTN_ReactionAnimRule& Rule = Rules[Index];
		if (Rule.RequiredTags.IsEmpty())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Reaction rule %d (%s) has no required tag: it would never play."), Index, *Rule.Name.ToString())));
			Result = EDataValidationResult::Invalid;
		}
		if (Rule.Animation.IsNull())
		{
			Context.AddWarning(FText::FromString(FString::Printf(TEXT("Reaction rule %d (%s) has no animation: it shows the character's own pose."), Index, *Rule.Name.ToString())));
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

float FGSTN_HitstopShake::Sample(float AmplitudeCm, float FrequencyHz, float DecaySeconds, float ElapsedSeconds)
{
	if (AmplitudeCm <= 0.f || ElapsedSeconds < 0.f || DecaySeconds <= 0.f || ElapsedSeconds >= DecaySeconds)
	{
		return 0.f;
	}
	const float Fade = 1.f - ElapsedSeconds / DecaySeconds;
	return AmplitudeCm * Fade * FMath::Cos(UE_TWO_PI * FrequencyHz * ElapsedSeconds);
}

float FGSTN_HitstopShake::DistanceScale(float CameraDistanceCm) const
{
	const float Reference = FMath::Max(ReferenceCameraDistanceCm, 1.f);
	return FMath::Clamp(CameraDistanceCm / Reference, 1.f, FMath::Max(MaxDistanceScale, 1.f));
}

const TSoftObjectPtr<UAnimSequenceBase>& FGSTN_DirectionalHitAdditives::ForSide(EGSTN_HitSide Side) const
{
	switch (Side)
	{
	case EGSTN_HitSide::Back: return Back;
	case EGSTN_HitSide::Left: return Left;
	case EGSTN_HitSide::Right: return Right;
	default: return Front;
	}
}

EGSTN_HitSide FGSTN_DirectionalHitMath::SelectSide(const FVector& PushDirection, const FVector& VictimForward, const FVector& VictimRight)
{
	// The hit comes from the opposite of the push: a push along -Forward is a hit on the front
	const FVector From = -FVector(PushDirection.X, PushDirection.Y, 0.0).GetSafeNormal();
	const double F = FVector::DotProduct(From, FVector(VictimForward.X, VictimForward.Y, 0.0).GetSafeNormal());
	const double R = FVector::DotProduct(From, FVector(VictimRight.X, VictimRight.Y, 0.0).GetSafeNormal());
	if (FMath::Abs(F) >= FMath::Abs(R))
	{
		return F >= 0.0 ? EGSTN_HitSide::Front : EGSTN_HitSide::Back;
	}
	return R > 0.0 ? EGSTN_HitSide::Right : EGSTN_HitSide::Left;
}

FVector FGSTN_DirectionalHitMath::SidePush(EGSTN_HitSide Side, const FVector& VictimForward, const FVector& VictimRight)
{
	const FVector Forward = FVector(VictimForward.X, VictimForward.Y, 0.0).GetSafeNormal();
	const FVector Right = FVector(VictimRight.X, VictimRight.Y, 0.0).GetSafeNormal();
	switch (Side)
	{
	case EGSTN_HitSide::Back: return Forward;
	case EGSTN_HitSide::Left: return Right;
	case EGSTN_HitSide::Right: return -Right;
	default: return -Forward;
	}
}

float FGSTN_DirectionalHitMath::IntensityWeight(bool bStaggered, int32 HitstunFrames, int32 FullFrames, float MinWeight, float UnstaggeredWeight)
{
	if (!bStaggered)
	{
		return FMath::Clamp(UnstaggeredWeight, 0.f, 1.f);
	}
	const float Alpha = FMath::Clamp(static_cast<float>(HitstunFrames) / static_cast<float>(FMath::Max(FullFrames, 1)), 0.f, 1.f);
	return FMath::Lerp(FMath::Clamp(MinWeight, 0.f, 1.f), 1.f, Alpha);
}

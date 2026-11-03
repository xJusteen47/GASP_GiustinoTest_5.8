// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceTypes.h"

#include "GSTN_SurfaceTags.h"

#define LOCTEXT_NAMESPACE "GSTNSurface"

bool FGSTN_SurfaceLaunchParams::Validate(TArray<FText>& OutErrors) const
{
	const int32 ErrorsBefore = OutErrors.Num();

	// ContainsNaN is true for infinite components too
	if (LocalDirection.ContainsNaN() || !FMath::IsFinite(TiltPitch) || !FMath::IsFinite(TiltYaw)
		|| !FMath::IsFinite(Speed) || !FMath::IsFinite(Restitution) || !FMath::IsFinite(MinSpeed) || !FMath::IsFinite(MaxSpeed))
	{
		OutErrors.Add(LOCTEXT("LaunchNotFinite", "Launch: a value is not a finite number"));
	}
	if (DirectionMode == EGSTN_SurfaceLaunchDirection::LocalDirection && LocalDirection.IsNearlyZero())
	{
		OutErrors.Add(LOCTEXT("LaunchZeroDirection", "Launch: LocalDirection is zero, so the launch has no direction"));
	}
	if (DirectionMode == EGSTN_SurfaceLaunchDirection::Tilt && (TiltPitch < 0.f || TiltPitch >= 90.f))
	{
		OutErrors.Add(LOCTEXT("LaunchTiltRange", "Launch: TiltPitch must be in [0, 90) degrees, or the launch goes into the surface"));
	}
	if (SpeedMode == EGSTN_SurfaceLaunchSpeed::Fixed && Speed < 0.f)
	{
		OutErrors.Add(LOCTEXT("LaunchNegativeSpeed", "Launch: Speed is negative"));
	}
	if (SpeedMode == EGSTN_SurfaceLaunchSpeed::ScaledByIncoming)
	{
		if (Restitution < 0.f || MinSpeed < 0.f || MaxSpeed < 0.f)
		{
			OutErrors.Add(LOCTEXT("LaunchNegativeScaled", "Launch: Restitution, MinSpeed and MaxSpeed must not be negative"));
		}
		if (MinSpeed > MaxSpeed)
		{
			OutErrors.Add(LOCTEXT("LaunchInvertedRange", "Launch: MinSpeed is above MaxSpeed"));
		}
	}

	return OutErrors.Num() == ErrorsBefore;
}

bool FGSTN_SurfaceBehaviour::Validate(TArray<FText>& OutErrors) const
{
	const int32 ErrorsBefore = OutErrors.Num();

	for (const FGameplayTag& Tag : Behaviours)
	{
		if (!Tag.MatchesTag(GSTNSurfaceTags::Root_Behaviour))
		{
			OutErrors.Add(FText::Format(LOCTEXT("BehaviourOutsideRoot", "Behaviours: {0} is not a Surface.Behaviour tag"),
				FText::FromName(Tag.GetTagName())));
		}
	}
	if (SurfaceType.IsValid() && !SurfaceType.MatchesTag(GSTNSurfaceTags::Root_Type))
	{
		OutErrors.Add(FText::Format(LOCTEXT("TypeOutsideRoot", "SurfaceType: {0} is not a Surface.Type tag"),
			FText::FromName(SurfaceType.GetTagName())));
	}
	if (HasBehaviour(GSTNSurfaceTags::Behaviour_Launch))
	{
		Launch.Validate(OutErrors);
	}

	return OutErrors.Num() == ErrorsBefore;
}

#undef LOCTEXT_NAMESPACE

// Defence, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_DefenceRules.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	bool IsFiniteNonNegative(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.f;
	}

	void CheckScaleMap(const FString& Label, const TMap<FGameplayTag, float>& Map, TArray<FString>& OutErrors)
	{
		for (const TPair<FGameplayTag, float>& Pair : Map)
		{
			if (!Pair.Key.IsValid())
			{
				OutErrors.Add(FString::Printf(TEXT("%s has an entry with no tag."), *Label));
			}
			if (!IsFiniteNonNegative(Pair.Value))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: the scale of %s must be finite and not negative."), *Label, *Pair.Key.ToString()));
			}
		}
	}
}

UGSTN_DefenceRules::UGSTN_DefenceRules()
{
	// Profile 0 is the shield's and always exists, so a fresh asset is valid
	ParryProfiles.Add(FGSTN_ParryProfile());
}

int32 UGSTN_DefenceRules::FindProfileIndex(FName ProfileName) const
{
	if (ProfileName.IsNone())
	{
		return 0;
	}
	for (int32 Index = 0; Index < ParryProfiles.Num(); ++Index)
	{
		if (ParryProfiles[Index].Name == ProfileName)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UGSTN_DefenceRules::Validate(TArray<FString>& OutErrors) const
{
	// ---- Parry profiles ----
	if (ParryProfiles.IsEmpty())
	{
		OutErrors.Add(TEXT("The rules have no parry profile; profile 0 is the shield's and must exist, or no parry could be resolved."));
	}
	if (ParryProfiles.Num() > GSTNDefenceLimits::MaxParryProfiles)
	{
		OutErrors.Add(FString::Printf(TEXT("The rules have %d parry profiles; the limit is %d (the index travels in the snapshot)."), ParryProfiles.Num(), GSTNDefenceLimits::MaxParryProfiles));
	}
	TSet<FName> SeenNames;
	for (int32 Index = 0; Index < ParryProfiles.Num(); ++Index)
	{
		const FGSTN_ParryProfile& Profile = ParryProfiles[Index];
		const FString Label = FString::Printf(TEXT("Parry profile %d"), Index);
		if (Profile.Name.IsNone())
		{
			if (Index != 0)
			{
				OutErrors.Add(FString::Printf(TEXT("%s has no name; only profile 0 may be unnamed, a parry window track finds the others by name."), *Label));
			}
		}
		else
		{
			bool bAlreadySeen = false;
			SeenNames.Add(Profile.Name, &bAlreadySeen);
			if (bAlreadySeen)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: the name %s is used by another profile; names must be unique."), *Label, *Profile.Name.ToString()));
			}
		}
		if (Profile.HitstopFrames < 0 || Profile.DefenderRecoveryFrames < 0 || Profile.AttackerStunFrames < 0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: the frame counts must not be negative."), *Label));
		}
		if (!IsFiniteNonNegative(Profile.MeterGain))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: the meter gain must be finite and not negative."), *Label));
		}
		for (const TPair<FGameplayTag, int32>& Pair : Profile.AttackerStunFramesByHitClass)
		{
			if (!Pair.Key.IsValid() || Pair.Value < 0)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: an attacker stun entry has no tag or a negative frame count."), *Label));
			}
		}
	}
	if (Parry.ProfileIndex < 0 || Parry.ProfileIndex >= ParryProfiles.Num())
	{
		OutErrors.Add(FString::Printf(TEXT("The shield's parry names profile %d, which does not exist."), Parry.ProfileIndex));
	}
	if (Parry.WindowFrames < 0 || Parry.MinHeldFramesBeforeRelease < 0)
	{
		OutErrors.Add(TEXT("The shield's parry window and minimum held frames must not be negative."));
	}

	// ---- Shield ----
	if (bShieldEnabled && !(FMath::IsFinite(Shield.RadiusCm) && Shield.RadiusCm > 0.f))
	{
		OutErrors.Add(TEXT("The shield is enabled but its bubble radius is not a positive number."));
	}
	if (!IsFiniteNonNegative(Shield.RadiusCm) || !IsFiniteNonNegative(Shield.CentreHeightCm))
	{
		OutErrors.Add(TEXT("The shield bubble radius and centre height must be finite and not negative."));
	}
	if (!(FMath::IsFinite(Shield.MaxShield) && Shield.MaxShield > 0.f))
	{
		OutErrors.Add(TEXT("The maximum shield health must be a positive number."));
	}
	if (!IsFiniteNonNegative(Shield.DrainPerFrame) || !IsFiniteNonNegative(Shield.RegenPerFrame) || !IsFiniteNonNegative(Shield.ShieldDamagePerDamage))
	{
		OutErrors.Add(TEXT("The shield drain, regeneration and damage per damage must be finite and not negative."));
	}
	if (!(FMath::IsFinite(Shield.RecoverFractionAfterBreak) && Shield.RecoverFractionAfterBreak >= 0.f && Shield.RecoverFractionAfterBreak <= 1.f))
	{
		OutErrors.Add(TEXT("The shield recover fraction after a break must be between 0 and 1."));
	}
	if (Shield.StartupFrames < 0 || Shield.ReleaseFrames < 0 || Shield.RegenDelayFrames < 0 || Shield.BreakStunFrames < 0
		|| Shield.ShieldStunBase < 0 || Shield.MaxShieldStun < 0 || Shield.PushbackFrames < 0)
	{
		OutErrors.Add(TEXT("The shield frame counts must not be negative."));
	}
	if (Shield.MaxShieldStun < Shield.ShieldStunBase)
	{
		OutErrors.Add(TEXT("The shield stun limit is below the base shield stun, so the base would never apply."));
	}
	if (!IsFiniteNonNegative(Shield.BreakLaunch) || !IsFiniteNonNegative(Shield.ShieldStunPerDamage) || !IsFiniteNonNegative(Shield.ShieldHitstopMultiplier)
		|| !IsFiniteNonNegative(Shield.PushbackSpeed) || !IsFiniteNonNegative(Shield.PushbackSpeedPerDamage))
	{
		OutErrors.Add(TEXT("The shield launch, stun, hitstop and pushback values must be finite and not negative."));
	}
	CheckScaleMap(TEXT("The shield damage scale by relationship"), Shield.IncomingShieldDamageScaleByRelationship, OutErrors);

	// ---- Tech ----
	if (Tech.WindowFrames < 0 || Tech.LockoutFrames < 0 || Tech.LateFrames < 0 || Tech.InPlaceFrames < 0 || Tech.RollFrames < 0
		|| Tech.WallTechFrames < 0 || Tech.KnockdownFrames < 0 || Tech.NoTechKnockdownBonusFrames < 0 || Tech.IntangibleFrames < 0)
	{
		OutErrors.Add(TEXT("The tech frame counts must not be negative."));
	}
	if (!IsFiniteNonNegative(Tech.RollDistance))
	{
		OutErrors.Add(TEXT("The tech roll distance must be finite and not negative."));
	}

	// ---- DI ----
	DI.StrengthCurve.Validate(TEXT("The DI strength curve"), OutErrors);
	DI.ElevationDeltaByElevation.Validate(TEXT("The DI elevation curve"), OutErrors);
	if (!IsFiniteNonNegative(DI.MinKnockbackForDI))
	{
		OutErrors.Add(TEXT("The minimum knockback for DI must be finite and not negative."));
	}
	CheckScaleMap(TEXT("The DI scale by victim tag"), DI.ScaleByVictimTag, OutErrors);
	if (!(DI.Params.MaxDIYawDegrees >= 0.f && DI.Params.MaxDIYawDegrees <= 90.f) || !(DI.Params.MaxDIElevationDot >= 0.f && DI.Params.MaxDIElevationDot <= 1.f)
		|| !(DI.Params.MaxElevationDot >= 0.f && DI.Params.MaxElevationDot <= 1.f) || !(DI.Params.UpwardThresholdDot >= -1.f && DI.Params.UpwardThresholdDot <= 1.f))
	{
		OutErrors.Add(TEXT("The DI parameters are outside their ranges (yaw 0..90, dots 0..1, upward threshold -1..1)."));
	}

	// ---- SDI ----
	SDI.StrengthCurve.Validate(TEXT("The SDI strength curve"), OutErrors);
	if (!IsFiniteNonNegative(SDI.DriftSpeedCmPerSec) || !IsFiniteNonNegative(SDI.MaxDistanceCmPerHit))
	{
		OutErrors.Add(TEXT("The SDI drift speed and maximum distance must be finite and not negative."));
	}
	if (!(SDI.MinZone >= 0.f && SDI.MinZone <= 1.f))
	{
		OutErrors.Add(TEXT("The SDI minimum zone must be between 0 and 1."));
	}
	CheckScaleMap(TEXT("The SDI scale by victim tag"), SDI.ScaleByVictimTag, OutErrors);

	// ---- Landing ----
	if (!IsFiniteNonNegative(GroundBounce.GroundBounceSpeed) || !(GroundBounce.GroundBounceSteerDeg >= 0.f && GroundBounce.GroundBounceSteerDeg <= 90.f))
	{
		OutErrors.Add(TEXT("The ground bounce speed must be finite and not negative, and its steer angle between 0 and 90 degrees."));
	}
	if (GroundBounce.MaxDownedFrames < 0)
	{
		OutErrors.Add(TEXT("The maximum downed frames must not be negative (0 = never)."));
	}
}

#if WITH_EDITOR
EDataValidationResult UGSTN_DefenceRules::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FString> Errors;
	Validate(Errors);
	for (const FString& Error : Errors)
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

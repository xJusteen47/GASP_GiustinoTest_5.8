// Defence, pure part (no GAS, no Network Prediction, no Mover).

#include "Math/GSTN_DefenceMath.h"

#include "Data/GSTN_DefenceRules.h"
#include "Sync/GSTN_SyncFragment.h"

namespace
{
	/** Round half up, for the conversions of a real to frames */
	int32 RoundHalfUp(double Value)
	{
		return static_cast<int32>(FMath::FloorToInt64(Value + 0.5));
	}

	double ClampShield(double Value, double Max)
	{
		return FMath::Clamp(Value, 0.0, FMath::Max(0.0, Max));
	}

	int32 TagDepth(const FGameplayTag& Tag)
	{
		int32 Depth = 0;
		for (const TCHAR Char : Tag.ToString())
		{
			Depth += Char == TEXT('.') ? 1 : 0;
		}
		return Depth;
	}

	bool IsProfileUsable(const UGSTN_DefenceRules& Rules, int32 Index, bool bProjectile)
	{
		if (!Rules.ParryProfiles.IsValidIndex(Index))
		{
			return false;
		}
		return !bProjectile || Rules.ParryProfiles[Index].bParryableProjectiles;
	}
}

FGSTN_ShieldRates FGSTN_ShieldRates::FromRules(const FGSTN_ShieldRules& Rules)
{
	FGSTN_ShieldRates Rates;
	Rates.Drain = Rules.DrainPerFrame;
	Rates.Regen = Rules.RegenPerFrame;
	Rates.Max = Rules.MaxShield;
	Rates.RegenDelayFrames = Rules.RegenDelayFrames;
	return Rates;
}

bool UGSTN_DefenceMath::IsShieldAvailable(const UGSTN_DefenceRules* Rules, bool bKitHasShield)
{
	return Rules != nullptr && Rules->bShieldEnabled && bKitHasShield;
}

double UGSTN_DefenceMath::ShieldValueAt(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates)
{
	const double Stamped = ClampShield(View.Value, Rates.Max);
	if (Frame <= View.StampFrame)
	{
		return Stamped;
	}

	if (View.bHeld)
	{
		return ClampShield(Stamped - Rates.Drain * static_cast<double>(Frame - View.StampFrame), Rates.Max);
	}

	const int32 RegenStart = View.ReleaseFrame == INDEX_NONE
		? View.StampFrame + Rates.RegenDelayFrames
		: FMath::Max(View.ReleaseFrame + Rates.RegenDelayFrames, View.StampFrame);
	if (Frame <= RegenStart)
	{
		return Stamped;
	}
	return ClampShield(Stamped + Rates.Regen * static_cast<double>(Frame - RegenStart), Rates.Max);
}

FGSTN_ShieldView UGSTN_DefenceMath::RestampShield(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates)
{
	FGSTN_ShieldView Result = View;
	Result.Value = ShieldValueAt(View, Frame, Rates);
	Result.StampFrame = Frame;
	return Result;
}

FGSTN_ShieldView UGSTN_DefenceMath::RaiseShield(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates)
{
	FGSTN_ShieldView Result = RestampShield(View, Frame, Rates);
	Result.bHeld = true;
	Result.PressFrame = Frame;
	Result.ReleaseFrame = INDEX_NONE;
	return Result;
}

FGSTN_ShieldView UGSTN_DefenceMath::ReleaseShield(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates)
{
	FGSTN_ShieldView Result = RestampShield(View, Frame, Rates);
	Result.bHeld = false;
	Result.ReleaseFrame = Frame;
	return Result;
}

FGSTN_ShieldView UGSTN_DefenceMath::ApplyShieldDamage(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates, double Damage)
{
	FGSTN_ShieldView Result = RestampShield(View, Frame, Rates);
	Result.Value = ClampShield(Result.Value - FMath::Max(0.0, Damage), Rates.Max);
	return Result;
}

double UGSTN_DefenceMath::ShieldDamageOf(double HitShieldDamage, double HitDamage, const FGSTN_ShieldRules& Rules, double VictimMultiplier, double RelationshipScale)
{
	const double Base = HitShieldDamage > 0.0 ? HitShieldDamage : FMath::Max(0.0, HitDamage) * Rules.ShieldDamagePerDamage;
	return Base * FMath::Max(0.0, VictimMultiplier) * FMath::Max(0.0, RelationshipScale);
}

double UGSTN_DefenceMath::RelationshipScaleOf(const FGSTN_ShieldRules& Rules, FGameplayTag Relationship)
{
	if (!Relationship.IsValid())
	{
		return 1.0;
	}
	if (const float* Exact = Rules.IncomingShieldDamageScaleByRelationship.Find(Relationship))
	{
		return *Exact;
	}

	double Best = 1.0;
	int32 BestDepth = -1;
	for (const TPair<FGameplayTag, float>& Pair : Rules.IncomingShieldDamageScaleByRelationship)
	{
		if (Pair.Key.IsValid() && Relationship.MatchesTag(Pair.Key))
		{
			const int32 Depth = TagDepth(Pair.Key);
			if (Depth > BestDepth)
			{
				BestDepth = Depth;
				Best = Pair.Value;
			}
		}
	}
	return Best;
}

FVector UGSTN_DefenceMath::ShieldCentreAt(const FGSTN_ShieldRules& Rules, const FVector& EntityUp)
{
	return EntityUp.GetSafeNormal() * static_cast<double>(Rules.CentreHeightCm);
}

FGSTN_ShieldBubble UGSTN_DefenceMath::MakeShieldBubble(const FGSTN_ShieldRules& Rules, const FTransform& EntityTransform)
{
	FGSTN_ShieldBubble Bubble;
	Bubble.Centre = EntityTransform.GetLocation() + ShieldCentreAt(Rules, EntityTransform.GetUnitAxis(EAxis::Z));
	Bubble.Radius = Rules.RadiusCm;
	return Bubble;
}

FGSTN_ExtraHurtbox UGSTN_DefenceMath::MakeShieldBubbleHurtbox(const FGSTN_ShieldRules& Rules, const FGameplayTagContainer& IgnoreHitTypes)
{
	FGSTN_ExtraHurtbox Extra;
	Extra.Shape.Type = EGSTN_ShapeType::Sphere;
	Extra.Shape.Extent = FVector(Rules.RadiusCm, Rules.RadiusCm, Rules.RadiusCm);
	// Root-relative: the entity's local up is +Z of the root, whatever the world gravity
	Extra.Shape.Local = FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, Rules.CentreHeightCm));
	Extra.Priority = TNumericLimits<uint8>::Max();
	Extra.IgnoreHitTypes = IgnoreHitTypes;
	Extra.Source = GSTNFeatureIds::Defence;
	return Extra;
}

int32 UGSTN_DefenceMath::ShieldStunFrames(double Damage, const FGSTN_ShieldRules& Rules)
{
	const double Raw = static_cast<double>(Rules.ShieldStunBase) + static_cast<double>(Rules.ShieldStunPerDamage) * FMath::Max(0.0, Damage);
	return FMath::Clamp(RoundHalfUp(Raw), 0, Rules.MaxShieldStun);
}

double UGSTN_DefenceMath::PushbackSpeedOf(double Damage, const FGSTN_ShieldRules& Rules)
{
	return static_cast<double>(Rules.PushbackSpeed) + static_cast<double>(Rules.PushbackSpeedPerDamage) * FMath::Max(0.0, Damage);
}

bool UGSTN_DefenceMath::EvaluateParryWindow(int32 PressFrame, int32 HitFrame, int32 ReleaseFrame, const FGSTN_ParryRules& Rules)
{
	if (Rules.WindowFrames <= 0 || PressFrame == INDEX_NONE)
	{
		return false;
	}

	if (Rules.Anchor == EGSTN_ParryAnchor::Press)
	{
		return HitFrame >= PressFrame && HitFrame < PressFrame + Rules.WindowFrames;
	}

	if (ReleaseFrame == INDEX_NONE || ReleaseFrame - PressFrame < Rules.MinHeldFramesBeforeRelease)
	{
		return false;
	}
	return HitFrame >= ReleaseFrame && HitFrame < ReleaseFrame + Rules.WindowFrames;
}

FGSTN_ParryDecision UGSTN_DefenceMath::ResolveParry(const FGSTN_ParryContext& Context, const UGSTN_DefenceRules* Rules)
{
	FGSTN_ParryDecision Decision;
	if (!Rules || !Context.bHitParryable || Context.bHitImmune || Context.bVictimImmune || Context.bGrabLikeHit)
	{
		return Decision;
	}

	// The shield first: it and a parrying move are one timeline ability each, so both stamps are rarely open together
	if (Context.bShieldAvailable && Context.bHitOnBubble
		&& EvaluateParryWindow(Context.ShieldPressFrame, Context.HitFrame, Context.ShieldReleaseFrame, Rules->Parry)
		&& IsProfileUsable(*Rules, Rules->Parry.ProfileIndex, Context.bProjectile))
	{
		Decision.Source = EGSTN_ParrySource::Shield;
		Decision.ProfileIndex = Rules->Parry.ProfileIndex;
		return Decision;
	}

	if (Context.MoveStartFrame != INDEX_NONE && Context.HitFrame >= Context.MoveStartFrame && Context.HitFrame < Context.MoveEndFrame
		&& Context.bMoveCoversHurtbox && IsProfileUsable(*Rules, Context.MoveProfileIndex, Context.bProjectile))
	{
		Decision.Source = EGSTN_ParrySource::Move;
		Decision.ProfileIndex = Context.MoveProfileIndex;
	}
	return Decision;
}

int32 UGSTN_DefenceMath::MoveWindowEndAfterParry(bool bCloseOnParry, int32 HitFrame, int32 WindowEndFrame)
{
	return bCloseOnParry ? FMath::Min(WindowEndFrame, HitFrame + 1) : WindowEndFrame;
}

int32 UGSTN_DefenceMath::AttackerStunFramesOf(const FGSTN_ParryProfile& Profile, FGameplayTag HitType)
{
	if (!HitType.IsValid())
	{
		return Profile.AttackerStunFrames;
	}
	if (const int32* Exact = Profile.AttackerStunFramesByHitClass.Find(HitType))
	{
		return *Exact;
	}

	int32 Best = Profile.AttackerStunFrames;
	int32 BestDepth = -1;
	for (const TPair<FGameplayTag, int32>& Pair : Profile.AttackerStunFramesByHitClass)
	{
		if (Pair.Key.IsValid() && HitType.MatchesTag(Pair.Key))
		{
			const int32 Depth = TagDepth(Pair.Key);
			if (Depth > BestDepth)
			{
				BestDepth = Depth;
				Best = Pair.Value;
			}
		}
	}
	return Best;
}

bool UGSTN_DefenceMath::HasParryImmunity(const FGameplayTagContainer& Tags)
{
	const FGameplayTag Immunity = FGameplayTag::RequestGameplayTag(TEXT("Combat.Immunity.Parry"), false);
	return Immunity.IsValid() && Tags.HasTag(Immunity);
}

bool UGSTN_DefenceMath::IsGrabLikeHitType(FGameplayTag HitType)
{
	if (!HitType.IsValid())
	{
		return false;
	}
	for (const TCHAR* Name : { TEXT("Combat.HitType.Grab"), TEXT("Combat.HitType.Throw"), TEXT("Combat.HitType.Pummel") })
	{
		const FGameplayTag Candidate = FGameplayTag::RequestGameplayTag(FName(Name), false);
		if (Candidate.IsValid() && HitType.MatchesTag(Candidate))
		{
			return true;
		}
	}
	return false;
}

bool UGSTN_DefenceMath::RegisterTechPress(FGSTN_TechState& State, int32 PressFrame, int32 HitstopEndFrame, bool bTumble, const FGSTN_TechRules& Rules)
{
	// Hitstun that is not a tumble: ignored, and it starts no lockout
	if (!bTumble && !Rules.bTechInHitstunNotTumble)
	{
		return false;
	}
	if (PressFrame < HitstopEndFrame)
	{
		return false;
	}
	// A press inside the lockout is ignored and does not extend it
	if (PressFrame < State.TechLockoutEndFrame)
	{
		return false;
	}

	State.LastTechPressFrame = PressFrame;
	State.TechLockoutEndFrame = PressFrame + Rules.LockoutFrames;
	return true;
}

FGSTN_TechEvaluation UGSTN_DefenceMath::EvaluateTech(const FGSTN_TechState& State, TConstArrayView<int32> PressFrames, int32 ContactFrame, int32 HitstopEndFrame, bool bTumble, const FGSTN_TechRules& Rules)
{
	FGSTN_TechEvaluation Result;
	Result.State = State;

	for (const int32 Press : PressFrames)
	{
		if (State.LastTechPressFrame != INDEX_NONE && Press <= State.LastTechPressFrame)
		{
			continue;
		}
		RegisterTechPress(Result.State, Press, HitstopEndFrame, bTumble, Rules);
	}

	if (ContactFrame == INDEX_NONE || (Result.State.LastHandledContact != INDEX_NONE && ContactFrame <= Result.State.LastHandledContact))
	{
		return Result;
	}

	Result.bContactEvaluated = true;
	Result.State.LastHandledContact = ContactFrame;
	if (Result.State.LastTechPressFrame != INDEX_NONE)
	{
		const int32 Early = ContactFrame - Result.State.LastTechPressFrame;
		if (Early >= -Rules.LateFrames && Early <= Rules.WindowFrames)
		{
			Result.bSuccess = true;
			Result.PressFrame = Result.State.LastTechPressFrame;
			Result.State.TechLockoutEndFrame = 0;
		}
	}
	return Result;
}

FVector UGSTN_DefenceMath::GroundBounceVelocity(const FGSTN_GroundBounceRules& Rules, const FVector& Up, const FVector& StickOnPlane)
{
	const FVector UpUnit = Up.GetSafeNormal();
	const FVector UpAxis = UpUnit.IsNearlyZero() ? FVector::UpVector : UpUnit;
	const double Speed = Rules.GroundBounceSpeed;

	// Only the part of the stick on the plane normal to Up steers
	const FVector OnPlane = StickOnPlane - UpAxis * FVector::DotProduct(StickOnPlane, UpAxis);
	const double Magnitude = FMath::Min(1.0, OnPlane.Size());
	if (Magnitude <= 0.0)
	{
		return UpAxis * Speed;
	}

	const double Angle = FMath::DegreesToRadians(static_cast<double>(Rules.GroundBounceSteerDeg) * Magnitude);
	const FVector Direction = OnPlane / OnPlane.Size();
	return UpAxis * (Speed * FMath::Cos(Angle)) + Direction * (Speed * FMath::Sin(Angle));
}

// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "State/GSTN_LinkMath.h"

#include "GSTN_LinkTags.h"

int32 UGSTN_LinkMath::MashInitialFrames(const FGSTN_MashRule& Rule, float DamagePercent, float Resistance)
{
	const double Raw = static_cast<double>(Rule.BaseFrames) + static_cast<double>(Rule.FramesPerDamagePercent) * FMath::Max(static_cast<double>(DamagePercent), 0.0);
	const double Low = FMath::Max(Rule.MinFrames, 1);
	const double High = FMath::Max(static_cast<double>(Rule.MaxFrames), Low);
	const double Scaled = FMath::Clamp(Raw, Low, High) * FMath::Max(static_cast<double>(Resistance), 0.0);
	return FMath::Max(FMath::RoundToInt(Scaled), 1);
}

int32 UGSTN_LinkMath::MashProgress(const FGSTN_MashRule& Rule, int32 EndFrame, int32 Frame, int32 Presses, int32 Flicks)
{
	if (EndFrame <= 0)
	{
		return EndFrame;
	}
	const int64 Counted = static_cast<int64>(FMath::Max(Presses, 0)) + (Rule.bCountFlicks ? static_cast<int64>(FMath::Max(Flicks, 0)) : 0);
	const int64 Reduced = static_cast<int64>(EndFrame) - Counted * static_cast<int64>(FMath::Max(Rule.ReducePerPress, 0));

	// Presses never pull the end before the current frame, and an end that is already past stays where it is
	const int64 Floor = FMath::Min(EndFrame, Frame);
	return static_cast<int32>(FMath::Max(Reduced, Floor));
}

FGSTN_LinkBreakDecision UGSTN_LinkMath::EvaluateBreak(const FGSTN_LinkBreakRules& Rules, int32 MaxFrames, const FGSTN_LinkBreakInput& Input)
{
	FGSTN_LinkBreakDecision Decision;
	const auto Break = [&Decision](EGSTN_LinkBreakReason Reason)
	{
		Decision.bBreak = true;
		Decision.Reason = Reason;
		return Decision;
	};

	if (Input.bPendingBreak)
	{
		return Break(Input.PendingReason == EGSTN_LinkBreakReason::None ? EGSTN_LinkBreakReason::Forced : Input.PendingReason);
	}
	if (Rules.bBreakOnOwnDeath && Input.bOwnDead)
	{
		return Break(EGSTN_LinkBreakReason::OwnDeath);
	}
	if (Rules.bBreakOnPartnerDeath && Input.bPartnerDead)
	{
		return Break(EGSTN_LinkBreakReason::PartnerDeath);
	}
	if (!Rules.PartnerMustNot.IsEmpty() && Input.PartnerStateTags.HasAny(Rules.PartnerMustNot))
	{
		return Break(EGSTN_LinkBreakReason::PartnerState);
	}

	if (Input.Hit.bHit)
	{
		for (const FGSTN_RoleHitBreak& HitBreak : Rules.HitBreaks)
		{
			if (HitBreak.RoleTag != Input.OwnRoleTag)
			{
				continue;
			}
			// Hits from the partner never break unless the rule says so, so a pummel keeps the link
			if (Input.Hit.bFromPartner && !HitBreak.bPartnerHitsBreak)
			{
				break;
			}
			if (Input.Hit.Knockback >= HitBreak.MinKnockbackToBreak)
			{
				return Break(EGSTN_LinkBreakReason::Hit);
			}
			break;
		}
	}

	if (Rules.MaxDistanceCm > 0.f && Input.PartnerDistanceCm > Rules.MaxDistanceCm)
	{
		return Break(EGSTN_LinkBreakReason::Distance);
	}
	if (MashEscaped(Input.EndFrame, Input.Frame))
	{
		return Break(EGSTN_LinkBreakReason::Escape);
	}
	if (MaxFrames > 0 && Input.Frame - Input.LinkFrame >= MaxFrames)
	{
		return Break(EGSTN_LinkBreakReason::Timeout);
	}
	return Decision;
}

FGameplayTag UGSTN_LinkMath::BreakReasonTag(EGSTN_LinkBreakReason Reason)
{
	switch (Reason)
	{
	case EGSTN_LinkBreakReason::ShakeOff: return GSTNLinkTags::BreakReason_ShakeOff;
	case EGSTN_LinkBreakReason::Timeout: return GSTNLinkTags::BreakReason_Timeout;
	case EGSTN_LinkBreakReason::Distance: return GSTNLinkTags::BreakReason_Distance;
	case EGSTN_LinkBreakReason::OwnDeath: return GSTNLinkTags::BreakReason_OwnDeath;
	case EGSTN_LinkBreakReason::PartnerDeath: return GSTNLinkTags::BreakReason_PartnerDeath;
	case EGSTN_LinkBreakReason::Hit: return GSTNLinkTags::BreakReason_Hit;
	case EGSTN_LinkBreakReason::PartnerState: return GSTNLinkTags::BreakReason_PartnerState;
	case EGSTN_LinkBreakReason::Escape: return GSTNLinkTags::BreakReason_Escape;
	case EGSTN_LinkBreakReason::Release: return GSTNLinkTags::BreakReason_Release;
	case EGSTN_LinkBreakReason::Throw: return GSTNLinkTags::BreakReason_Throw;
	case EGSTN_LinkBreakReason::Forced: return GSTNLinkTags::BreakReason_Forced;
	default: return FGameplayTag();
	}
}

EGSTN_LinkBreakReason UGSTN_LinkMath::BreakReasonFromTag(const FGameplayTag& Tag)
{
	for (uint8 Index = static_cast<uint8>(EGSTN_LinkBreakReason::ShakeOff); Index <= static_cast<uint8>(EGSTN_LinkBreakReason::Forced); ++Index)
	{
		if (BreakReasonTag(static_cast<EGSTN_LinkBreakReason>(Index)) == Tag)
		{
			return static_cast<EGSTN_LinkBreakReason>(Index);
		}
	}
	return EGSTN_LinkBreakReason::None;
}

FTransform UGSTN_LinkMath::ParkedTransform(const FTransform& PartnerTransform, const FGSTN_LinkRoleDef& ParkedRole)
{
	return ParkedTransformOnSocket(PartnerTransform, FTransform::Identity, ParkedRole);
}

FTransform UGSTN_LinkMath::ParkedTransformOnSocket(const FTransform& PartnerTransform, const FTransform& SocketRootRelative, const FGSTN_LinkRoleDef& ParkedRole)
{
	// FTransform composition applies the left operand first: the offset in socket space, the socket in root space, the root in the world
	return ParkedRole.SocketOffset * SocketRootRelative * PartnerTransform;
}

bool UGSTN_LinkMath::TryGetParkedTransform(const UGSTN_LinkTypeDefinition& Definition, int32 RoleIndex, const FTransform& PartnerTransform,
	const FTransform& SocketRootRelative, FTransform& OutTransform)
{
	if (RoleIndex < 1 || RoleIndex > 2 || Definition.GetParkedRoleIndex() != RoleIndex)
	{
		return false;
	}
	OutTransform = ParkedTransformOnSocket(PartnerTransform, SocketRootRelative, Definition.GetRoleDef(RoleIndex));
	return true;
}

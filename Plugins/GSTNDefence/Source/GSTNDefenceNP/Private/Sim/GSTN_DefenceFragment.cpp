// Defence, Network Prediction half.

#include "Sim/GSTN_DefenceFragment.h"

namespace
{
	/** Presence bits of the wire mask, one per group of fields; the layout is fixed, so a group never moves between builds */
	namespace Presence
	{
		constexpr uint16 Shield = 1 << 0;
		constexpr uint16 MoveParry = 1 << 1;
		constexpr uint16 LastParry = 1 << 2;
		constexpr uint16 Stuns = 1 << 3;
		constexpr uint16 Punish = 1 << 4;
		constexpr uint16 Tech = 1 << 5;
		constexpr uint16 Knockdown = 1 << 6;
		constexpr uint16 GrabImmunity = 1 << 7;
		constexpr uint16 Pushback = 1 << 8;
		constexpr uint16 Mirror = 1 << 9;
		constexpr uint16 All = (1 << 10) - 1;
	}

	bool SamePushback(const FGSTN_DefenceFragment& A, const FGSTN_DefenceFragment& B)
	{
		return A.PushbackStartFrame == B.PushbackStartFrame && A.PushbackFrames == B.PushbackFrames
			&& A.PushbackVelocityQ[0] == B.PushbackVelocityQ[0] && A.PushbackVelocityQ[1] == B.PushbackVelocityQ[1] && A.PushbackVelocityQ[2] == B.PushbackVelocityQ[2];
	}
}

FGSTN_ShieldView FGSTN_DefenceFragment::MakeShieldView(const FGSTN_ShieldRates& Rates) const
{
	FGSTN_ShieldView View;
	if (ShieldStampFrame == INDEX_NONE)
	{
		// Never stamped: a full shield, as the attribute set was granted with
		View.Value = Rates.Max;
		View.StampFrame = 0;
	}
	else
	{
		View.Value = DequantizeValue(ShieldValueQ);
		View.StampFrame = ShieldStampFrame;
	}
	View.bHeld = IsRaised();
	View.PressFrame = ShieldPressFrame;
	View.ReleaseFrame = ShieldReleaseFrame;
	return View;
}

void FGSTN_DefenceFragment::SetShieldView(const FGSTN_ShieldView& View)
{
	ShieldValueQ = QuantizeValue(View.Value);
	ShieldStampFrame = View.StampFrame;
	ShieldFlags = static_cast<uint8>(View.bHeld ? (ShieldFlags | GSTNShieldFlags::Raised) : (ShieldFlags & ~(GSTNShieldFlags::Raised | GSTNShieldFlags::Empty)));
	ShieldPressFrame = View.PressFrame;
	ShieldReleaseFrame = View.ReleaseFrame;
}

FVector FGSTN_DefenceFragment::GetPushbackVelocity() const
{
	return FVector(PushbackVelocityQ[0], PushbackVelocityQ[1], PushbackVelocityQ[2]) / VelocityUnitsPerCmPerSec;
}

void FGSTN_DefenceFragment::SetPushback(int32 StartFrame, const FVector& Velocity, int32 Frames)
{
	PushbackStartFrame = StartFrame;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		PushbackVelocityQ[Axis] = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Velocity[Axis] * VelocityUnitsPerCmPerSec), -32768, 32767));
	}
	PushbackFrames = static_cast<uint8>(FMath::Clamp(Frames, 0, 255));
}

void FGSTN_DefenceFragment::ClearPushback()
{
	PushbackStartFrame = INDEX_NONE;
	PushbackVelocityQ[0] = PushbackVelocityQ[1] = PushbackVelocityQ[2] = 0;
	PushbackFrames = 0;
}

int16 FGSTN_DefenceFragment::QuantizeValue(double Value)
{
	// Round half up, as the core's curve quantisation does, so two machines agree on the bit
	return static_cast<int16>(FMath::Clamp(FMath::FloorToInt(Value * ValueUnitsPerPoint + 0.5), -32768, 32767));
}

bool FGSTN_DefenceFragment::IsDefault() const
{
	return *this == FGSTN_DefenceFragment();
}

bool FGSTN_DefenceFragment::operator==(const FGSTN_DefenceFragment& Other) const
{
	return ShieldValueQ == Other.ShieldValueQ && ShieldStampFrame == Other.ShieldStampFrame && ShieldFlags == Other.ShieldFlags
		&& ShieldPressFrame == Other.ShieldPressFrame && ShieldReleaseFrame == Other.ShieldReleaseFrame
		&& MoveParryStartFrame == Other.MoveParryStartFrame && MoveParryEndFrame == Other.MoveParryEndFrame && MoveParryProfile == Other.MoveParryProfile
		&& MoveParryHurtboxMask == Other.MoveParryHurtboxMask && LastParryFrame == Other.LastParryFrame
		&& ShieldStunEndFrame == Other.ShieldStunEndFrame && ParryRecoveryEndFrame == Other.ParryRecoveryEndFrame && BreakEndFrame == Other.BreakEndFrame
		&& PunishEndFrame == Other.PunishEndFrame && PunishKind == Other.PunishKind
		&& TechLockoutEndFrame == Other.TechLockoutEndFrame && LastTechPressFrame == Other.LastTechPressFrame && TechEndFrame == Other.TechEndFrame && TechType == Other.TechType
		&& KnockdownEndFrame == Other.KnockdownEndFrame && GrabImmunityEndFrame == Other.GrabImmunityEndFrame
		&& SamePushback(*this, Other)
		&& MirrorValueQ == Other.MirrorValueQ && MirrorFrame == Other.MirrorFrame;
}

void FGSTN_DefenceFragment::NetSerialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		if (IsDefault())
		{
			return;
		}
		const FGSTN_DefenceFragment Defaults;
		uint16 Mask = 0;
		Mask |= (ShieldValueQ != 0 || ShieldStampFrame != INDEX_NONE || ShieldFlags != 0 || ShieldPressFrame != INDEX_NONE || ShieldReleaseFrame != INDEX_NONE) ? Presence::Shield : 0;
		Mask |= (MoveParryStartFrame != INDEX_NONE || MoveParryEndFrame != INDEX_NONE || MoveParryProfile != 0 || MoveParryHurtboxMask != 0) ? Presence::MoveParry : 0;
		Mask |= LastParryFrame != INDEX_NONE ? Presence::LastParry : 0;
		Mask |= (ShieldStunEndFrame != INDEX_NONE || ParryRecoveryEndFrame != INDEX_NONE || BreakEndFrame != INDEX_NONE) ? Presence::Stuns : 0;
		Mask |= (PunishEndFrame != INDEX_NONE || PunishKind != 0) ? Presence::Punish : 0;
		Mask |= (TechLockoutEndFrame != INDEX_NONE || LastTechPressFrame != INDEX_NONE || TechEndFrame != INDEX_NONE || TechType != 0) ? Presence::Tech : 0;
		Mask |= KnockdownEndFrame != INDEX_NONE ? Presence::Knockdown : 0;
		Mask |= GrabImmunityEndFrame != INDEX_NONE ? Presence::GrabImmunity : 0;
		Mask |= !SamePushback(*this, Defaults) ? Presence::Pushback : 0;
		Mask |= (MirrorValueQ != 0 || MirrorFrame != INDEX_NONE) ? Presence::Mirror : 0;
		Ar << Mask;
		if (Mask & Presence::Shield)
		{
			Ar << ShieldValueQ << ShieldStampFrame << ShieldFlags << ShieldPressFrame << ShieldReleaseFrame;
		}
		if (Mask & Presence::MoveParry)
		{
			Ar << MoveParryStartFrame << MoveParryEndFrame << MoveParryProfile << MoveParryHurtboxMask;
		}
		if (Mask & Presence::LastParry)
		{
			Ar << LastParryFrame;
		}
		if (Mask & Presence::Stuns)
		{
			Ar << ShieldStunEndFrame << ParryRecoveryEndFrame << BreakEndFrame;
		}
		if (Mask & Presence::Punish)
		{
			Ar << PunishEndFrame << PunishKind;
		}
		if (Mask & Presence::Tech)
		{
			Ar << TechLockoutEndFrame << LastTechPressFrame << TechEndFrame << TechType;
		}
		if (Mask & Presence::Knockdown)
		{
			Ar << KnockdownEndFrame;
		}
		if (Mask & Presence::GrabImmunity)
		{
			Ar << GrabImmunityEndFrame;
		}
		if (Mask & Presence::Pushback)
		{
			Ar << PushbackStartFrame << PushbackVelocityQ[0] << PushbackVelocityQ[1] << PushbackVelocityQ[2] << PushbackFrames;
		}
		if (Mask & Presence::Mirror)
		{
			Ar << MirrorValueQ << MirrorFrame;
		}
		return;
	}

	*this = FGSTN_DefenceFragment();
	// The registry reads into a default instance: a payload of zero bytes is the default fragment
	if (Ar.TotalSize() == 0)
	{
		return;
	}
	uint16 Mask = 0;
	Ar << Mask;
	if (Mask == 0 || (Mask & ~Presence::All) != 0)
	{
		// No group at all, or a bit this build does not know: a corrupt or foreign payload, not state to compare
		Ar.SetError();
		*this = FGSTN_DefenceFragment();
		return;
	}
	if (Mask & Presence::Shield)
	{
		Ar << ShieldValueQ << ShieldStampFrame << ShieldFlags << ShieldPressFrame << ShieldReleaseFrame;
	}
	if (Mask & Presence::MoveParry)
	{
		Ar << MoveParryStartFrame << MoveParryEndFrame << MoveParryProfile << MoveParryHurtboxMask;
	}
	if (Mask & Presence::LastParry)
	{
		Ar << LastParryFrame;
	}
	if (Mask & Presence::Stuns)
	{
		Ar << ShieldStunEndFrame << ParryRecoveryEndFrame << BreakEndFrame;
	}
	if (Mask & Presence::Punish)
	{
		Ar << PunishEndFrame << PunishKind;
	}
	if (Mask & Presence::Tech)
	{
		Ar << TechLockoutEndFrame << LastTechPressFrame << TechEndFrame << TechType;
	}
	if (Mask & Presence::Knockdown)
	{
		Ar << KnockdownEndFrame;
	}
	if (Mask & Presence::GrabImmunity)
	{
		Ar << GrabImmunityEndFrame;
	}
	if (Mask & Presence::Pushback)
	{
		Ar << PushbackStartFrame << PushbackVelocityQ[0] << PushbackVelocityQ[1] << PushbackVelocityQ[2] << PushbackFrames;
	}
	if (Mask & Presence::Mirror)
	{
		Ar << MirrorValueQ << MirrorFrame;
	}
	if (Ar.IsError())
	{
		*this = FGSTN_DefenceFragment();
	}
}

bool FGSTN_DefenceFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	return !(*this == static_cast<const FGSTN_DefenceFragment&>(Authority));
}

void FGSTN_DefenceFragment::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("Defence");
	if (ShieldStampFrame != INDEX_NONE || ShieldFlags != 0 || ShieldPressFrame != INDEX_NONE)
	{
		Out.Appendf(" shield %d.%02d@%d flags 0x%X press %d release %d", ShieldValueQ / 4, (ShieldValueQ % 4) * 25, ShieldStampFrame, ShieldFlags, ShieldPressFrame, ShieldReleaseFrame);
	}
	if (HasMoveParryWindow())
	{
		Out.Appendf(" parry [%d,%d) p%d m0x%X", MoveParryStartFrame, MoveParryEndFrame, static_cast<int32>(MoveParryProfile), static_cast<int32>(MoveParryHurtboxMask));
	}
	if (LastParryFrame != INDEX_NONE)
	{
		Out.Appendf(" parried@%d", LastParryFrame);
	}
	if (ShieldStunEndFrame != INDEX_NONE || ParryRecoveryEndFrame != INDEX_NONE || BreakEndFrame != INDEX_NONE)
	{
		Out.Appendf(" stun %d recovery %d break %d", ShieldStunEndFrame, ParryRecoveryEndFrame, BreakEndFrame);
	}
	if (PunishEndFrame != INDEX_NONE)
	{
		Out.Appendf(" punish %d kind %d", PunishEndFrame, static_cast<int32>(PunishKind));
	}
	if (TechLockoutEndFrame != INDEX_NONE || LastTechPressFrame != INDEX_NONE || TechEndFrame != INDEX_NONE)
	{
		Out.Appendf(" tech lockout %d press %d end %d type %d", TechLockoutEndFrame, LastTechPressFrame, TechEndFrame, static_cast<int32>(TechType));
	}
	if (KnockdownEndFrame != INDEX_NONE)
	{
		Out.Appendf(" knockdown %d", KnockdownEndFrame);
	}
	if (GrabImmunityEndFrame != INDEX_NONE)
	{
		Out.Appendf(" grabimmune %d", GrabImmunityEndFrame);
	}
	if (PushbackStartFrame != INDEX_NONE)
	{
		Out.Appendf(" pushback@%d (%d,%d,%d) %d", PushbackStartFrame, PushbackVelocityQ[0], PushbackVelocityQ[1], PushbackVelocityQ[2], static_cast<int32>(PushbackFrames));
	}
	if (MirrorFrame != INDEX_NONE)
	{
		Out.Appendf(" mirror %d@%d", MirrorValueQ, MirrorFrame);
	}
}

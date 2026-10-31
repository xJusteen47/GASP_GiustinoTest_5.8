// Links, Network Prediction half.

#include "Sim/GSTN_LinkFragment.h"

namespace
{
	/** Presence bits of the serialised fragment */
	namespace Presence
	{
		constexpr uint8 Body = 1 << 0;
		constexpr uint8 Hand = 1 << 1;
		constexpr uint8 Actions = 1 << 2;
		constexpr uint8 GrabImmunity = 1 << 3;
		constexpr uint8 Mash = 1 << 4;
		constexpr uint8 All = Body | Hand | Actions | GrabImmunity | Mash;
	}

	void SerializeLinkState(FArchive& Ar, FGSTN_LinkState& State)
	{
		bool bSuccess = true;
		State.NetSerialize(Ar, nullptr, bSuccess);
		if (!bSuccess)
		{
			Ar.SetError();
		}
	}

	void SerializeVector(FArchive& Ar, FVector& Value)
	{
		// The exit is the watchdog's result, written as floats so every machine reads back the same bits it wrote
		float X = static_cast<float>(Value.X);
		float Y = static_cast<float>(Value.Y);
		float Z = static_cast<float>(Value.Z);
		Ar << X << Y << Z;
		if (Ar.IsLoading())
		{
			Value = FVector(X, Y, Z);
		}
	}

	void SerializeActions(FArchive& Ar, FGSTN_LinkActions& Actions)
	{
		Ar << Actions.LastRefusedSerial << Actions.RefusedFrame << Actions.CooldownEndFrame;
		Ar << Actions.Active.Id << Actions.Active.StartFrame;
		Ar << Actions.PendingStart.Frame << Actions.PendingStart.Id;
		Ar << Actions.Exit.Frame;
		SerializeVector(Ar, Actions.Exit.Position);
		Ar << Actions.Exit.YawDeg;
		SerializeVector(Ar, Actions.Exit.Velocity);
		Ar << Actions.Exit.Flags;
	}
}

bool FGSTN_LinkFragment::IsDefault() const
{
	return *this == FGSTN_LinkFragment();
}

bool FGSTN_LinkFragment::operator==(const FGSTN_LinkFragment& Other) const
{
	return Links == Other.Links && Actions == Other.Actions && GrabImmunityEndFrame == Other.GrabImmunityEndFrame
		&& MashProcessedFrame == Other.MashProcessedFrame;
}

void FGSTN_LinkFragment::NetSerialize(FArchive& Ar)
{
	uint8 Mask = 0;
	if (Ar.IsSaving())
	{
		if (IsDefault())
		{
			return;
		}
		Mask |= Links.Body != FGSTN_LinkState() ? Presence::Body : 0;
		Mask |= Links.Hand != FGSTN_LinkState() ? Presence::Hand : 0;
		Mask |= !Actions.IsIdle() ? Presence::Actions : 0;
		Mask |= GrabImmunityEndFrame != 0 ? Presence::GrabImmunity : 0;
		Mask |= MashProcessedFrame != 0 ? Presence::Mash : 0;
		Ar << Mask;
	}
	else
	{
		*this = FGSTN_LinkFragment();
		// The registry reads into a default instance: a payload of zero bytes is the default fragment
		if (Ar.TotalSize() == 0)
		{
			return;
		}
		Ar << Mask;
		if ((Mask & ~Presence::All) != 0)
		{
			Ar.SetError();
			return;
		}
	}

	if (Mask & Presence::Body)
	{
		SerializeLinkState(Ar, Links.Body);
	}
	if (Mask & Presence::Hand)
	{
		SerializeLinkState(Ar, Links.Hand);
	}
	if (Mask & Presence::Actions)
	{
		SerializeActions(Ar, Actions);
	}
	if (Mask & Presence::GrabImmunity)
	{
		Ar << GrabImmunityEndFrame;
	}
	if (Mask & Presence::Mash)
	{
		Ar << MashProcessedFrame;
	}
}

bool FGSTN_LinkFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	return !(*this == static_cast<const FGSTN_LinkFragment&>(Authority));
}

void FGSTN_LinkFragment::ToString(FAnsiStringBuilderBase& Out) const
{
	Out << "Link";
	if (Links.Body != FGSTN_LinkState())
	{
		Out << " body" << TCHAR_TO_ANSI(*Links.Body.ToString());
	}
	if (Links.Hand != FGSTN_LinkState())
	{
		Out << " hand" << TCHAR_TO_ANSI(*Links.Hand.ToString());
	}
	if (!Actions.IsIdle())
	{
		Out.Appendf(" actions{refused %d@%d cooldown %d active %d@%d pending %d@%d exit %d}", Actions.LastRefusedSerial, Actions.RefusedFrame,
			Actions.CooldownEndFrame, Actions.Active.Id, Actions.Active.StartFrame, Actions.PendingStart.Id, Actions.PendingStart.Frame, Actions.Exit.Frame);
	}
	if (GrabImmunityEndFrame != 0)
	{
		Out.Appendf(" immune %d", GrabImmunityEndFrame);
	}
	if (MashProcessedFrame != 0)
	{
		Out.Appendf(" mash %d", MashProcessedFrame);
	}
}

bool FGSTN_LinkFragmentRegistration::Register()
{
	return FGSTN_SyncFragmentRegistry::Register<FGSTN_LinkFragment>(GSTNFeatureIds::Link);
}

void FGSTN_LinkFragmentRegistration::Unregister()
{
	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::Link);
}

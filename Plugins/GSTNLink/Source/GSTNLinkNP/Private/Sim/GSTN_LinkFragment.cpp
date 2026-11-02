// Links, Network Prediction half.

#include "Sim/GSTN_LinkFragment.h"

#include "GSTN_LinkSettings.h"
#include "GSTN_LinkWatchdog.h"

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
		constexpr uint8 ExitIgnore = 1 << 5;
		constexpr uint8 All = Body | Hand | Actions | GrabImmunity | Mash | ExitIgnore;
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
		Ar << Actions.Active.Id << Actions.Active.StartFrame << Actions.Active.Side;
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
		&& MashProcessedFrame == Other.MashProcessedFrame && ExitIgnorePartner == Other.ExitIgnorePartner && ExitIgnoreEndFrame == Other.ExitIgnoreEndFrame;
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
		Mask |= ExitIgnoreEndFrame != 0 || !ExitIgnorePartner.IsNull() ? Presence::ExitIgnore : 0;
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
	if (Mask & Presence::ExitIgnore)
	{
		bool bSuccess = true;
		ExitIgnorePartner.NetSerialize(Ar, nullptr, bSuccess);
		Ar << ExitIgnoreEndFrame;
		if (!bSuccess)
		{
			Ar.SetError();
		}
	}
}

bool FGSTN_LinkFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	const FGSTN_LinkFragment& Other = static_cast<const FGSTN_LinkFragment&>(Authority);
	if (*this == Other)
	{
		return false;
	}
	// M7 G1: an optimistic accept (Active, role 1, flag Predicted) is the authority's confirmed link but for the flag, which only the response
	// clears and which never reaches a machine that does not simulate the target: equal links otherwise are not a misprediction
	FGSTN_LinkFragment Local = *this;
	FGSTN_LinkFragment Server = Other;
	for (const EGSTN_LinkChannel Channel : { EGSTN_LinkChannel::Body, EGSTN_LinkChannel::Hand })
	{
		FGSTN_LinkState& Mine = Local.Get(Channel);
		const FGSTN_LinkState& Theirs = Server.Get(Channel);
		if (Mine.IsActive() && Mine.Role == 1 && Mine.HasFlag(GSTNLinkFlags::Predicted) && !Theirs.HasFlag(GSTNLinkFlags::Predicted))
		{
			Mine.Flags &= ~GSTNLinkFlags::Predicted;
		}
		// M7 G6: a per-link anchor both machines decided from their own view of the same hit is the same anchor within the tolerance
		FVector MineAnchor;
		FVector TheirAnchor;
		if (Mine.GetAnchorOverride(MineAnchor) && Theirs.GetAnchorOverride(TheirAnchor) && IsSamePlacement(MineAnchor, TheirAnchor))
		{
			FMemory::Memcpy(Mine.AnchorCm, Theirs.AnchorCm, sizeof(Mine.AnchorCm));
		}
	}
	// M7 G6: so is an exit the parked initiator placed from its view (the authority placed it from the rewound one): same frame, same flags
	FGSTN_LinkActions::FExit& MineExit = Local.Actions.Exit;
	const FGSTN_LinkActions::FExit& TheirExit = Server.Actions.Exit;
	if ((MineExit.Flags & GSTNLinkExitFlags::InitiatorPlaced) != 0 && MineExit.Frame == TheirExit.Frame && MineExit.Flags == TheirExit.Flags
		&& IsSamePlacement(MineExit.Position, TheirExit.Position) && IsSamePlacement(MineExit.Velocity, TheirExit.Velocity)
		&& FMath::Abs(FMath::FindDeltaAngleDegrees(MineExit.YawDeg, TheirExit.YawDeg)) <= PlacementToleranceYawDeg)
	{
		MineExit = TheirExit;
	}
	return !(Local == Server);
}

bool FGSTN_LinkFragment::IsSamePlacement(const FVector& Mine, const FVector& Theirs)
{
	const double Tolerance = UGSTN_LinkSettings::Get().PredictedPlacementToleranceCm;
	return FVector::DistSquared(Mine, Theirs) <= Tolerance * Tolerance;
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
		Out.Appendf(" actions{refused %d@%d cooldown %d active %d@%d side %d pending %d@%d exit %d}", Actions.LastRefusedSerial, Actions.RefusedFrame,
			Actions.CooldownEndFrame, Actions.Active.Id, Actions.Active.StartFrame, Actions.Active.Side, Actions.PendingStart.Id, Actions.PendingStart.Frame, Actions.Exit.Frame);
	}
	if (GrabImmunityEndFrame != 0)
	{
		Out.Appendf(" immune %d", GrabImmunityEndFrame);
	}
	if (MashProcessedFrame != 0)
	{
		Out.Appendf(" mash %d", MashProcessedFrame);
	}
	if (ExitIgnoreEndFrame != 0)
	{
		Out << " exitignore " << TCHAR_TO_ANSI(*ExitIgnorePartner.ToString());
		Out.Appendf(" until %d", ExitIgnoreEndFrame);
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

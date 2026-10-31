// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "State/GSTN_LinkState.h"

namespace
{
	/** Header bits of the serialised state: the live fields follow, the serial follows */
	constexpr uint8 HeaderLive = 1 << 0;
	constexpr uint8 HeaderSerial = 1 << 1;
}

bool FGSTN_LinkState::operator==(const FGSTN_LinkState& Other) const
{
	return LinkTypeIndex == Other.LinkTypeIndex && Role == Other.Role && Phase == Other.Phase && PartnerId == Other.PartnerId
		&& Serial == Other.Serial && LinkFrame == Other.LinkFrame && EndFrame == Other.EndFrame && Flags == Other.Flags
		&& BreakReason == Other.BreakReason && Payload == Other.Payload && LocalYawQ == Other.LocalYawQ;
}

uint8 FGSTN_LinkState::NextSerial(uint8 A, uint8 B)
{
	const uint8 Newest = IsSerialNewer(A, B) ? A : B;
	const uint8 Next = static_cast<uint8>(Newest + 1);
	// 0 means "never linked", so a live serial skips it when the circle wraps
	return Next == 0 ? 1 : Next;
}

bool FGSTN_LinkState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;

	uint8 Header = 0;
	if (Ar.IsSaving())
	{
		// "Live" is anything but the serial differing from the default, so any state round-trips exactly
		FGSTN_LinkState WithoutSerial = *this;
		WithoutSerial.Serial = 0;
		Header = (WithoutSerial == FGSTN_LinkState() ? 0 : HeaderLive) | (Serial != 0 ? HeaderSerial : 0);
	}
	Ar << Header;

	if (Ar.IsLoading())
	{
		*this = FGSTN_LinkState();
	}
	if ((Header & HeaderSerial) != 0)
	{
		Ar << Serial;
	}
	if ((Header & HeaderLive) != 0)
	{
		uint8 PhaseByte = static_cast<uint8>(Phase);
		Ar << LinkTypeIndex;
		Ar << Role;
		Ar << PhaseByte;
		Ar << LinkFrame;
		Ar << EndFrame;
		Ar << Flags;
		Ar << BreakReason;
		Ar << Payload;
		Ar << LocalYawQ;
		bool bPartnerSuccess = true;
		PartnerId.NetSerialize(Ar, Map, bPartnerSuccess);
		bOutSuccess = bPartnerSuccess;
		if (Ar.IsLoading())
		{
			Phase = static_cast<EGSTN_LinkPhase>(PhaseByte);
		}
	}
	return true;
}

FString FGSTN_LinkState::ToString() const
{
	return FString::Printf(TEXT("Link{type %d role %d phase %d partner %s serial %d frame %d end %d flags %d reason %d}"),
		LinkTypeIndex, Role, static_cast<int32>(Phase), *PartnerId.ToString(), Serial, LinkFrame, EndFrame, Flags, BreakReason);
}

bool FGSTN_LinkActions::operator==(const FGSTN_LinkActions& Other) const
{
	return LastRefusedSerial == Other.LastRefusedSerial && RefusedFrame == Other.RefusedFrame && CooldownEndFrame == Other.CooldownEndFrame
		&& Active.Id == Other.Active.Id && Active.StartFrame == Other.Active.StartFrame
		&& PendingStart.Frame == Other.PendingStart.Frame && PendingStart.Id == Other.PendingStart.Id
		&& Exit.Frame == Other.Exit.Frame && Exit.Position == Other.Exit.Position && Exit.YawDeg == Other.Exit.YawDeg
		&& Exit.Velocity == Other.Exit.Velocity && Exit.Flags == Other.Exit.Flags;
}

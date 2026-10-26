// Destructible parts, Network Prediction half.

#include "State/GSTN_PartsFragment.h"

namespace
{
	/** The percent of a quantised byte as an integer, rounded to nearest: pure integer math, the same text on every machine */
	int32 PercentOf(uint8 Byte)
	{
		return (static_cast<int32>(Byte) * 100 + 127) / 255;
	}
}

bool FGSTN_PartsFragment::IsEmpty() const
{
	if (BrokenMask != 0 || BreakSerial != 0)
	{
		return false;
	}
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxParts; ++Index)
	{
		if (PartDamage[Index] != 0)
		{
			return false;
		}
	}
	return true;
}

int32 FGSTN_PartsFragment::GetWireCount() const
{
	int32 Count = FMath::Min<int32>(PartCount, GSTN_PartsLimits::MaxParts);
	if (BrokenMask != 0)
	{
		Count = FMath::Max(Count, 32 - static_cast<int32>(FMath::CountLeadingZeros(static_cast<uint32>(BrokenMask))));
	}
	for (int32 Index = GSTN_PartsLimits::MaxParts - 1; Index >= Count; --Index)
	{
		if (PartDamage[Index] != 0)
		{
			Count = Index + 1;
			break;
		}
	}
	return Count;
}

FGSTN_PartsState FGSTN_PartsFragment::ToState() const
{
	FGSTN_PartsState State;
	State.BrokenMask = BrokenMask;
	FMemory::Memcpy(State.PartDamage, PartDamage, sizeof(PartDamage));
	State.BreakSerial = BreakSerial;
	return State;
}

void FGSTN_PartsFragment::SetState(const FGSTN_PartsState& State, int32 InPartCount)
{
	BrokenMask = State.BrokenMask;
	FMemory::Memcpy(PartDamage, State.PartDamage, sizeof(PartDamage));
	BreakSerial = static_cast<uint8>(State.BreakSerial & FGSTN_PartsState::SerialMask);
	PartCount = static_cast<uint8>(FMath::Clamp(InPartCount, 0, GSTN_PartsLimits::MaxParts));
}

void FGSTN_PartsFragment::NetSerialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		if (IsEmpty())
		{
			return;
		}
		const int32 Count = GetWireCount();
		uint16 Mask = BrokenMask;
		Ar << Mask;
		if (Count > 0)
		{
			Ar.Serialize(PartDamage, Count);
		}
		uint8 Serial = BreakSerial;
		Ar << Serial;
		return;
	}

	BrokenMask = 0;
	FMemory::Memzero(PartDamage, sizeof(PartDamage));
	BreakSerial = 0;
	PartCount = 0;

	// The registry reads into a default instance, so N comes from how many bytes the payload has
	const int64 Size = Ar.TotalSize();
	if (Size == 0)
	{
		return;
	}
	if (Size < HeaderBytes || Size > GetPayloadBytes(GSTN_PartsLimits::MaxParts))
	{
		Ar.SetError();
		return;
	}
	const int32 Count = static_cast<int32>(Size) - HeaderBytes;
	Ar << BrokenMask;
	if (Count > 0)
	{
		Ar.Serialize(PartDamage, Count);
	}
	Ar << BreakSerial;
	if (BreakSerial > FGSTN_PartsState::SerialMask)
	{
		// A 4 bit field cannot hold it: a value like this is a corrupt or foreign payload, not state to compare
		Ar.SetError();
		return;
	}
	PartCount = static_cast<uint8>(Count);
}

bool FGSTN_PartsFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	const FGSTN_PartsFragment& Other = static_cast<const FGSTN_PartsFragment&>(Authority);
	return BrokenMask != Other.BrokenMask || BreakSerial != Other.BreakSerial
		|| FMemory::Memcmp(PartDamage, Other.PartDamage, sizeof(PartDamage)) != 0;
}

void FGSTN_PartsFragment::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Parts mask 0x%04X serial %d n=%d [", BrokenMask, BreakSerial, GetWireCount());
	const int32 Count = GetWireCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Out.Appendf("%s%d:%d%%", Index > 0 ? " " : "", Index, PercentOf(PartDamage[Index]));
	}
	Out.Append("]");
}

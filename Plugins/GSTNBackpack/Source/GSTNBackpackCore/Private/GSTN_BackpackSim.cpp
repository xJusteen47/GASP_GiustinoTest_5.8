// Backpacks, pure simulation data (M11 L7b).

#include "GSTN_BackpackSim.h"
#include "GSTN_BackpackTypes.h"

namespace GSTNBackpackSim
{
	template <typename TrackT>
	const TrackT* FindTrack(const FGSTN_CombatTimelineData& Data)
	{
		const UScriptStruct* Wanted = TrackT::StaticStruct();
		for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
		{
			const UScriptStruct* Struct = Track.GetScriptStruct();
			if (Struct && Struct->IsChildOf(Wanted))
			{
				return &Track.Get<TrackT>();
			}
		}
		return nullptr;
	}

	/** +1 so INDEX_NONE is 0 and the packed form stays small */
	void SerializeFrame(FArchive& Ar, int32& Frame)
	{
		uint32 PlusOne = static_cast<uint32>(FMath::Max(Frame + 1, 0));
		Ar.SerializeIntPacked(PlusOne);
		if (Ar.IsLoading())
		{
			Frame = static_cast<int32>(PlusOne) - 1;
		}
	}

	/** A full slot with no running refill forgets its rule, so an idle fragment is the default again (no bytes on the wire) */
	void NormalizeSlot(FGSTN_BackpackMemorySlot& Slot)
	{
		if (Slot.Spent == 0)
		{
			Slot = FGSTN_BackpackMemorySlot();
		}
	}

	void ClearDome(FGSTN_BackpackFragment& Fragment)
	{
		Fragment.DomeStartFrame = INDEX_NONE;
		Fragment.DomeUntilFrame = INDEX_NONE;
		Fragment.DomeTimeline = 0;
		Fragment.DomeRadiusCm = 0;
		Fragment.DomeLifetime = EGSTN_DomeLifetime::WhileHeld;
		Fragment.DomeCenter = FIntVector::ZeroValue;
		Fragment.DomePushSpeedQ = 0;
		Fragment.DomePushFrames = 0;
		Fragment.DomePushDirection = EGSTN_DomePushDirection::AwayFromCenter;
	}
}

const FGSTN_BackpackChargeTrack* FGSTN_BackpackChargeTrack::Find(const FGSTN_CombatTimelineData& Data)
{
	return GSTNBackpackSim::FindTrack<FGSTN_BackpackChargeTrack>(Data);
}

const FGSTN_BackpackDomeTrack* FGSTN_BackpackDomeTrack::Find(const FGSTN_CombatTimelineData& Data)
{
	return GSTNBackpackSim::FindTrack<FGSTN_BackpackDomeTrack>(Data);
}

// ---- Fragment

bool FGSTN_BackpackFragment::DomeCovers(const FVector& Point) const
{
	if (!HasDome())
	{
		return false;
	}
	const double Radius = static_cast<double>(DomeRadiusCm);
	return FVector::DistSquared(Point, FVector(DomeCenter)) <= Radius * Radius;
}

bool FGSTN_BackpackFragment::IsDefault() const
{
	for (const FGSTN_BackpackMemorySlot& Slot : Memory)
	{
		if (!Slot.IsDefault())
		{
			return false;
		}
	}
	return !HasDome() && DomeUntilFrame == INDEX_NONE && DomeTimeline == 0 && DomeRadiusCm == 0 && DomeCenter == FIntVector::ZeroValue
		&& DomePushSpeedQ == 0 && DomePushFrames == 0 && DomePushDirection == EGSTN_DomePushDirection::AwayFromCenter;
}

bool FGSTN_BackpackFragment::operator==(const FGSTN_BackpackFragment& Other) const
{
	for (int32 Index = 0; Index < GSTNBackpackMemory::NumSlots; ++Index)
	{
		if (!(Memory[Index] == Other.Memory[Index]))
		{
			return false;
		}
	}
	return DomeStartFrame == Other.DomeStartFrame && DomeUntilFrame == Other.DomeUntilFrame && DomeTimeline == Other.DomeTimeline
		&& DomeRadiusCm == Other.DomeRadiusCm && DomeLifetime == Other.DomeLifetime && DomeCenter == Other.DomeCenter
		&& DomePushSpeedQ == Other.DomePushSpeedQ && DomePushFrames == Other.DomePushFrames && DomePushDirection == Other.DomePushDirection;
}

void FGSTN_BackpackFragment::NetSerialize(FArchive& Ar)
{
	uint8 Mask = 0;
	if (!Ar.IsLoading())
	{
		for (int32 Index = 0; Index < GSTNBackpackMemory::NumSlots; ++Index)
		{
			Mask |= Memory[Index].IsDefault() ? 0 : (1 << Index);
		}
		Mask |= HasDome() ? (1 << 4) : 0;
	}
	Ar.SerializeBits(&Mask, 5);
	if (Ar.IsLoading())
	{
		*this = FGSTN_BackpackFragment();
	}

	for (int32 Index = 0; Index < GSTNBackpackMemory::NumSlots; ++Index)
	{
		if ((Mask & (1 << Index)) == 0)
		{
			continue;
		}
		FGSTN_BackpackMemorySlot& Slot = Memory[Index];
		uint8 Refill = static_cast<uint8>(Slot.Refill);
		uint32 RefillFrames = Slot.RefillFrames;
		Ar << Slot.Spent;
		Ar << Slot.Charges;
		Ar << Refill;
		Ar.SerializeIntPacked(RefillFrames);
		GSTNBackpackSim::SerializeFrame(Ar, Slot.RefillFromFrame);
		if (Ar.IsLoading())
		{
			Slot.Refill = static_cast<EGSTN_BackpackRefill>(FMath::Min<uint8>(Refill, static_cast<uint8>(EGSTN_BackpackRefill::OverTime)));
			Slot.RefillFrames = static_cast<uint16>(FMath::Min<uint32>(RefillFrames, MAX_uint16));
		}
	}

	if ((Mask & (1 << 4)) != 0)
	{
		uint32 Timeline = DomeTimeline;
		uint32 Radius = DomeRadiusCm;
		uint8 Lifetime = static_cast<uint8>(DomeLifetime);
		GSTNBackpackSim::SerializeFrame(Ar, DomeStartFrame);
		GSTNBackpackSim::SerializeFrame(Ar, DomeUntilFrame);
		Ar.SerializeIntPacked(Timeline);
		Ar.SerializeIntPacked(Radius);
		Ar << Lifetime;
		Ar << DomeCenter.X;
		Ar << DomeCenter.Y;
		Ar << DomeCenter.Z;
		// Frames in the low 7 bits, the direction in the top one
		uint8 PushPacked = static_cast<uint8>((DomePushFrames & 0x7F) | (DomePushDirection == EGSTN_DomePushDirection::BackAlongAttack ? 0x80 : 0));
		Ar << DomePushSpeedQ;
		Ar << PushPacked;
		if (Ar.IsLoading())
		{
			DomePushFrames = PushPacked & 0x7F;
			DomePushDirection = (PushPacked & 0x80) != 0 ? EGSTN_DomePushDirection::BackAlongAttack : EGSTN_DomePushDirection::AwayFromCenter;
			DomeTimeline = static_cast<uint16>(FMath::Min<uint32>(Timeline, MAX_uint16));
			DomeRadiusCm = static_cast<uint16>(FMath::Min<uint32>(Radius, MAX_uint16));
			DomeLifetime = Lifetime == static_cast<uint8>(EGSTN_DomeLifetime::Deploy) ? EGSTN_DomeLifetime::Deploy : EGSTN_DomeLifetime::WhileHeld;
		}
	}
}

bool FGSTN_BackpackFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	// Integers only: exact equality, no tolerance. The registry hands over a fragment of the same type
	return !(*this == static_cast<const FGSTN_BackpackFragment&>(Authority));
}

void FGSTN_BackpackFragment::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("Backpack");
	for (int32 Index = 0; Index < GSTNBackpackMemory::NumSlots; ++Index)
	{
		const FGSTN_BackpackMemorySlot& Slot = Memory[Index];
		if (!Slot.IsDefault())
		{
			Out.Appendf(" m%d %d/%d r%d@%d", Index, Slot.Spent, Slot.Charges, static_cast<int32>(Slot.Refill), Slot.RefillFromFrame);
		}
	}
	if (HasDome())
	{
		Out.Appendf(" dome@%d until %d tl %d r %d", DomeStartFrame, DomeUntilFrame, DomeTimeline, DomeRadiusCm);
	}
}

bool FGSTN_BackpackFragmentRegistration::Register()
{
	return FGSTN_SyncFragmentRegistry::Register<FGSTN_BackpackFragment>(GSTNFeatureIds::Backpack);
}

void FGSTN_BackpackFragmentRegistration::Unregister()
{
	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::Backpack);
}

// ---- Step

void FGSTN_BackpackStep::AfterClocks(FGSTN_BackpackFragment& Fragment, int32 Frame, bool bDomeTimelineRunning)
{
	for (FGSTN_BackpackMemorySlot& Slot : Fragment.Memory)
	{
		if (Slot.Refill != EGSTN_BackpackRefill::OverTime || Slot.Spent == 0 || Slot.RefillFrames == 0 || Slot.RefillFromFrame == INDEX_NONE)
		{
			continue;
		}
		if (Frame - Slot.RefillFromFrame >= Slot.RefillFrames)
		{
			--Slot.Spent;
			// The next charge counts from this frame; a full slot has no running refill
			Slot.RefillFromFrame = Slot.Spent > 0 ? Frame : INDEX_NONE;
			GSTNBackpackSim::NormalizeSlot(Slot);
		}
	}

	if (Fragment.HasDome())
	{
		const bool bExpired = Fragment.DomeUntilFrame != INDEX_NONE && Frame >= Fragment.DomeUntilFrame;
		const bool bReleased = Fragment.DomeLifetime == EGSTN_DomeLifetime::WhileHeld && !bDomeTimelineRunning;
		if (bExpired || bReleased)
		{
			GSTNBackpackSim::ClearDome(Fragment);
		}
	}
}

void FGSTN_BackpackStep::Landed(FGSTN_BackpackFragment& Fragment)
{
	for (FGSTN_BackpackMemorySlot& Slot : Fragment.Memory)
	{
		if (Slot.Refill == EGSTN_BackpackRefill::OnLanding)
		{
			Slot = FGSTN_BackpackMemorySlot();
		}
	}
}

bool FGSTN_BackpackStep::Spend(FGSTN_BackpackFragment& Fragment, int32 Frame, const FGSTN_BackpackChargeTrack& Track)
{
	if (Track.Slot < 0 || Track.Slot >= GSTNBackpackMemory::NumSlots)
	{
		return false;
	}
	FGSTN_BackpackMemorySlot& Slot = Fragment.Memory[Track.Slot];
	// The slot's rule is the track's: the last move that spends it says what it is (the same on every machine: content)
	Slot.Charges = static_cast<uint8>(FMath::Clamp(Track.Charges, 1, 255));
	Slot.Refill = Track.Refill;
	Slot.RefillFrames = static_cast<uint16>(Track.Refill == EGSTN_BackpackRefill::OverTime ? FMath::Clamp(Track.RefillFrames, 1, 65535) : 0);
	const bool bHadCharge = Slot.Spent < Slot.Charges;
	Slot.Spent = static_cast<uint8>(FMath::Min(static_cast<int32>(Slot.Spent) + FMath::Max(Track.Cost, 1), static_cast<int32>(Slot.Charges)));
	if (Slot.Refill == EGSTN_BackpackRefill::OverTime && Slot.RefillFromFrame == INDEX_NONE)
	{
		Slot.RefillFromFrame = Frame;
	}
	return bHadCharge;
}

void FGSTN_BackpackStep::RaiseDome(FGSTN_BackpackFragment& Fragment, int32 Frame, const FGSTN_BackpackDomeTrack& Track, const FVector& Center, int32 TimelineIndex, int32 TimelineFrames)
{
	Fragment.DomeStartFrame = Frame;
	Fragment.DomeLifetime = Track.Lifetime;
	Fragment.DomeRadiusCm = static_cast<uint16>(FMath::Clamp(Track.RadiusCm, 1, 65535));
	Fragment.DomeTimeline = static_cast<uint16>(FMath::Clamp(TimelineIndex, 0, 65535));
	const int32 Length = Track.Lifetime == EGSTN_DomeLifetime::Deploy ? Track.DurationFrames : TimelineFrames;
	Fragment.DomeUntilFrame = Length > 0 ? Frame + Length : INDEX_NONE;
	Fragment.DomeCenter = FIntVector(FMath::RoundToInt32(Center.X), FMath::RoundToInt32(Center.Y), FMath::RoundToInt32(Center.Z));
	Fragment.DomePushSpeedQ = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Track.PushbackSpeed / FGSTN_BackpackDomeTrack::PushSpeedStep), 0, 255));
	Fragment.DomePushFrames = static_cast<uint8>(FMath::Clamp(Track.PushbackFrames, 1, 127));
	Fragment.DomePushDirection = Track.PushbackDirection;
}

void FGSTN_BackpackStep::GatherStateTags(const FGSTN_BackpackFragment* Fragment, FGameplayTagContainer& OutTags)
{
	if (!Fragment)
	{
		return;
	}
	for (int32 Index = 0; Index < GSTNBackpackMemory::NumSlots; ++Index)
	{
		if (Fragment->Memory[Index].IsEmpty())
		{
			OutTags.AddTag(GSTNBackpackTags::EmptySlotTag(Index));
		}
	}
	if (Fragment->HasDome())
	{
		OutTags.AddTag(GSTNBackpackTags::State_Dome.GetTag());
	}
}

const FGameplayTagContainer& FGSTN_BackpackStep::GetProjectableTags()
{
	static const FGameplayTagContainer Tags = []()
	{
		FGameplayTagContainer Result;
		for (int32 Index = 0; Index < GSTNBackpackMemory::NumSlots; ++Index)
		{
			Result.AddTag(GSTNBackpackTags::EmptySlotTag(Index));
		}
		Result.AddTag(GSTNBackpackTags::State_Dome.GetTag());
		return Result;
	}();
	return Tags;
}

int32 FGSTN_BackpackStep::ChargesLeft(const FGSTN_BackpackFragment& Fragment, int32 Slot)
{
	if (Slot < 0 || Slot >= GSTNBackpackMemory::NumSlots || Fragment.Memory[Slot].Charges == 0)
	{
		return INDEX_NONE;
	}
	const FGSTN_BackpackMemorySlot& Memory = Fragment.Memory[Slot];
	return FMath::Max(static_cast<int32>(Memory.Charges) - static_cast<int32>(Memory.Spent), 0);
}
